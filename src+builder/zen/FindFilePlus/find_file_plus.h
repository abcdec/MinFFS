// **************************************************************************
// * Copyright (C) 2014 abcdec @GitHub - All Rights Reserved                *
// * This file is part of a modified version of FreeFileSync, MinFFS.       *
// * The original FreeFileSync program and source code are distributed by   *
// * the FreeFileSync project: http://www.freefilesync.org/                 *
// * This particular file is created by by abcdec @GitHub as part of the    *
// * MinFFS project: https://github.com/abcdec/MinFFS                       *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This file is distributed under GNU General Public License:             *
// * http://www.gnu.org/licenses/gpl-3.0 per the FreeFileSync License.      *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#ifndef DLL_FINDFILEPLUS_FILE_OPFIND_FILE_PLUS_H_INCLUDED
#define DLL_FINDFILEPLUS_FILE_OPFIND_FILE_PLUS_H_INCLUDED
// The original FreeFileSync source distribution does not come with this
// file, although it appeared this file is required to build properly.
// Thus it was recreated from its usage context.

#include "zen/file_id_def.h"

namespace findplus
{
    struct FindHandle {
    public:
	FindHandle();
	~FindHandle();
    };
    struct DirHandle {
    public:
	DirHandle();
	~DirHandle();
    };
    struct FileInformation {
    public:
	FileInformation();
	~FileInformation();

        std::uint64_t fileSize;
        FILETIME lastWriteTime;
        FILETIME creationTime;
        wchar_t *shortName;
	DWORD fileAttributes;
	std::int16_t fileId;

        FILETIME ftLastWriteTime;
        FILETIME ftCreationTime;
	std::wstring cFileName;
	DWORD dwFileAttributes;
	DWORD reparseTag;
    };

#ifdef TODO_MinFFS
    typedef FindHandle (*FunType_openDir)(TODO);
    typedef FindHandle (*FunType_readDir)(TODO);
    typedef FindHandle (*FunType_closeDir)(TODO);

    const std::string funName_openDir = "openDir";
    const std::string funName_readDir = "readDir";
    const std::string funName_closeDir = "closeDir";

//TODO_MinFFS need real DLL name
    inline std::wstring getDllName() {
	return L"TODO.dll";
    };
#endif
}

#endif//DLL_FINDFILEPLUS_FILE_OPFIND_FILE_PLUS_H_INCLUDED
