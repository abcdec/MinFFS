// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef BUILDINFO_H_INCLUDED
#define BUILDINFO_H_INCLUDED

namespace zen
{
//determine build info
//safer than checking for _WIN64 (defined on windows for 64-bit compilations only) while _WIN32 is always defined (even for x64 compiler!)
static const bool is32BitBuild = sizeof(void*) == 4;
static const bool is64BitBuild = sizeof(void*) == 8;

static_assert(is32BitBuild || is64BitBuild, "");
}

#endif //BUILDINFO_H_INCLUDED
