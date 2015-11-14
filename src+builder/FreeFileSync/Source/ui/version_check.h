// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef VERSION_CHECK_H_324872374893274983275
#define VERSION_CHECK_H_324872374893274983275

#include <functional>
#include <memory>
#include <wx/window.h>


namespace zen
{
bool updateCheckActive(time_t lastUpdateCheck);
void disableUpdateCheck(time_t& lastUpdateCheck);
bool haveNewerVersionOnline(const std::wstring& onlineVersion);

//periodic update check:
bool shouldRunPeriodicUpdateCheck(time_t lastUpdateCheck);

//long-runing part of the check: thread-safe => run asynchronously
struct UpdateCheckResult;
std::shared_ptr<UpdateCheckResult> retrieveOnlineVersion();

//eval on main thread:
void evalPeriodicUpdateCheck(wxWindow* parent, time_t& lastUpdateCheck, std::wstring& lastOnlineVersion, const UpdateCheckResult* result);

//call from main thread:
void checkForUpdateNow(wxWindow* parent, std::wstring& lastOnlineVersion);
}

#endif //VERSION_CHECK_H_324872374893274983275
