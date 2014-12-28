@echo off

@IF NOT EXIST %MINGW% MINGW_NOT_DEFINED
@IF NOT EXIST %WXWIN% WXWIN_NOT_DEFINED

@IF NOT EXIST "bin-debug" mkdir bin-debug
@IF NOT EXIST "bin-debug\Bin" mkdir bin-debug\Bin

@xcopy FreeFileSyncMod.exe bin-debug\Bin\ /Y/Q > NUL
@xcopy ..\MswCommon\DLLs\* bin-debug\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libstdc++-6.dll bin-debug\Bin\ /Y/Q > NUL
@xcopy %MINGW%\bin\libgcc_s_dw2-1.dll bin-debug\Bin\ /Y/Q > NUL
@xcopy ..\MswCommon\Help\FreeFileSync.chm bin-debug\Bin\ /Y/Q > NUL
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
