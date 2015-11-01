// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 7.5         *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#ifndef WINDOWS_VERSION_HEADER_238470348254325
#define WINDOWS_VERSION_HEADER_238470348254325

#include <cassert>
#include <utility>
#include "win.h" //includes "windows.h"
#include "build_info.h"
#include "dll.h"

namespace zen
{
struct OsVersion
{
    OsVersion() : major(), minor() {}
    OsVersion(DWORD high, DWORD low) : major(high), minor(low) {}

    DWORD major;
    DWORD minor;
};
inline bool operator< (const OsVersion& lhs, const OsVersion& rhs) { return lhs.major != rhs.major ? lhs.major < rhs.major : lhs.minor < rhs.minor; }
inline bool operator==(const OsVersion& lhs, const OsVersion& rhs) { return lhs.major == rhs.major && lhs.minor == rhs.minor; }


//version overview: http://msdn.microsoft.com/en-us/library/ms724834(VS.85).aspx
const OsVersion osVersionWin10       (10, 0);
const OsVersion osVersionWin81        (6, 3);
const OsVersion osVersionWin8         (6, 2);
const OsVersion osVersionWin7         (6, 1);
const OsVersion osVersionWinVista     (6, 0);
const OsVersion osVersionWinServer2003(5, 2);
const OsVersion osVersionWinXp        (5, 1);
const OsVersion osVersionWin2000      (5, 0);

/*
	NOTE: there are two basic APIs to check Windows version: (empiric study following)
		GetVersionEx      -> reports version considering compatibility mode (and compatibility setting in app manifest since Windows 8.1)
		VerifyVersionInfo -> always reports *real* Windows Version 
								*) Win10 Technical preview caveat: VerifyVersionInfo returns 6.3 unless manifest entry is added!!!
*/

//GetVersionEx()-based APIs:
OsVersion getOsVersion();
inline bool win81OrLater        () { using namespace std::rel_ops; return getOsVersion() >= osVersionWin81;         }
inline bool win8OrLater         () { using namespace std::rel_ops; return getOsVersion() >= osVersionWin8;          }
inline bool win7OrLater         () { using namespace std::rel_ops; return getOsVersion() >= osVersionWin7;          }
inline bool vistaOrLater        () { using namespace std::rel_ops; return getOsVersion() >= osVersionWinVista;      }
inline bool winServer2003orLater() { using namespace std::rel_ops; return getOsVersion() >= osVersionWinServer2003; }
inline bool winXpOrLater        () { using namespace std::rel_ops; return getOsVersion() >= osVersionWinXp;         }

//VerifyVersionInfo()-based APIs:
bool isRealOsVersion(const OsVersion& ver);


bool runningWOW64();
bool running64BitWindows();




//######################### implementation #########################
inline
OsVersion getOsVersion()
{
    OSVERSIONINFO osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#ifdef _MSC_VER
#pragma warning(suppress: 4996) //"'GetVersionExW': was declared deprecated"
#endif
    if (!::GetVersionEx(&osvi)) //38 ns per call! (yes, that's nano!) -> we do NOT miss C++11 thread-safe statics right now...
    {
        assert(false);
        return OsVersion();
    }
    return OsVersion(osvi.dwMajorVersion, osvi.dwMinorVersion);
}


inline
bool isRealOsVersion(const OsVersion& ver)
{
    OSVERSIONINFOEX verInfo = {};
    verInfo.dwOSVersionInfoSize = sizeof(verInfo);
    verInfo.dwMajorVersion = ver.major;
    verInfo.dwMinorVersion = ver.minor;

    //Syntax: http://msdn.microsoft.com/en-us/library/windows/desktop/ms725491%28v=vs.85%29.aspx
    DWORDLONG conditionMask = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_EQUAL);

    const bool rv = ::VerifyVersionInfo(&verInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask)
                    == TRUE; //silence VC "performance warnings"
    assert(rv || ::GetLastError() == ERROR_OLD_WIN_VERSION);

    return rv;
}


inline
bool runningWOW64() //test if process is running under WOW64: http://msdn.microsoft.com/en-us/library/ms684139(VS.85).aspx
{
    typedef BOOL (WINAPI* IsWow64ProcessFun)(HANDLE hProcess, PBOOL Wow64Process);

    const SysDllFun<IsWow64ProcessFun> isWow64Process(L"kernel32.dll", "IsWow64Process");
    if (isWow64Process)
    {
        BOOL isWow64 = FALSE;
        if (isWow64Process(::GetCurrentProcess(), &isWow64))
            return isWow64 != FALSE;
    }
    return false;
}


inline
bool running64BitWindows() //http://blogs.msdn.com/b/oldnewthing/archive/2005/02/01/364563.aspx
{
#ifdef ZEN_BUILD_64BIT
    // MinFFS Patch. if binary is compiled for 64bit Windows, it will not run on 32bit
    return true;
#else
    // MinFFS Patch. if binary is compiled for 32bit windows, it check it is running in 32bit emulation.
    return runningWOW64();
#endif
}
}

#endif //WINDOWS_VERSION_HEADER_238470348254325
