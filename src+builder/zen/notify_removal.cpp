// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "notify_removal.h"
#include <set>
#include <algorithm>
#include <dbt.h>
#include "scope_guard.h"

using namespace zen;


class MessageProvider //administrates a single dummy window to receive messages
{
public:
    static MessageProvider& instance() //throw FileError
    {
        static MessageProvider inst;
        return inst;
    }

    struct Listener
    {
        virtual ~Listener() {}
        virtual void onMessage(UINT message, WPARAM wParam, LPARAM lParam) = 0; //throw()!
    };
    void   registerListener(Listener& l) { listener.insert(&l); }
    void unregisterListener(Listener& l) { listener.erase(&l); } //don't unregister objects with static lifetime

    HWND getWnd() const { return windowHandle; } //get handle in order to register additional notifications

private:
    MessageProvider();
    ~MessageProvider();
    MessageProvider           (const MessageProvider&) = delete;
    MessageProvider& operator=(const MessageProvider&) = delete;

    static const wchar_t dummyClassName[];

    static LRESULT CALLBACK topWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void processMessage(UINT message, WPARAM wParam, LPARAM lParam);

    const HINSTANCE hMainModule;
    HWND windowHandle;

    std::set<Listener*> listener;
};


const wchar_t MessageProvider::dummyClassName[] = L"E6AD5EB1-527B-4EEF-AC75-27883B233380"; //random name


LRESULT CALLBACK MessageProvider::topWndProc(HWND hwnd,     //handle to window
                                             UINT uMsg,     //message identifier
                                             WPARAM wParam, //first message parameter
                                             LPARAM lParam) //second message parameter
{
    if (auto pThis = reinterpret_cast<MessageProvider*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))
        try
        {
            pThis->processMessage(uMsg, wParam, lParam); //not supposed to throw!
        }
        catch (...) { assert(false); }

    return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}


MessageProvider::MessageProvider() :
    hMainModule(::GetModuleHandle(nullptr)), //get program's module handle
    windowHandle(nullptr)
{
    if (!hMainModule)
        throwFileError(_("Unable to register to receive system messages."), L"GetModuleHandle", getLastError());

    //register the main window class
    WNDCLASS wc = {};
    wc.lpfnWndProc   = topWndProc;
    wc.hInstance     = hMainModule;
    wc.lpszClassName = dummyClassName;

    if (::RegisterClass(&wc) == 0)
        throwFileError(_("Unable to register to receive system messages."), L"RegisterClass", getLastError());

    ScopeGuard guardConstructor = zen::makeGuard([&] { this->~MessageProvider(); });

    //create dummy-window
    windowHandle = ::CreateWindow(dummyClassName, //_In_opt_  LPCTSTR lpClassName,
                                  nullptr,        //_In_opt_  LPCTSTR lpWindowName,
                                  0,           //_In_      DWORD dwStyle,
                                  0, 0, 0, 0,  //_In_      int x, y, nWidth, nHeight,
                                  nullptr,     //_In_opt_  HWND hWndParent; we need a toplevel window to receive device arrival events, not a message-window (HWND_MESSAGE)!
                                  nullptr,     //_In_opt_  HMENU hMenu,
                                  hMainModule, //_In_opt_  HINSTANCE hInstance,
                                  nullptr);    //_In_opt_  LPVOID lpParam
    if (!windowHandle)
        throwFileError(_("Unable to register to receive system messages."), L"CreateWindow", getLastError());

    //store this-pointer for topWndProc() to use: do this AFTER CreateWindow() to avoid processing messages while this constructor is running!!!
    //unlike: http://blogs.msdn.com/b/oldnewthing/archive/2014/02/03/10496248.aspx
    ::SetLastError(ERROR_SUCCESS); //[!] required for proper error handling, see MSDN, SetWindowLongPtr
    if (::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)) == 0 && ::GetLastError() != ERROR_SUCCESS)
        throwFileError(_("Unable to register to receive system messages."), L"SetWindowLongPtr", ::GetLastError());

    guardConstructor.dismiss();
}


