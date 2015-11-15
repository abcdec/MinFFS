@REM // **************************************************************************
@REM // * Copyright (C) 2015 abcdec @GitHub - All Rights Reserved                *
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

@IF NOT DEFINED MINGW ( GOTO MINGW_NOT_DEFINED )
@IF NOT DEFINED WXWIN ( GOTO WXWIN_NOT_DEFINED )

@IF NOT EXIST %MINGW% ( GOTO MINGW_NOT_DEFINED )
@IF NOT EXIST %WXWIN% ( GOTO WXWIN_NOT_DEFINED )

@IF EXIST ExMinFFS.exe del ExMinFFS.exe

hhc ..\FreeFileSync\Build\Help\FreeFileSync.hhp
copy ..\FreeFileSync\Build\FreeFileSync.chm .

mkdir MinFFS_Doxygen
cd ..
doxygen ExMinFFS\MinFFS_Doxygen.conf
cd ExMinFFS
hhc MinFFS_Doxygen\html\index.hhp
copy MinFFS_Doxygen\html\index.chm MinFFS_Doxygen.chm

GOTO END

:MINGW_NOT_DEFINED
echo MINGW environment variable is not defined properly.  Please check setenv.bat and run it before running this batch file.
GOTO END

:WXWIN_NOT_DEFINED
echo WXWIN environment variable is not defined properly.  Please check setenv.bat and run it before running this batch file.
GOTO END

:END
