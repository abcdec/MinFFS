// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "file_io.h"
#include "file_access.h"

#ifdef ZEN_WIN
    #include "long_path_prefix.h"
    #include "privilege.h"
    #ifdef ZEN_WIN_VISTA_AND_LATER
        #include "vista_file_op.h"
    #endif

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <sys/stat.h>
    #include <fcntl.h>  //open, close
    #include <unistd.h> //read, write
#endif

using namespace zen;


namespace
{
#if defined ZEN_LINUX || defined ZEN_MAC
//- "filepath" could be a named pipe which *blocks* forever for open()!
//- open() with O_NONBLOCK avoids the block, but opens successfully
//- create sample pipe: "sudo mkfifo named_pipe"
void checkForUnsupportedType(const Zstring& filepath) //throw FileError
{
    struct ::stat fileInfo = {};
    if (::stat(filepath.c_str(), &fileInfo) != 0) //follows symlinks
        return; //let the caller handle errors like "not existing"

    if (!S_ISREG(fileInfo.st_mode) &&
        !S_ISLNK(fileInfo.st_mode) &&
        !S_ISDIR(fileInfo.st_mode))
    {
        auto getTypeName = [](mode_t m) -> std::wstring
        {
            const wchar_t* name =
            S_ISCHR (m) ? L"character device":
            S_ISBLK (m) ? L"block device" :
            S_ISFIFO(m) ? L"FIFO, named pipe" :
            S_ISSOCK(m) ? L"socket" : nullptr;
            const std::wstring numFmt = printNumber<std::wstring>(L"0%06o", m & S_IFMT);
            return name ? numFmt + L", " + name : numFmt;
        };
        throw FileError(replaceCpy(_("Type of item %x is not supported:"), L"%x", fmtPath(filepath)) + L" " + getTypeName(fileInfo.st_mode));
    }
}
#endif

inline
FileHandle getInvalidHandle()
{
#ifdef ZEN_WIN
    return INVALID_HANDLE_VALUE;
#elif defined ZEN_LINUX || defined ZEN_MAC
    return -1;
#endif
}
}


FileInput::FileInput(FileHandle handle, const Zstring& filepath) : FileBase(filepath), fileHandle(handle) {}


