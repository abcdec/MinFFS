// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "taskbar.h"

#ifdef ZEN_WIN
    #include <zen/dll.h>
    #include <zen/win_ver.h>
    #include <zen/stl_tools.h>
    #include "../dll/Taskbar_Seven/taskbar.h"

#elif defined HAVE_UBUNTU_UNITY
    #include <unity/unity/unity.h>

#elif defined ZEN_MAC
    #include <zen/basic_math.h>
    #include <zen/string_tools.h>
    #include "osx_dock.h"
#endif

using namespace zen;


#ifdef ZEN_WIN
using namespace tbseven;


class Taskbar::Pimpl //throw TaskbarNotAvailable
{
public:
    Pimpl(const wxFrame& window) : assocWindow(window.GetHWND())
    {
        if (!win7OrLater()) //check *before* trying to load DLL
            throw TaskbarNotAvailable();

        setStatus_   = DllFun<FunType_setStatus  >(getDllName(), funName_setStatus);
        setProgress_ = DllFun<FunType_setProgress>(getDllName(), funName_setProgress);

        if (!assocWindow || !setStatus_ || !setProgress_)
            throw TaskbarNotAvailable();
    }

    ~Pimpl() { setStatus_(assocWindow, tbseven::STATUS_NOPROGRESS); }

    void setStatus(Status status)
    {
        TaskBarStatus tbSevenStatus = tbseven::STATUS_NORMAL;
        switch (status)
        {
            case Taskbar::STATUS_INDETERMINATE:
                tbSevenStatus = tbseven::STATUS_INDETERMINATE;
                break;
            case Taskbar::STATUS_NORMAL:
                tbSevenStatus = tbseven::STATUS_NORMAL;
                break;
            case Taskbar::STATUS_ERROR:
                tbSevenStatus = tbseven::STATUS_ERROR;
                break;
            case Taskbar::STATUS_PAUSED:
                tbSevenStatus = tbseven::STATUS_PAUSED;
                break;
        }

        setStatus_(assocWindow, tbSevenStatus);
    }

    void setProgress(double fraction)
    {
        setProgress_(assocWindow, fraction * 100000, 100000);
    }

private:
    void* const assocWindow; //HWND
    DllFun<FunType_setStatus>   setStatus_;
    DllFun<FunType_setProgress> setProgress_;
};

#elif defined HAVE_UBUNTU_UNITY //Ubuntu unity
namespace
{
const char FFS_DESKTOP_FILE[] = "freefilesync.desktop";
}

class Taskbar::Pimpl //throw (TaskbarNotAvailable)
{
public:
    Pimpl(const wxFrame& window) :
        tbEntry(unity_launcher_entry_get_for_desktop_id(FFS_DESKTOP_FILE))
        //tbEntry(unity_launcher_entry_get_for_app_uri("application://freefilesync.desktop"))
    {
        if (!tbEntry)
            throw TaskbarNotAvailable();
    }

    ~Pimpl() { setStatus(STATUS_INDETERMINATE); } //it seems UnityLauncherEntry* does not need destruction

    void setStatus(Status status)
    {
        switch (status)
        {
            case Taskbar::STATUS_ERROR:
                unity_launcher_entry_set_urgent(tbEntry, true);
                break;

            case Taskbar::STATUS_INDETERMINATE:
                unity_launcher_entry_set_urgent(tbEntry, false);
                unity_launcher_entry_set_progress_visible(tbEntry, false);
                break;

            case Taskbar::STATUS_NORMAL:
                unity_launcher_entry_set_urgent(tbEntry, false);
                unity_launcher_entry_set_progress_visible(tbEntry, true);
                break;

            case Taskbar::STATUS_PAUSED:
                unity_launcher_entry_set_urgent(tbEntry, false);
                break;
        }
    }

    void setProgress(double fraction)
    {
        unity_launcher_entry_set_progress(tbEntry, fraction);
    }

private:
    UnityLauncherEntry* tbEntry;
};

#elif defined ZEN_MAC
class Taskbar::Pimpl
{
public:
    Pimpl(const wxFrame& window) {}

    ~Pimpl() { setDockText(""); }

    void setStatus(Status status) {}

    void setProgress(double fraction)
    {
        //no decimal places to make output less noisy
        setDockText((numberTo<std::string>(numeric::round(fraction * 100.0)) + '%').c_str()); //no need to internationalize fraction!?
    }

private:
    void setDockText(const char* str)
    {
        try
        {
            osx::dockIconSetText(str); //throw SysError
        }
        catch (const zen::SysError& e) { assert(false); }
    }
};


#else //no taskbar support
class Taskbar::Pimpl
{
public:
    Pimpl(const wxFrame& window) { throw TaskbarNotAvailable(); }
    void setStatus(Status status) {}
    void setProgress(double fraction) {}
};
#endif

//########################################################################################################

Taskbar::Taskbar(const wxFrame& window) : pimpl_(zen::make_unique<Pimpl>(window)) {} //throw TaskbarNotAvailable
Taskbar::~Taskbar() {}

void Taskbar::setStatus(Status status) { pimpl_->setStatus(status); }
void Taskbar::setProgress(double fraction) { pimpl_->setProgress(fraction); }