MessageProvider::~MessageProvider()
{
    //clean-up in reverse order
    if (windowHandle)
        ::DestroyWindow(windowHandle);
    ::UnregisterClass(dummyClassName, //LPCTSTR lpClassName OR ATOM in low-order word!
                      hMainModule);   //HINSTANCE hInstance
}


void MessageProvider::processMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    for (Listener* ls : listener)
        ls->onMessage(message, wParam, lParam);
}

//####################################################################################################

class NotifyRequestDeviceRemoval::Pimpl : private MessageProvider::Listener
{
public:
    Pimpl(NotifyRequestDeviceRemoval& parent, HANDLE hDir) : //throw FileError
        parent_(parent)
    {
        MessageProvider::instance().registerListener(*this); //throw FileError

        ScopeGuard guardProvider = makeGuard([&] { MessageProvider::instance().unregisterListener(*this); });

        //register handles to receive notifications
        DEV_BROADCAST_HANDLE filter = {};
        filter.dbch_size = sizeof(filter);
        filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
        filter.dbch_handle = hDir;

        hNotification = ::RegisterDeviceNotification(MessageProvider::instance().getWnd(), //__in  HANDLE hRecipient,
                                                     &filter,                              //__in  LPVOID NotificationFilter,
                                                     DEVICE_NOTIFY_WINDOW_HANDLE);         //__in  DWORD Flags
        if (!hNotification)
        {
            const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
            if (lastError != ERROR_CALL_NOT_IMPLEMENTED   && //fail on SAMBA share: this shouldn't be a showstopper!
                lastError != ERROR_SERVICE_SPECIFIC_ERROR && //neither should be fail for "Pogoplug" mapped network drives
                lastError != ERROR_INVALID_DATA)             //this seems to happen for a NetDrive-mapped FTP server
                throwFileError(_("Unable to register to receive system messages."), L"RegisterDeviceNotification", lastError);
        }

        guardProvider.dismiss();
    }

    ~Pimpl()
    {
        if (hNotification)
            ::UnregisterDeviceNotification(hNotification);
        MessageProvider::instance().unregisterListener(*this);
    }

private:
    Pimpl           (const Pimpl&) = delete;
    Pimpl& operator=(const Pimpl&) = delete;

    void onMessage(UINT message, WPARAM wParam, LPARAM lParam) override //throw()!
    {
        //DBT_DEVICEQUERYREMOVE example: http://msdn.microsoft.com/en-us/library/aa363427(v=VS.85).aspx
        if (message == WM_DEVICECHANGE)
            if (wParam == DBT_DEVICEQUERYREMOVE       ||
                wParam == DBT_DEVICEQUERYREMOVEFAILED ||
                wParam == DBT_DEVICEREMOVECOMPLETE)
            {
                PDEV_BROADCAST_HDR header = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
                if (header->dbch_devicetype == DBT_DEVTYP_HANDLE)
                {
                    PDEV_BROADCAST_HANDLE body = reinterpret_cast<PDEV_BROADCAST_HANDLE>(lParam);

                    if (body->dbch_hdevnotify == hNotification) //is it for the notification we registered?
                        switch (wParam)
                        {
                            case DBT_DEVICEQUERYREMOVE:
                                parent_.onRequestRemoval(body->dbch_handle);
                                break;
                            case DBT_DEVICEQUERYREMOVEFAILED:
                                parent_.onRemovalFinished(body->dbch_handle, false);
                                break;
                            case DBT_DEVICEREMOVECOMPLETE:
                                parent_.onRemovalFinished(body->dbch_handle, true);
                                break;
                        }
                }
            }
    }

    NotifyRequestDeviceRemoval& parent_;
    HDEVNOTIFY hNotification;
};

//####################################################################################################

NotifyRequestDeviceRemoval::NotifyRequestDeviceRemoval(HANDLE hDir) : pimpl(zen::make_unique<Pimpl>(*this, hDir)) {}


NotifyRequestDeviceRemoval::~NotifyRequestDeviceRemoval() {} //make sure ~unique_ptr() works with complete type
