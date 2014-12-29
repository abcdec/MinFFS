@REM // **************************************************************************
@REM // * Copyright (C) 2014 abcdec @GitHub - All Rights Reserved                *
@REM // * This file is part of a modified version of FreeFileSync, MinFFS.       *
@REM // * The original FreeFileSync program and source code are distributed by   *
@REM // * the FreeFileSync project: http://www.freefilesync.org/                 *
@REM // * This particular file is created by by abcdec @GitHub as part of the    *
@REM // * MinFFS project: https://github.com/abcdec/MinFFS                       *
@REM // *                          --EXPERIMENTAL--                              *
@REM // * This program is experimental and not recommended for general use.      *
@REM // * Please consider using the original FreeFileSync program unless there   *
@REM // * are specific needs to use this experimental MinFFS version.            *
@REM // *                          --EXPERIMENTAL--                              *
@REM // * This file is distributed under GNU General Public License:             *
@REM // * http://www.gnu.org/licenses/gpl-3.0 per the FreeFileSync License.      *
@REM // * This modified program is distributed in the hope that it will be       *
@REM // * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
@REM // * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
@REM // * General Public License for more details.                               *
@REM // **************************************************************************

@echo off

@IF NOT EXIST %MINGW% MINGW_NOT_DEFINED
@IF NOT EXIST %WXWIN% WXWIN_NOT_DEFINED


g++ -g -mthreads -DHAVE_W32API_H -D__WXMSW__ -D__WXDEBUG__ -D_UNICODE -IC:\wxWidgets\lib\gcc_lib\mswud -IC:\wxWidgets\include -Wno-ctor-dtor-privacy -pipe -fmessage-length=0 -Wl,--subsystem,windows -mwindows -Wl,--enable-auto-import -c -o sanity.o sanity.cpp
g++ -o sanity.exe sanity.o -mthreads -LC:\wxWidgets\lib\gcc_lib -lwxmsw30ud_xrc -lwxmsw30ud_aui -lwxmsw30ud_html -lwxmsw30ud_adv -lwxmsw30ud_core -lwxbase30ud_xml -lwxbase30ud_net -lwxbase30ud -lwxtiffd -lwxjpegd -lwxpngd -lwxzlibd -lwxregexud -lwxexpatd -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwxregexud -lwinspool -lwinmm -lshell32 -lcomctl32 -lole32 -loleaut32 -luuid -lrpcrt4 -ladvapi32 -lwsock32


GOTO END

:MINGW_NOT_DEFINED
%MINGW% echo Please set MINGW environment variable properly. Exit.
GOTO END

:WXWIN_NOT_DEFINED
%MINGW% echo Please set WXWIN environment variable properly. Exit.
GOTO END

:END
