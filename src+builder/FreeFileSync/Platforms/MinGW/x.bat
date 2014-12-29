REM // **************************************************************************
REM // * Copyright (C) 2014 abcdec @GitHub - All Rights Reserved                *
REM // * This file is part of a modified version of FreeFileSync, MinFFS.       *
REM // * The original FreeFileSync program and source code are distributed by   *
REM // * the FreeFileSync project: http://www.freefilesync.org/                 *
REM // * This particular file is created by by abcdec @GitHub as part of the    *
REM // * MinFFS project: https://github.com/abcdec/MinFFS                       *
REM // *                          --EXPERIMENTAL--                              *
REM // * This program is experimental and not recommended for general use.      *
REM // * Please consider using the original FreeFileSync program unless there   *
REM // * are specific needs to use this experimental MinFFS version.            *
REM // *                          --EXPERIMENTAL--                              *
REM // * This file is distributed under GNU General Public License:             *
REM // * http://www.gnu.org/licenses/gpl-3.0 per the FreeFileSync License.      *
REM // * This modified program is distributed in the hope that it will be       *
REM // * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
REM // * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
REM // * General Public License for more details.                               *
REM // **************************************************************************
@echo off

@IF NOT EXIST %MINGW% MINGW_NOT_DEFINED
@IF NOT EXIST %WXWIN% WXWIN_NOT_DEFINED

@IF NOT EXIST "bin-debug" mkdir bin-debug
@IF NOT EXIST "bin-debug\Bin" mkdir bin-debug\Bin

@xcopy FreeFileSyncMod.exe bin-debug\Bin\ /Y/Q > NUL
@xcopy ..\MswCommon\DLLs\* bin-debug\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libstdc++-6.dll bin-debug\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libgcc_s_dw2-1.dll bin-debug\Bin\ /Y/Q > NUL
@xcopy ..\MswCommon\Help\FreeFileSync.chm bin-debug\ /Y/Q > NUL
@xcopy ..\..\Build\Languages bin-debug\Languages\ /Y/Q/S  > NUL
@xcopy ..\..\Build\Resources.zip bin-debug\ /Y/Q > NUL
@xcopy ..\..\Build\Sync_Complete.wav bin-debug\ /Y/Q > NUL

@echo Launching MinFFS
bin-debug\Bin\FreeFileSyncMod.exe

GOTO END

:MINGW_NOT_DEFINED
%MINGW% echo Please set MINGW environment variable properly. Exit.
GOTO END

:WXWIN_NOT_DEFINED
%MINGW% echo Please set WXWIN environment variable properly. Exit.
GOTO END

:END
