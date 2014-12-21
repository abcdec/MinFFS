// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef TRAYICON_H_84217830427534285
#define TRAYICON_H_84217830427534285

#include <functional>
#include <wx/image.h>

/*
show tray icon with progress during lifetime of this instance

ATTENTION: wxWidgets never assumes that an object indirectly destroys itself while processing an event!
           this includes wxEvtHandler-derived objects!!!
           it seems ProcessEvent() works (on Windows), but AddPendingEvent() will crash since it uses "this" after the event processing!

=> don't derive from wxEvtHandler or any other wxWidgets object here!!!!!!
=> use simple std::function as callback instead => instance may now be safely deleted in callback!
*/

class FfsTrayIcon
{
public:
    FfsTrayIcon(const std::function<void()>& onRequestResume); //callback only held during lifetime of this instance
    ~FfsTrayIcon();

    void setToolTip(const wxString& toolTip);
    void setProgress(double fraction); //number between [0, 1], for small progress indicator

private:
    FfsTrayIcon           (const FfsTrayIcon&) = delete;
    FfsTrayIcon& operator=(const FfsTrayIcon&) = delete;

    class TaskBarImpl;
    TaskBarImpl* trayIcon;

    wxString activeToolTip;
    double activeFraction;
    wxImage logo;
};

#endif //TRAYICON_H_84217830427534285
