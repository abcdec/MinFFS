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

@IF NOT EXIST "bin-release" mkdir bin-release
@IF NOT EXIST "bin-release\Bin" mkdir bin-release\Bin

@xcopy MinFFS.exe bin-release\Bin\ /Y/Q > NUL
@xcopy License.rtf bin-release\ /Y/Q > NUL
@xcopy ..\MswCommon\DLLs\* bin-release\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libstdc++-6.dll bin-release\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libgcc_s_dw2-1.dll bin-release\Bin\ /Y/Q > NUL
@xcopy ..\MswCommon\Help\FreeFileSync.chm bin-release\ /Y/Q > NUL
@xcopy ..\..\Build\Languages bin-release\Languages\ /Y/Q/S  > NUL
@xcopy ..\..\Build\Resources.zip bin-release\ /Y/Q > NUL
@xcopy ..\..\Build\Sync_Complete.wav bin-release\ /Y/Q > NUL

@echo Packaging MinFFS
"C:\Program Files (x86)\NSIS\Unicode\makensis.exe" MinFFS-Setup.nsi

GOTO END

:MINGW_NOT_DEFINED
%MINGW% echo Please set MINGW environment variable properly. Exit.
GOTO END

:WXWIN_NOT_DEFINED
%MINGW% echo Please set WXWIN environment variable properly. Exit.
GOTO END

:END
