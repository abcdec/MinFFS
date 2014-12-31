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

#ifndef DLL_IFILEDIALOG_VISTA_IFILE_DIALOG_H_INCLUDED
#define DLL_IFILEDIALOG_VISTA_IFILE_DIALOG_H_INCLUDED
// The original FreeFileSync source distribution does not come with this
// file, although it appeared this file is required to build properly.
// Thus it was recreated from its usage context.

#include <string>

namespace ifile {

    typedef char GuidProxy[16];

    // freeString should actually be void, but making it to be the sam as
    // zen/IFileOperation/file_op.h : getLockingProcesses in order to make
    // funciton object work. keep this until a better solution is found...
    typedef wchar_t *(*FunType_freeString)(const wchar_t *freeStringPtr);
    typedef void (*FunType_showFolderPicker)(HWND winHandleIn,
					     const wchar_t* defaultDirPathIn,
					     const GuidProxy* guidIn,
					     wchar_t* &selectedFolderOut,
					     bool &cancelledOut,
					     wchar_t* &errorMsgOut);

    const std::string funName_freeString = "freeString";
    const std::string funName_showFolderPicker = "showFolderPicker";

    inline std::wstring getDllName() {
#ifdef TODO_MinFFS_REAL_DLL_NAME
	return L"IFileDialog_Vista_Win32.dll";
	//return L"IFileDialog_Vista_x64.dll";
#else//TODO_MinFFS_REAL_DLL_NAME
	return L"Dummy_IFileDialog_Vista.dll";
#endif//TODO_MinFFS_REAL_DLL_NAME
    };

}

#endif//DLL_IFILEDIALOG_VISTA_IFILE_DIALOG_H_INCLUDED