FileInput::FileInput(const Zstring& filepath) : //throw FileError, ErrorFileLocked
    FileBase(filepath), fileHandle(getInvalidHandle())
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_activatePrivilege
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) {}
#endif//TODO_MinFFS_activatePrivilege

    auto createHandle = [&](DWORD dwShareMode)
    {
        return ::CreateFile(applyLongPathPrefix(filepath).c_str(),         //_In_      LPCTSTR lpFileName,
                            GENERIC_READ,              //_In_      DWORD dwDesiredAccess,
                            dwShareMode,               //_In_      DWORD dwShareMode,
                            nullptr,                   //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                            OPEN_EXISTING,             //_In_      DWORD dwCreationDisposition,
                            FILE_FLAG_SEQUENTIAL_SCAN //_In_      DWORD dwFlagsAndAttributes,
                            /* possible values: (Reference http://msdn.microsoft.com/en-us/library/aa363858(VS.85).aspx#caching_behavior)
                              FILE_FLAG_NO_BUFFERING
                              FILE_FLAG_RANDOM_ACCESS
                              FILE_FLAG_SEQUENTIAL_SCAN

                              tests on Win7 x64 show that FILE_FLAG_SEQUENTIAL_SCAN provides best performance for binary comparison in all cases:
                              - comparing different physical disks (DVD <-> HDD and HDD <-> HDD)
                              - even on same physical disk! (HDD <-> HDD)
                              - independent from client buffer size!

                              tests on XP show that FILE_FLAG_SEQUENTIAL_SCAN provides best performance for binary comparison when
                              - comparing different physical disks (DVD <-> HDD)

                              while FILE_FLAG_RANDOM_ACCESS offers best performance for
                              - same physical disk (HDD <-> HDD)

                            Problem: bad XP implementation of prefetch makes flag FILE_FLAG_SEQUENTIAL_SCAN effectively load two files at the same time
                            from one drive, swapping every 64 kB (or similar). File access times explode!
                            => For XP it is critical to use FILE_FLAG_RANDOM_ACCESS (to disable prefetch) if reading two files on same disk and
                            FILE_FLAG_SEQUENTIAL_SCAN when reading from different disk (e.g. massive performance improvement compared to random access for DVD <-> HDD!)
                            => there is no compromise that satisfies all cases! (on XP)

                            for FFS most comparisons are probably between different disks => let's use FILE_FLAG_SEQUENTIAL_SCAN
                             */
                            | FILE_FLAG_BACKUP_SEMANTICS,
                            nullptr); //_In_opt_  HANDLE hTemplateFile
    };
    fileHandle = createHandle(FILE_SHARE_READ | FILE_SHARE_DELETE);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        //=> support reading files which are open for write (e.g. Firefox db files): follow CopyFileEx() by addding FILE_SHARE_WRITE only for second try:
        if (::GetLastError() == ERROR_SHARING_VIOLATION)
            fileHandle = createHandle(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);

        //begin of "regular" error reporting
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!
            const std::wstring errorMsg = replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filepath));
            std::wstring errorDescr = formatSystemError(L"CreateFile", ec);

            if (ec == ERROR_SHARING_VIOLATION || //-> enhance error message!
                ec == ERROR_LOCK_VIOLATION)
            {
#ifdef ZEN_WIN_VISTA_AND_LATER //(try to) enhance error message
                const std::wstring procList = vista::getLockingProcesses(filepath); //noexcept
                if (!procList.empty())
                    errorDescr = _("The file is locked by another process:") + L"\n" + procList;
#endif
                throw ErrorFileLocked(errorMsg, errorDescr);
            }
            throw FileError(errorMsg, errorDescr);
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    checkForUnsupportedType(filepath); //throw FileError; opening a named pipe would block forever!

    //don't use O_DIRECT: http://yarchive.net/comp/linux/o_direct.html
    fileHandle = ::open(filepath.c_str(), O_RDONLY);
    if (fileHandle == -1) //don't check "< 0" -> docu seems to allow "-2" to be a valid file handle
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filepath)), L"open");
#endif

    //------------------------------------------------------------------------------------------------------

    ScopeGuard constructorGuard = zen::makeGuard([&] //destructor call would lead to member double clean-up!!!
    {
#ifdef ZEN_WIN
        ::CloseHandle(fileHandle);
#elif defined ZEN_LINUX || defined ZEN_MAC
        ::close(fileHandle);
#endif
    });

#ifdef ZEN_LINUX //handle still un-owned => need constructor guard
    //optimize read-ahead on input file:
    if (::posix_fadvise(fileHandle, 0, 0, POSIX_FADV_SEQUENTIAL) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(filepath)), L"posix_fadvise");

#elif defined ZEN_MAC
    //"dtruss" doesn't show use of "fcntl() F_RDAHEAD/F_RDADVISE" for "cp")
#endif

    constructorGuard.dismiss();
}


FileInput::~FileInput()
{
    if (fileHandle != getInvalidHandle())
#ifdef ZEN_WIN
        ::CloseHandle(fileHandle);
#elif defined ZEN_LINUX || defined ZEN_MAC
        ::close(fileHandle);
#endif
}


size_t FileInput::read(void* buffer, size_t bytesToRead) //throw FileError; returns actual number of bytes read
{
    size_t bytesReadTotal = 0;

    while (bytesToRead > 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
    {
#ifdef ZEN_WIN
        //test for end of file: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365690%28v=vs.85%29.aspx
        DWORD bytesRead = 0;
        if (!::ReadFile(fileHandle, //__in         HANDLE hFile,
                        buffer,     //__out        LPVOID lpBuffer,
                        static_cast<DWORD>(bytesToRead), //__in         DWORD nNumberOfBytesToRead,
                        &bytesRead, //__out_opt    LPDWORD lpNumberOfBytesRead,
                        nullptr))   //__inout_opt  LPOVERLAPPED lpOverlapped
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), L"ReadFile");

#elif defined ZEN_LINUX || defined ZEN_MAC
        ssize_t bytesRead = 0;
        do
        {
            bytesRead = ::read(fileHandle, buffer, bytesToRead);
        }
        while (bytesRead < 0 && errno == EINTR); //Compare copy_reg() in copy.c: ftp://ftp.gnu.org/gnu/coreutils/coreutils-8.23.tar.xz

        if (bytesRead < 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), L"read");
