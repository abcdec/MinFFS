// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "application.h"
#include "main_dlg.h"
#include <zen/file_access.h>
#include <zen/thread.h>
#include <wx/event.h>
#include <wx/log.h>
#include <wx/tooltip.h>
#include <wx+/string_conv.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "xml_proc.h"
#include "../lib/localization.h"
#include "../lib/ffs_paths.h"
#include "../lib/return_codes.h"
#include "../lib/error_log.h"

#ifdef ZEN_WIN
    #include <zen/win_ver.h>
    #include <zen/dll.h>
    #include "../lib/app_user_mode_id.h"

#elif defined ZEN_LINUX
    #include <gtk/gtk.h>
#endif

using namespace zen;


IMPLEMENT_APP(Application);

namespace
{
/*
boost::thread::id mainThreadId = boost::this_thread::get_id();

void onTerminationRequested()
{
std::wstring msg = boost::this_thread::get_id() == mainThreadId ?
                   L"Termination requested in main thread!\n\n" :
                   L"Termination requested in worker thread!\n\n";
msg += L"Please file a bug report at: http://sourceforge.net/projects/freefilesync";

wxSafeShowMessage(_("An exception occurred"), msg);
std::abort();
}
*/
#ifdef _MSC_VER
void crtInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved) { assert(false); }
#endif

const wxEventType EVENT_ENTER_EVENT_LOOP = wxNewEventType();
}


bool Application::OnInit()
{
    //std::set_terminate(onTerminationRequested); //unlike wxWidgets uncaught exception handling, this works for all worker threads

#ifdef ZEN_WIN
#ifdef _MSC_VER
    _set_invalid_parameter_handler(crtInvalidParameterHandler); //see comment in <zen/time.h>
#endif
    //Quote: "Best practice is that all applications call the process-wide SetErrorMode function with a parameter of
    //SEM_FAILCRITICALERRORS at startup. This is to prevent error mode dialogs from hanging the application."
    ::SetErrorMode(SEM_FAILCRITICALERRORS);

    setAppUserModeId(L"RealtimeSync", L"Zenju.RealtimeSync"); //noexcept
    //consider: RealtimeSync.exe, RealtimeSync_Win32.exe, RealtimeSync_x64.exe

    wxToolTip::SetMaxWidth(-1); //disable tooltip wrapping -> Windows only

#elif defined ZEN_LINUX
    ::gtk_rc_parse((zen::getResourceDir() + "styles.gtk_rc").c_str()); //remove inner border from bitmap buttons
#endif

    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::SetAutoPop(7000); //http://msdn.microsoft.com/en-us/library/windows/desktop/aa511495.aspx

    SetAppName(L"RealtimeSync");

    initResourceImages(getResourceDir() + Zstr("Resources.zip"));

    Connect(wxEVT_QUERY_END_SESSION, wxEventHandler(Application::onQueryEndSession), nullptr, this);
    Connect(wxEVT_END_SESSION,       wxEventHandler(Application::onQueryEndSession), nullptr, this);

    //do not call wxApp::OnInit() to avoid using default commandline parser

    //Note: app start is deferred:  -> see FreeFileSync
    Connect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);
    wxCommandEvent scrollEvent(EVENT_ENTER_EVENT_LOOP);
    AddPendingEvent(scrollEvent);

    return true; //true: continue processing; false: exit immediately.
}


int Application::OnExit()
{
    releaseWxLocale();
    return wxApp::OnExit();
}


void Application::onEnterEventLoop(wxEvent& event)
{
    Disconnect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);

    try
    {
        int lid = xmlAccess::getProgramLanguage();
        setLanguage(lid); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        //continue!
    }

    //try to set config/batch- filepath set by %1 parameter
    std::vector<Zstring> commandArgs;
    for (int i = 1; i < argc; ++i)
    {
        Zstring filepath = toZ(argv[i]);

        if (!fileExists(filepath)) //be a little tolerant
        {
            if (fileExists(filepath + Zstr(".ffs_real")))
                filepath += Zstr(".ffs_real");
            else if (fileExists(filepath + Zstr(".ffs_batch")))
                filepath += Zstr(".ffs_batch");
            else
            {
                showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(replaceCpy(_("Cannot find file %x."), L"%x", fmtFileName(filepath))));
                return;
            }
        }
        commandArgs.push_back(filepath);
    }

    Zstring cfgFilename;
    if (!commandArgs.empty())
        cfgFilename = commandArgs[0];

    MainDialog::create(cfgFilename);
}


int Application::OnRun()
{

    auto processException = [](const std::wstring& msg)
    {
        //it's not always possible to display a message box, e.g. corrupted stack, however low-level file output works!
        logError(utfCvrtTo<std::string>(msg));
        wxSafeShowMessage(_("An exception occurred"), msg);
    };

    try
    {
        wxApp::OnRun();
    }
    catch (const std::exception& e) //catch all STL exceptions
    {
        processException(utfCvrtTo<std::wstring>(e.what()));
        return FFS_RC_EXCEPTION;
    }
    catch (...) //catch the rest
    {
        processException(L"Unknown error.");
        return FFS_RC_EXCEPTION;
    }

    return FFS_RC_SUCCESS; //program's return code
}



void Application::onQueryEndSession(wxEvent& event)
{
    if (auto mainWin = dynamic_cast<MainDialog*>(GetTopWindow()))
        mainWin->onQueryEndSession();
    OnExit(); //wxWidgets screws up again: http://trac.wxwidgets.org/ticket/3069
    //wxEntryCleanup(); -> gives popup "dll init failed" on XP
    std::exit(FFS_RC_SUCCESS); //Windows will terminate anyway: destruct global objects
}
