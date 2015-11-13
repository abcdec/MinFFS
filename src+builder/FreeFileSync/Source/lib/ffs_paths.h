// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FFS_PATHS_H_842759083425342534253
#define FFS_PATHS_H_842759083425342534253

#include <zen/zstring.h>

namespace zen
{
//------------------------------------------------------------------------------
//global program directories
//------------------------------------------------------------------------------
Zstring getResourceDir(); //resource directory WITH path separator at end
Zstring getConfigDir  (); //config directory WITH path separator at end
//------------------------------------------------------------------------------

Zstring getFreeFileSyncLauncherPath(); //full path to application launcher C:\...\FreeFileSync.exe
bool manualProgramUpdateRequired();
}

#endif //FFS_PATHS_H_842759083425342534253
