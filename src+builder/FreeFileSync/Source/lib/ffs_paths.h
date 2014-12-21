// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef STANDARDPATHS_H_84275908342534253425
#define STANDARDPATHS_H_84275908342534253425

#include <zen/zstring.h>

namespace zen
{
//------------------------------------------------------------------------------
//global program directories
//------------------------------------------------------------------------------
Zstring getResourceDir(); //resource directory WITH path separator at end
Zstring getConfigDir  (); //config directory WITH path separator at end
//------------------------------------------------------------------------------

Zstring getFreeFileSyncLauncher(); //full path to application launcher C:\...\FreeFileSync.exe
bool manualProgramUpdateRequired();
}

#endif //STANDARDPATHS_H_84275908342534253425
