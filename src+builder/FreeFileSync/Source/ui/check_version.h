// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef UPDATEVERSION_H_INCLUDED
#define UPDATEVERSION_H_INCLUDED

#include <functional>
#include <wx/window.h>


namespace zen
{
void checkForUpdateNow(wxWindow* parent, wxString& lastOnlineVersion);
void checkForUpdatePeriodically(wxWindow* parent, long& lastUpdateCheck, wxString& lastOnlineVersion, const std::function<void()>& onBeforeInternetAccess); //-1: check never

bool updateCheckActive(long lastUpdateCheck);
void disableUpdateCheck(long& lastUpdateCheck);

bool isNewerFreeFileSyncVersion(const wxString& onlineVersion);
}

#endif // UPDATEVERSION_H_INCLUDED
