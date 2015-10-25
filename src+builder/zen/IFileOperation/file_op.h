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

#ifndef DLL_IFILEOPERATION_FILE_OP_H_INCLUDED
#define DLL_IFILEOPERATION_FILE_OP_H_INCLUDED
// The original FreeFileSync source distribution does not come with this
// file, although it appeared this file is required to build properly.
// Thus it was recreated from its usage context.

namespace fileop
{
    // freeString should actually be void, but making it to be the sam as
    // getLockingProcesses in order to make funciton object work. keep
    // this until a better solution is found...
    typedef wchar_t* (*FunType_freeString)(const wchar_t*);
    typedef wchar_t* (*FunType_getLastErrorMessage)(void);
    //typedef wchar_t* (*FunType_getLockingProcesses)(const wchar_t*);
    typedef wchar_t* (*FunType_getLockingProcesses)(const wchar_t*, const wchar_t*);
    typedef bool (*FunType_getRecycleBinStatus)(const wchar_t *, bool&);
    typedef bool (*FunType_moveToRecycleBin)(const wchar_t**,
					     std::vector<const wchar_t*>::size_type,
					     bool (&)(const wchar_t*, void *),
					     void *);
    
    const std::string funName_freeString = "freeString";
    const std::string funName_getLastErrorMessage = "getLastErrorMessage";
    const std::string funName_getLockingProcesses = "getLockingProcesses";
    const std::string funName_getRecycleBinStatus = "getRecycleBinStatus";
    const std::string funName_moveToRecycleBin = "moveToRecycleBin";
	

    inline std::wstring getDllName() {
#ifdef TODO_MinFFS_REAL_DLL_NAME
	return L"FileOperation_Win32.dll";	
	//return L"FileOperation_x64.dll";
#else//TODO_MinFFS_REAL_DLL_NAME
	return L"Dummy_FileOperation.dll";
#endif//TODO_MinFFS_REAL_DLL_NAME
    };
}

#endif//DLL_IFILEOPERATION_FILE_OP_H_INCLUDED