#endif
        if (bytesRead == 0) //"zero indicates end of file"
            return bytesReadTotal;

        if (static_cast<size_t>(bytesRead) > bytesToRead) //better safe than sorry
            throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), L"ReadFile: buffer overflow."); //user should never see this

        //if ::read is interrupted (EINTR) right in the middle, it will return successfully with "bytesRead < bytesToRead" => loop!
        buffer = static_cast<char*>(buffer) + bytesRead; //suppress warning about pointer arithmetics on void*
        bytesToRead -= bytesRead;
        bytesReadTotal += bytesRead;
    }
    return bytesReadTotal;
}

//----------------------------------------------------------------------------------------------------

FileOutput::FileOutput(FileHandle handle, const Zstring& filepath) : FileBase(filepath), fileHandle(handle) {}


FileOutput::FileOutput(const Zstring& filepath, AccessFlag access) : //throw FileError, ErrorTargetExisting
    FileBase(filepath), fileHandle(getInvalidHandle())
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_activatePrivilege
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) {}
    try { activatePrivilege(SE_RESTORE_NAME); }
    catch (const FileError&) {}
#endif//TODO_MinFFS_activatePrivilege

    const DWORD dwCreationDisposition = access == FileOutput::ACC_OVERWRITE ? CREATE_ALWAYS : CREATE_NEW;

    auto createHandle = [&](DWORD dwFlagsAndAttributes)
    {
        return ::CreateFile(applyLongPathPrefix(filepath).c_str(), //_In_      LPCTSTR lpFileName,
                            GENERIC_READ | GENERIC_WRITE, //_In_      DWORD dwDesiredAccess,
                            /*  http://msdn.microsoft.com/en-us/library/aa363858(v=vs.85).aspx
                                   quote: When an application creates a file across a network, it is better
                                   to use GENERIC_READ | GENERIC_WRITE for dwDesiredAccess than to use GENERIC_WRITE alone.
                                   The resulting code is faster, because the redirector can use the cache manager and send fewer SMBs with more data.
                                   This combination also avoids an issue where writing to a file across a network can occasionally return ERROR_ACCESS_DENIED. */
                            FILE_SHARE_DELETE,         //_In_      DWORD dwShareMode,
                            //FILE_SHARE_DELETE is required to rename file while handle is open!
                            nullptr,                   //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                            dwCreationDisposition,     //_In_      DWORD dwCreationDisposition,
                            dwFlagsAndAttributes |
                            FILE_FLAG_SEQUENTIAL_SCAN //_In_      DWORD dwFlagsAndAttributes,
                            | FILE_FLAG_BACKUP_SEMANTICS,
                            nullptr);                  //_In_opt_  HANDLE hTemplateFile
    };

    fileHandle = createHandle(FILE_ATTRIBUTE_NORMAL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

        //CREATE_ALWAYS fails with ERROR_ACCESS_DENIED if the existing file is hidden or "system" http://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
        if (ec == ERROR_ACCESS_DENIED && dwCreationDisposition == CREATE_ALWAYS)
        {
            const DWORD attrib = ::GetFileAttributes(applyLongPathPrefix(filepath).c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES)
            {
                fileHandle = createHandle(attrib); //retry: alas this may still fail for hidden file, e.g. accessing shared folder in XP as Virtual Box guest!
                ec = ::GetLastError();
            }
        }

        //begin of "regular" error reporting
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(filepath));
            std::wstring errorDescr = formatSystemError(L"CreateFile", ec);

#ifdef ZEN_WIN_VISTA_AND_LATER //(try to) enhance error message
            if (ec == ERROR_SHARING_VIOLATION || //-> enhance error message!
                ec == ERROR_LOCK_VIOLATION)
            {
                const std::wstring procList = vista::getLockingProcesses(filepath); //noexcept
                if (!procList.empty())
                    errorDescr = _("The file is locked by another process:") + L"\n" + procList;
            }
