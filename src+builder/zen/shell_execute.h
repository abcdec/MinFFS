// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef EXECUTE_HEADER_23482134578134134
#define EXECUTE_HEADER_23482134578134134

#include "file_error.h"

#ifdef ZEN_WIN
    #include "scope_guard.h"
    #include "win.h" //includes "windows.h"

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include "thread.h"
    #include <stdlib.h> //::system()
#endif


namespace zen
{
//launch commandline and report errors via popup dialog
//windows: COM needs to be initialized before calling this function!
enum ExecutionType
{
    EXEC_TYPE_SYNC,
    EXEC_TYPE_ASYNC
};

namespace
{
#ifdef ZEN_WIN
template <class Function>
bool shellExecuteImpl(Function fillExecInfo, ExecutionType type)
{
    SHELLEXECUTEINFO execInfo = {};
    execInfo.cbSize = sizeof(execInfo);
    execInfo.lpVerb = nullptr;
    execInfo.nShow  = SW_SHOWNORMAL;
    execInfo.fMask  = type == EXEC_TYPE_SYNC ? (SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC) : 0;
    //don't use SEE_MASK_ASYNCOK -> different async mode than the default which returns successful despite errors!
    execInfo.fMask |= SEE_MASK_FLAG_NO_UI; //::ShellExecuteEx() shows a non-blocking pop-up dialog on errors -> we want a blocking one
    //for the record, SEE_MASK_UNICODE does nothing: http://blogs.msdn.com/b/oldnewthing/archive/2014/02/27/10503519.aspx

    fillExecInfo(execInfo);

    if (!::ShellExecuteEx(&execInfo)) //__inout  LPSHELLEXECUTEINFO lpExecInfo
        return false;

    if (execInfo.hProcess)
    {
        ZEN_ON_SCOPE_EXIT(::CloseHandle(execInfo.hProcess));

        if (type == EXEC_TYPE_SYNC)
            ::WaitForSingleObject(execInfo.hProcess, INFINITE);
    }
    return true;
}


void shellExecute(const void* /*PCIDLIST_ABSOLUTE*/ shellItemPidl, const std::wstring& displayPath, ExecutionType type) //throw FileError
{
    auto fillExecInfo = [&](SHELLEXECUTEINFO& execInfo)
    {
        execInfo.fMask |= SEE_MASK_IDLIST;
        execInfo.lpIDList = const_cast<void*>(shellItemPidl); //lpIDList is documented as PCIDLIST_ABSOLUTE!
    };

    if (!shellExecuteImpl(fillExecInfo , type)) //throw FileError
        THROW_LAST_FILE_ERROR(_("Incorrect command line:") + L"\n" + fmtPath(displayPath), L"ShellExecuteEx");
}
#endif


void shellExecute(const Zstring& command, ExecutionType type) //throw FileError
{
#ifdef ZEN_WIN
    //parse commandline
    Zstring commandTmp = command;
    trim(commandTmp, true, false); //CommandLineToArgvW() does not like leading spaces

    std::vector<Zstring> argv;
    int argc = 0;
    if (LPWSTR* tmp = ::CommandLineToArgvW(commandTmp.c_str(), &argc))
    {
        ZEN_ON_SCOPE_EXIT(::LocalFree(tmp));
        std::copy(tmp, tmp + argc, std::back_inserter(argv));
    }

    Zstring filepath;
    Zstring arguments;
    if (!argv.empty())
    {
        filepath = argv[0];
        for (auto iter = argv.begin() + 1; iter != argv.end(); ++iter)
            arguments += (iter != argv.begin() ? L" " : L"") +
                         (iter->empty() || std::any_of(iter->begin(), iter->end(), &isWhiteSpace<wchar_t>) ? L"\"" + *iter + L"\"" : *iter);
    }

    auto fillExecInfo = [&](SHELLEXECUTEINFO& execInfo)
    {
        execInfo.lpFile       = filepath.c_str();
        execInfo.lpParameters = arguments.c_str();
    };

    if (!shellExecuteImpl(fillExecInfo, type))
        THROW_LAST_FILE_ERROR(_("Incorrect command line:") + L"\nFile: " + fmtPath(filepath) + L"\nArg: " + copyStringTo<std::wstring>(arguments), L"ShellExecuteEx");

#elif defined ZEN_LINUX || defined ZEN_MAC
    /*
    we cannot use wxExecute due to various issues:
    - screws up encoding on OS X for non-ASCII characters
    - does not provide any reasonable error information
    - uses a zero-sized dummy window as a hack to keep focus which leaves a useless empty icon in ALT-TAB list in Windows
    */

    if (type == EXEC_TYPE_SYNC)
    {
        //Posix::system - execute a shell command
        int rv = ::system(command.c_str()); //do NOT use std::system as its documentation says nothing about "WEXITSTATUS(rv)", ect...
        if (rv == -1 || WEXITSTATUS(rv) == 127) //http://linux.die.net/man/3/system    "In case /bin/sh could not be executed, the exit status will be that of a command that does exit(127)"
            throw FileError(_("Incorrect command line:") + L"\n" + utfCvrtTo<std::wstring>(command));
    }
    else
        runAsync([=] { int rv = ::system(command.c_str()); (void)rv; });
#endif
}
}
}

#endif //EXECUTE_HEADER_23482134578134134
