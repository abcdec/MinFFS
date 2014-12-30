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

#ifndef DLL_TASKBAR_SEVEN_TASKBAR_H_INCLUDED
#define DLL_TASKBAR_SEVEN_TASKBAR_H_INCLUDED
// The original FreeFileSync source distribution does not come with this
// file, although it appeared this file is required to build properly.
// Thus it was recreated from its usage context.

#include <string>
#include "zen/dll.h"

namespace tbseven
{
    enum TaskBarStatus {
        STATUS_INDETERMINATE,
	STATUS_NORMAL,
	STATUS_ERROR,
	STATUS_PAUSED,
	STATUS_NOPROGRESS
    };

    typedef void (*FunType_setStatus)(void *, TaskBarStatus);
    typedef void (*FunType_setProgress)(void *, double, double);

    const std::string funName_setStatus = "SetStatus";
    const std::string funName_setProgress = "SetProgress";

    inline std::wstring getDllName() {
#ifdef TODO_MinFFS
	return L"Taskbar7_win32.dll";
	//return L"Taskbar7_x64.dll";
#else//TODO_MinFFS
	return L"Dummy_Taskbar7.dll";
#endif//TODO_MinFFS
    };
}

#endif//DLL_TASKBAR_SEVEN_TASKBAR_H_INCLUDED