#endif
            if (ec == ERROR_FILE_EXISTS || //confirmed to be used
                ec == ERROR_ALREADY_EXISTS) //comment on msdn claims, this one is used on Windows Mobile 6
                throw ErrorTargetExisting(errorMsg, errorDescr);
            //if (ec == ERROR_PATH_NOT_FOUND) throw ErrorTargetPathMissing(errorMsg, errorDescr);

            throw FileError(errorMsg, errorDescr);
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    //checkForUnsupportedType(filepath); -> not needed, open() + O_WRONLY should fail fast

    fileHandle = ::open(filepath.c_str(), O_WRONLY | O_CREAT | (access == FileOutput::ACC_CREATE_NEW ? O_EXCL : O_TRUNC),
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fileHandle == -1)
    {
        const int ec = errno; //copy before making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(filepath));
        const std::wstring errorDescr = formatSystemError(L"open", ec);

        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        //if (ec == ENOENT) throw ErrorTargetPathMissing(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
#endif

    //------------------------------------------------------------------------------------------------------

    //ScopeGuard constructorGuard = zen::makeGuard

    //guard handle when adding code!!!

    //constructorGuard.dismiss();
}


FileOutput::FileOutput(FileOutput&& tmp) : FileBase(tmp.getFilePath()), fileHandle(tmp.fileHandle) { tmp.fileHandle = getInvalidHandle(); }


FileOutput::~FileOutput()
{
    if (fileHandle != getInvalidHandle())
        try
        {
            close(); //throw FileError
        }
        catch (FileError&) { assert(false); }
}


void FileOutput::close() //throw FileError
{
    if (fileHandle == getInvalidHandle())
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"Contract error: close() called more than once.");
    ZEN_ON_SCOPE_EXIT(fileHandle = getInvalidHandle());

    //no need to clean-up on failure here (just like there is no clean on FileOutput::write failure!) => FileOutput is not transactional!

#ifdef ZEN_WIN
    if (!::CloseHandle(fileHandle))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"CloseHandle");
#elif defined ZEN_LINUX || defined ZEN_MAC
    if (::close(fileHandle) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"close");
#endif
}


void FileOutput::write(const void* buffer, size_t bytesToWrite) //throw FileError
{
#ifdef ZEN_WIN
    DWORD bytesWritten = 0; //this parameter is NOT optional: http://blogs.msdn.com/b/oldnewthing/archive/2013/04/04/10407417.aspx
    if (!::WriteFile(fileHandle,    //__in         HANDLE hFile,
                     buffer,        //__out        LPVOID lpBuffer,
                     static_cast<DWORD>(bytesToWrite),  //__in         DWORD nNumberOfBytesToWrite,
                     &bytesWritten, //__out_opt    LPDWORD lpNumberOfBytesWritten,
                     nullptr))      //__inout_opt  LPOVERLAPPED lpOverlapped
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"WriteFile");

    if (bytesWritten != bytesToWrite) //must be fulfilled for synchronous writes!
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"WriteFile: incomplete write."); //user should never see this

#elif defined ZEN_LINUX || defined ZEN_MAC
    while (bytesToWrite > 0)
    {
        ssize_t bytesWritten = 0;
        do
        {
            bytesWritten = ::write(fileHandle, buffer, bytesToWrite);
        }
        while (bytesWritten < 0 && errno == EINTR);

        if (bytesWritten <= 0)
        {
            if (bytesWritten == 0) //comment in safe-read.c suggests to treat this as an error due to buggy drivers
                errno = ENOSPC;

            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"write");
        }
        if (bytesWritten > static_cast<ssize_t>(bytesToWrite)) //better safe than sorry
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"write: buffer overflow."); //user should never see this

        //if ::write() is interrupted (EINTR) right in the middle, it will return successfully with "bytesWritten < bytesToWrite"!
        buffer = static_cast<const char*>(buffer) + bytesWritten; //suppress warning about pointer arithmetics on void*
        bytesToWrite -= bytesWritten;
    }
#endif
}
