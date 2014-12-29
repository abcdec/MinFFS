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
#ifndef DLL_SHADOW_COPY_H_INCLUDED
#define DLL_SHADOW_COPY_H_INCLUDED
// The original FreeFileSync source distribution does not come with this
// file, although it appeared this file is required to build properly.
// Thus it was recreated from its usage context.


namespace shadowCopy {
    typedef void (*FunType_createShadowCopy)();
    typedef void (*FunType_getLastErrorMessage)();
    typedef void (*FunType_getShadowVolume)();
    typedef void (*FunType_releaseShadowCopy)();

    const std::string funName_createShadowCopy = "createShadowCopy";
    const std::string funName_getLastErrorMessage = "getLastErrorMessage";
    const std::string funName_getShadowVolume = "getShadowVolume";
    const std::string funName_releaseShadowCopy = "releaseShadowCopy";

    inline std::wstring getDllName() {
	//return L"Shadow_Server2003_Win32.dll";
	//return L"Shadow_Server2003_x64.dll";
	return L"Shadow_Windows7_Win32.dll";
	//return L"Shadow_Windows7_x64.dll";
	//return L"Shadow_XP_Win32.dll";
	//return L"Shadow_XP_x64.dll";
    };
}

#endif//DLL_SHADOW_COPY_H_INCLUDED
