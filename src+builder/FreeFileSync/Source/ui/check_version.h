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
void checkForUpdateNow(wxWindow* parent);
void checkForUpdatePeriodically(wxWindow* parent, long& lastUpdateCheck, const std::function<void()>& onBeforeInternetAccess); //-1: check never
}

#endif // UPDATEVERSION_H_INCLUDED
