// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef RECYCLER_H_INCLUDED_18345067341545
#define RECYCLER_H_INCLUDED_18345067341545

#include <vector>
#include <functional>
#include "file_error.h"

namespace zen
{
/*
--------------------
|Recycle Bin Access|
--------------------

Windows
-------
Recycler API always available: during runtime either SHFileOperation or IFileOperation (since Vista) will be dynamically selected

Linux
-----
Compiler flags: `pkg-config --cflags gio-2.0`
Linker   flags: `pkg-config --libs gio-2.0`

Already included in package "gtk+-2.0"!
*/

//move a file or folder to Recycle Bin (deletes permanently if recycler is not available) -> crappy semantics, but we have no choice thanks to Windows' design
bool recycleOrDelete(const Zstring& itempath); //throw FileError, return "true" if file/dir was actually deleted


#ifdef ZEN_WIN
//can take a long time if recycle bin is full and drive is slow!!! => buffer volume ids!
bool recycleBinExists(const Zstring& pathName, const std::function<void ()>& onUpdateGui); //throw FileError

void recycleOrDelete(const std::vector<Zstring>& filepaths, //throw FileError, return "true" if file/dir was actually deleted
                     const std::function<void (const Zstring& currentItem)>& notifyDeletionStatus); //optional; currentItem may be empty
#endif
}

#endif //RECYCLER_H_INCLUDED_18345067341545
