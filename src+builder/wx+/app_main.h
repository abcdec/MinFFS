// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef APPMAIN_H_INCLUDED
#define APPMAIN_H_INCLUDED

#include <wx/window.h>
#include <wx/app.h>

namespace zen
{
//just some wrapper around a global variable representing the (logical) main application window
void setMainWindow(wxWindow* window); //set main window and enable "exit on frame delete"
bool mainWindowWasSet();












//######################## implementation ########################
inline
bool& refMainWndStatus()
{
    static bool status = false; //external linkage!
    return status;
}

inline
void setMainWindow(wxWindow* window)
{
    wxTheApp->SetTopWindow(window);
    wxTheApp->SetExitOnFrameDelete(true);

    refMainWndStatus() = true;
}

inline bool mainWindowWasSet() { return refMainWndStatus(); }
}

#endif // APPMAIN_H_INCLUDED
