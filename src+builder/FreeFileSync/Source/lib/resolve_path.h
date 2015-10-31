// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef RESOLVE_PATH_H_INCLUDED
#define RESOLVE_PATH_H_INCLUDED

#include <vector>
#include <zen/zstring.h>

namespace zen
{
/*
FULL directory format:
	- expand macros
	- expand volume path by name
	- convert relative paths into absolute
	- trim whitespace and append file name separator
*/
Zstring getFormattedDirectoryPath(const Zstring& dirpathPhrase); //noexcept; may still block for slow USB sticks! not thread-safe!!! see ::GetFullPathName()

//macro substitution only
Zstring expandMacros(const Zstring& text);

std::vector<Zstring> getDirectoryAliases(const Zstring& dirpassPhrase); //may block for slow USB sticks when resolving [<volume name>]

#ifdef ZEN_WIN
    //*blocks* if network is not reachable or when showing login prompt dialog!
    void loginNetworkShare(const Zstring& dirpath, bool allowUserInteraction); //noexcept; user interaction: show OS password prompt
#endif
}


#endif // RESOLVE_PATH_H_INCLUDED
