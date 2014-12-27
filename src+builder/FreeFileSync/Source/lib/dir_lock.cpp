// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
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
#include "dir_lock.h"
#include <utility>
#include <wx/log.h>
#include <memory>
#include <zen/sys_error.h>
#include <zen/thread.h> //includes <boost/thread.hpp>
#include <zen/scope_guard.h>
#include <zen/guid.h>
#include <zen/tick_count.h>
#include <zen/int64.h>
#include <zen/file_access.h>
#include <zen/serialize.h>
#include <zen/optional.h>

#ifdef ZEN_WIN
    #include <tlhelp32.h>
    #include <zen/win.h> //includes "windows.h"
    #include <zen/long_path_prefix.h>
    #include <Sddl.h> //login sid
    #include <Lmcons.h> //UNLEN

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <fcntl.h>    //open()
    #include <sys/stat.h> //
    #include <unistd.h> //getsid()
    #include <signal.h> //kill()
    #include <pwd.h> //getpwuid_r()
#endif

using namespace zen;
using namespace std::rel_ops;


namespace
{
const int EMIT_LIFE_SIGN_INTERVAL   =  5; //show life sign;        unit: [s]
const int POLL_LIFE_SIGN_INTERVAL   =  4; //poll for life sign;    unit: [s]
const int DETECT_ABANDONED_INTERVAL = 30; //assume abandoned lock; unit: [s]

const char LOCK_FORMAT_DESCR[] = "FreeFileSync";
const int LOCK_FORMAT_VER = 2; //lock file format version
}

//worker thread
class LifeSigns
{
public:
    LifeSigns(const Zstring& lockfilepath) : //throw()!!! siehe SharedDirLock()
        lockfilepath_(lockfilepath) {} //thread safety: make deep copy!

    void operator()() const //thread entry
    {
        try
        {
            while (true)
            {
#ifdef TODO_MinFFS_UI
                boost::this_thread::sleep(boost::posix_time::seconds(EMIT_LIFE_SIGN_INTERVAL)); //interruption point!
#endif//TODO_MinFFS_UI

                //actual work
                emitLifeSign(); //throw ()
            }
        }
        catch (const std::exception& e) //exceptions must be catched per thread
        {
            wxSafeShowMessage(L"FreeFileSync - " + _("An exception occurred"), utfCvrtTo<wxString>(e.what()) + L" (Dirlock)"); //simple wxMessageBox won't do for threads
        }
    }

    void emitLifeSign() const //try to append one byte...; throw()
    {
        const char buffer[1] = {' '};
#ifdef ZEN_WIN
        //ATTENTION: setting file pointer IS required! => use CreateFile/GENERIC_WRITE + SetFilePointerEx!
        //although CreateFile/FILE_APPEND_DATA without SetFilePointerEx works locally, it MAY NOT work on some network shares creating a 4 gig file!!!

        const HANDLE fileHandle = ::CreateFile(applyLongPathPrefix(lockfilepath_).c_str(), //_In_      LPCTSTR lpFileName,
                                               //use both when writing over network, see comment in file_io.cpp
                                               GENERIC_READ | GENERIC_WRITE,  //_In_      DWORD dwDesiredAccess,
                                               FILE_SHARE_READ,               //_In_      DWORD dwShareMode,
                                               nullptr,                       //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                               OPEN_EXISTING,                 //_In_      DWORD dwCreationDisposition,
                                               FILE_ATTRIBUTE_NORMAL,         //_In_      DWORD dwFlagsAndAttributes,
                                               nullptr);                      //_In_opt_  HANDLE hTemplateFile
        if (fileHandle == INVALID_HANDLE_VALUE)
            return;
        ZEN_ON_SCOPE_EXIT(::CloseHandle(fileHandle));

        const LARGE_INTEGER moveDist = {};
        if (!::SetFilePointerEx(fileHandle, //__in       HANDLE hFile,
                                moveDist,   //__in       LARGE_INTEGER liDistanceToMove,
                                nullptr,    //__out_opt  PLARGE_INTEGER lpNewFilePointer,
                                FILE_END))  //__in       DWORD dwMoveMethod
            return;

        DWORD bytesWritten = 0; //this parameter is NOT optional: http://blogs.msdn.com/b/oldnewthing/archive/2013/04/04/10407417.aspx
        if (!::WriteFile(fileHandle,    //_In_         HANDLE hFile,
                         buffer,        //_In_         LPCVOID lpBuffer,
                         1,             //_In_         DWORD nNumberOfBytesToWrite,
                         &bytesWritten, //_Out_opt_    LPDWORD lpNumberOfBytesWritten,
                         nullptr))      //_Inout_opt_  LPOVERLAPPED lpOverlapped
            return;

#elif defined ZEN_LINUX || defined ZEN_MAC
        const int fileHandle = ::open(lockfilepath_.c_str(), O_WRONLY | O_APPEND);
        if (fileHandle == -1)
            return;
        ZEN_ON_SCOPE_EXIT(::close(fileHandle));

        const ssize_t bytesWritten = ::write(fileHandle, buffer, 1);
        (void)bytesWritten;
#endif
    }

private:
    const Zstring lockfilepath_; //thread local! atomic ref-count => binary value-type semantics!
};


namespace
{
std::uint64_t getLockFileSize(const Zstring& filepath) //throw FileError
{
#ifdef ZEN_WIN
    WIN32_FIND_DATA fileInfo = {};
    const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(filepath).c_str(), &fileInfo);
    if (searchHandle == INVALID_HANDLE_VALUE)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"FindFirstFile", getLastError());
    ::FindClose(searchHandle);

    return get64BitUInt(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::stat(filepath.c_str(), &fileInfo) != 0) //follow symbolic links
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"stat", getLastError());

    return fileInfo.st_size;
#endif
}


Zstring deleteAbandonedLockName(const Zstring& lockfilepath) //make sure to NOT change file ending!
{
    const size_t pos = lockfilepath.rfind(FILE_NAME_SEPARATOR); //search from end
    return pos == Zstring::npos ? Zstr("Del.") + lockfilepath :
           Zstring(lockfilepath.c_str(), pos + 1) + //include path separator
           Zstr("Del.") +
           afterLast(lockfilepath, FILE_NAME_SEPARATOR); //returns the whole string if ch not found
}


#ifdef ZEN_WIN
Zstring getLoginSid() //throw FileError
{
    HANDLE hToken = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), //__in   HANDLE ProcessHandle,
                            TOKEN_ALL_ACCESS,      //__in   DWORD DesiredAccess,
                            &hToken))              //__out  PHANDLE TokenHandle
        throwFileError(_("Cannot get process information."), L"OpenProcessToken", getLastError());
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hToken));

    DWORD bufferSize = [&]
    {
        DWORD sz = 0;
        if (!::GetTokenInformation(hToken, TokenGroups, nullptr, 0, &sz))
        {
            if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                throwFileError(_("Cannot get process information."), L"GetTokenInformation", getLastError());

            if (sz > 0)
                return sz;
        }

        throw FileError(_("Cannot get process information."), L"failed to get GetTokenInformation buffer size"); //shouldn't happen
    }();

    std::vector<char> buffer(bufferSize);
    if (!::GetTokenInformation(hToken,       //__in       HANDLE TokenHandle,
                               TokenGroups,  //__in       TOKEN_INFORMATION_CLASS TokenInformationClass,
                               &buffer[0],   //__out_opt  LPVOID TokenInformation,
                               bufferSize,   //__in       DWORD TokenInformationLength,
                               &bufferSize)) //__out      PDWORD ReturnLength
        throwFileError(_("Cannot get process information."), L"GetTokenInformation", getLastError());

    auto groups = reinterpret_cast<const TOKEN_GROUPS*>(&buffer[0]);

    for (DWORD i = 0; i < groups->GroupCount; ++i)
        if ((groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) != 0)
        {
            LPTSTR sidStr = nullptr;
            if (!::ConvertSidToStringSid(groups->Groups[i].Sid, //__in   PSID Sid,
                                         &sidStr))              //__out  LPTSTR *StringSid
                throwFileError(_("Cannot get process information."), L"ConvertSidToStringSid", getLastError());
            ZEN_ON_SCOPE_EXIT(::LocalFree(sidStr));
            return sidStr;
        }
    throw FileError(_("Cannot get process information."), L"no login found"); //shouldn't happen
}
#endif


#ifdef ZEN_WIN
    typedef DWORD ProcessId;
    typedef DWORD SessionId;
#elif defined ZEN_LINUX || defined ZEN_MAC
    typedef pid_t ProcessId;
    typedef pid_t SessionId;
#endif

//return ppid on Windows, sid on Linux/Mac, "no value" if process corresponding to "processId" is not existing
Opt<SessionId> getSessionId(ProcessId processId) //throw FileError
{
#ifdef ZEN_WIN
    //note: ::OpenProcess() is no alternative as it may successfully return for crashed processes! -> remark: "WaitForSingleObject" may identify this case!
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, //__in  DWORD dwFlags,
                                                 0);                 //__in  DWORD th32ProcessID
    if (snapshot == INVALID_HANDLE_VALUE)
        throwFileError(_("Cannot get process information."), L"CreateToolhelp32Snapshot", getLastError());
    ZEN_ON_SCOPE_EXIT(::CloseHandle(snapshot));

    PROCESSENTRY32 processEntry = {};
    processEntry.dwSize = sizeof(processEntry);

    if (!::Process32First(snapshot,       //__in     HANDLE hSnapshot,
                          &processEntry)) //__inout  LPPROCESSENTRY32 lppe
        throwFileError(_("Cannot get process information."), L"Process32First", getLastError()); //ERROR_NO_MORE_FILES not possible
    do
    {
        if (processEntry.th32ProcessID == processId) //yes, MSDN says this is the way: http://msdn.microsoft.com/en-us/library/windows/desktop/ms684868(v=vs.85).aspx
            return processEntry.th32ParentProcessID; //parent id is stable, even if parent process has already terminated!
    }
    while (::Process32Next(snapshot, &processEntry));

    const DWORD lastError = ::GetLastError(); //caveat: eval before "throw" which can overwrite error code after memory allocation! (MinGW + Win XP)
    if (lastError != ERROR_NO_MORE_FILES) //yes, they call it "files"
        throwFileError(_("Cannot get process information."), L"Process32Next", lastError);

    return NoValue();

#elif defined ZEN_LINUX || defined ZEN_MAC
    if (::kill(processId, 0) != 0) //sig == 0: no signal sent, just existence check
        return NoValue();

    pid_t procSid = ::getsid(processId); //NOT to be confused with "login session", e.g. not stable on OS X!!!
    if (procSid == -1)
        throwFileError(_("Cannot get process information."), L"getsid", getLastError());

    return procSid;
#endif
}


class FromCurrentProcess {}; //tag

struct LockInformation //throw FileError
{
    explicit LockInformation(FromCurrentProcess) :
        lockId(zen::generateGUID()),
        sessionId(), //dummy value
#ifdef ZEN_WIN
        processId(::GetCurrentProcessId()) //never fails
    {
        DWORD bufferSize = 0;
        ::GetComputerNameEx(ComputerNameDnsFullyQualified, nullptr, &bufferSize); //get required buffer size

        std::vector<wchar_t> buffer(bufferSize);
        if (!::GetComputerNameEx(ComputerNameDnsFullyQualified,         //__in     COMPUTER_NAME_FORMAT NameType,
                                 bufferSize > 0 ? &buffer[0] : nullptr, //__out    LPTSTR lpBuffer,
                                 &bufferSize))                          //__inout  LPDWORD lpnSize
            throwFileError(_("Cannot get process information."), L"GetComputerNameEx", getLastError());

        computerName = "Windows." + utfCvrtTo<std::string>(&buffer[0]);

        bufferSize = UNLEN + 1;
        buffer.resize(bufferSize);
        if (!::GetUserName(&buffer[0],   //__out    LPTSTR lpBuffer,
                           &bufferSize)) //__inout  LPDWORD lpnSize
            throwFileError(_("Cannot get process information."), L"GetUserName", getLastError());
        userId = utfCvrtTo<std::string>(&buffer[0]);

#elif defined ZEN_LINUX || defined ZEN_MAC
        processId(::getpid()) //never fails
    {
        std::vector<char> buffer(10000);

        if (::gethostname(&buffer[0], buffer.size()) != 0)
            throwFileError(_("Cannot get process information."), L"gethostname", getLastError());
        computerName += "Linux."; //distinguish linux/windows lock files
        computerName += &buffer[0];

        if (::getdomainname(&buffer[0], buffer.size()) != 0)
            throwFileError(_("Cannot get process information."), L"getdomainname", getLastError());
        computerName += ".";
        computerName += &buffer[0];

        const uid_t userIdNo = ::getuid(); //never fails
        userId = numberTo<std::string>(userIdNo);

        //the id alone is not very distinctive, e.g. often 1000 on Ubuntu => add name
        buffer.resize(std::max<long>(buffer.size(), ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1)
        struct passwd buffer2 = {};
        struct passwd* pwsEntry = nullptr;
        if (::getpwuid_r(userIdNo, &buffer2, &buffer[0], buffer.size(), &pwsEntry) != 0) //getlogin() is deprecated and not working on Ubuntu at all!!!
            throwFileError(_("Cannot get process information."), L"getpwuid_r", getLastError());
        if (!pwsEntry)
            throw FileError(_("Cannot get process information."), L"no login found"); //should not happen?
        userId += '(' + std::string(pwsEntry->pw_name) + ')'; //follow Linux naming convention "1000(zenju)"
#endif

        Opt<SessionId> sessionIdTmp = getSessionId(processId); //throw FileError
        if (!sessionIdTmp)
            throw FileError(_("Cannot get process information."), L"no session id found"); //should not happen?
        sessionId = *sessionIdTmp;
    }

    explicit LockInformation(BinStreamIn& stream) //throw UnexpectedEndOfStreamError
    {
        char tmp[sizeof(LOCK_FORMAT_DESCR)] = {};
        readArray(stream, &tmp, sizeof(tmp));                           //file format header
        const int lockFileVersion = readNumber<boost::int32_t>(stream); //

        if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(LOCK_FORMAT_DESCR)) ||
            lockFileVersion != LOCK_FORMAT_VER)
            throw UnexpectedEndOfStreamError(); //well, not really...!?

        lockId       = readContainer<std::string>(stream); //
        computerName = readContainer<std::string>(stream); //UnexpectedEndOfStreamError
        userId       = readContainer<std::string>(stream); //
        sessionId    = static_cast<SessionId>(readNumber<std::uint64_t>(stream)); //[!] conversion
        processId    = static_cast<ProcessId>(readNumber<std::uint64_t>(stream)); //[!] conversion
    }

    void toStream(BinStreamOut& stream) const //throw ()
    {
        writeArray(stream, LOCK_FORMAT_DESCR, sizeof(LOCK_FORMAT_DESCR));
        writeNumber<boost::int32_t>(stream, LOCK_FORMAT_VER);

        static_assert(sizeof(processId) <= sizeof(std::uint64_t), ""); //ensure cross-platform compatibility!
        static_assert(sizeof(sessionId) <= sizeof(std::uint64_t), ""); //

        writeContainer(stream, lockId);
        writeContainer(stream, computerName);
        writeContainer(stream, userId);
        writeNumber<std::uint64_t>(stream, sessionId);
        writeNumber<std::uint64_t>(stream, processId);
    }

    std::string lockId; //16 byte GUID - a universal identifier for this lock (no matter what the path is, considering symlinks, distributed network, etc.)

    //identify local computer
    std::string computerName; //format: HostName.DomainName
    std::string userId;

    //identify running process
    SessionId sessionId; //Windows: parent process id; Linux/OS X: session of the process, NOT the user
    ProcessId processId;
};


//wxGetFullHostName() is a performance killer for some users, so don't touch!


LockInformation retrieveLockInfo(const Zstring& lockfilepath) //throw FileError
{
    BinStreamIn streamIn = loadBinStream<BinaryStream>(lockfilepath,  nullptr); //throw FileError
    try
    {
        return LockInformation(streamIn); //throw UnexpectedEndOfStreamError
    }
    catch (UnexpectedEndOfStreamError&)
    {
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(lockfilepath)), L"unexpected end of stream");
    }
}


inline
std::string retrieveLockId(const Zstring& lockfilepath) //throw FileError
{
    return retrieveLockInfo(lockfilepath).lockId; //throw FileError
}


enum ProcessStatus
{
    PROC_STATUS_NOT_RUNNING,
    PROC_STATUS_RUNNING,
    PROC_STATUS_ITS_US,
    PROC_STATUS_CANT_TELL
};

ProcessStatus getProcessStatus(const LockInformation& lockInfo) //throw FileError
{
    const LockInformation localInfo((FromCurrentProcess())); //throw FileError

    if (lockInfo.computerName != localInfo.computerName ||
        lockInfo.userId != localInfo.userId) //another user may run a session right now!
        return PROC_STATUS_CANT_TELL; //lock owned by different computer in this network

    if (lockInfo.sessionId == localInfo.sessionId &&
        lockInfo.processId == localInfo.processId) //obscure, but possible: deletion failed or a lock file is "stolen" and put back while the program is running
        return PROC_STATUS_ITS_US;

    if (Opt<SessionId> sessionId = getSessionId(lockInfo.processId)) //throw FileError
        return *sessionId == lockInfo.sessionId ? PROC_STATUS_RUNNING : PROC_STATUS_NOT_RUNNING;
    return PROC_STATUS_NOT_RUNNING;
}


const std::int64_t TICKS_PER_SEC = ticksPerSec(); //= 0 on error


void waitOnDirLock(const Zstring& lockfilepath, DirLockCallback* callback) //throw FileError
{
    std::wstring infoMsg = _("Waiting while directory is locked:") + L' ' + fmtFileName(lockfilepath);

    if (callback)
        callback->reportStatus(infoMsg);
    //---------------------------------------------------------------
    try
    {
        //convenience optimization only: if we know the owning process crashed, we needn't wait DETECT_ABANDONED_INTERVAL sec
        bool lockOwnderDead = false;
        std::string originalLockId; //empty if it cannot be retrieved
        try
        {
            const LockInformation& lockInfo = retrieveLockInfo(lockfilepath); //throw FileError
            //enhance status message and show which user is holding the lock:
            infoMsg += L" | " + _("Lock owner:") +  L' ' + utfCvrtTo<std::wstring>(lockInfo.userId);

            originalLockId = lockInfo.lockId;
            switch (getProcessStatus(lockInfo)) //throw FileError
            {
                case PROC_STATUS_ITS_US: //since we've already passed LockAdmin, the lock file seems abandoned ("stolen"?) although it's from this process
                case PROC_STATUS_NOT_RUNNING:
                    lockOwnderDead = true;
                    break;
                case PROC_STATUS_RUNNING:
                case PROC_STATUS_CANT_TELL:
                    break;
            }
        }
        catch (FileError&) {} //logfile may be only partly written -> this is no error!

        std::uint64_t fileSizeOld = 0;
        TickVal lastLifeSign = getTicks();

        while (true)
        {
            const TickVal now = getTicks();
            const std::uint64_t fileSizeNew = ::getLockFileSize(lockfilepath); //throw FileError

            if (TICKS_PER_SEC <= 0 || !lastLifeSign.isValid() || !now.isValid())
                throw FileError(L"System timer failed."); //no i18n: "should" never throw ;)

            if (fileSizeNew != fileSizeOld) //received life sign from lock
            {
                fileSizeOld  = fileSizeNew;
                lastLifeSign = now;
            }

            if (lockOwnderDead || //no need to wait any longer...
                dist(lastLifeSign, now) / TICKS_PER_SEC > DETECT_ABANDONED_INTERVAL)
            {
                DirLock dummy(deleteAbandonedLockName(lockfilepath), callback); //throw FileError

                //now that the lock is in place check existence again: meanwhile another process may have deleted and created a new lock!

                if (!originalLockId.empty())
                    if (retrieveLockId(lockfilepath) != originalLockId) //throw FileError -> since originalLockId is filled, we are not expecting errors!
                        return; //another process has placed a new lock, leave scope: the wait for the old lock is technically over...

                if (::getLockFileSize(lockfilepath) != fileSizeOld) //throw FileError
                    continue; //late life sign

                removeFile(lockfilepath); //throw FileError
                return;
            }

            //wait some time...
            static_assert(1000 * POLL_LIFE_SIGN_INTERVAL % GUI_CALLBACK_INTERVAL == 0, "");
            for (size_t i = 0; i < 1000 * POLL_LIFE_SIGN_INTERVAL / GUI_CALLBACK_INTERVAL; ++i)
            {
                if (callback) callback->requestUiRefresh();
#ifdef TODO_MinFFS_UI
                boost::this_thread::sleep(boost::posix_time::milliseconds(GUI_CALLBACK_INTERVAL));
#endif//TODO_MinFFS_UI

                if (callback)
                {
                    //one signal missed: it's likely this is an abandoned lock => show countdown
                    if (dist(lastLifeSign, now) / TICKS_PER_SEC > EMIT_LIFE_SIGN_INTERVAL)
                    {
                        const int remainingSeconds = std::max<int>(0, DETECT_ABANDONED_INTERVAL - dist(lastLifeSign, getTicks()) / TICKS_PER_SEC);
                        const std::wstring remSecMsg = replaceCpy(_P("1 sec", "%x sec", remainingSeconds), L"%x", toGuiString(remainingSeconds));
                        callback->reportStatus(infoMsg + L" | " + _("Detecting abandoned lock...") + L' ' + remSecMsg);
                    }
                    else
                        callback->reportStatus(infoMsg); //emit a message in any case (might clear other one)
                }
            }
        }
    }
    catch (FileError&)
    {
        if (!somethingExists(lockfilepath)) //a benign(?) race condition with FileError
            return; //what we are waiting for...
        throw;
    }
}


void releaseLock(const Zstring& lockfilepath) //throw ()
{
    try
    {
        removeFile(lockfilepath); //throw FileError
    }
    catch (FileError&) {}
}


bool tryLock(const Zstring& lockfilepath) //throw FileError
{
#ifdef ZEN_WIN
    const HANDLE fileHandle = ::CreateFile(applyLongPathPrefix(lockfilepath).c_str(),              //_In_      LPCTSTR lpFileName,
                                           //use both when writing over network, see comment in file_io.cpp
                                           GENERIC_READ | GENERIC_WRITE,                           //_In_      DWORD dwDesiredAccess,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                           nullptr,               //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                           CREATE_NEW,            //_In_      DWORD dwCreationDisposition,
                                           FILE_ATTRIBUTE_NORMAL, //_In_      DWORD dwFlagsAndAttributes,
                                           nullptr);              //_In_opt_  HANDLE hTemplateFile
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD lastError = ::GetLastError();
        if (lastError == ERROR_FILE_EXISTS || //confirmed to be used
            lastError == ERROR_ALREADY_EXISTS) //comment on msdn claims, this one is used on Windows Mobile 6
            return false;
        else
            throwFileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtFileName(lockfilepath)), L"CreateFile", lastError);
    }
    ScopeGuard guardLockFile = zen::makeGuard([&] { removeFile(lockfilepath); });
    FileOutput fileOut(fileHandle, lockfilepath); //pass handle ownership

    //be careful to avoid CreateFile() + CREATE_ALWAYS on a hidden file -> see file_io.cpp
    //=> we don't need it that badly //::SetFileAttributes(applyLongPathPrefix(lockfilepath).c_str(), FILE_ATTRIBUTE_HIDDEN); //(try to) hide it

#elif defined ZEN_LINUX || defined ZEN_MAC
    ::umask(0); //important! -> why?
    //O_EXCL contains a race condition on NFS file systems: http://linux.die.net/man/2/open
    const int fileHandle = ::open(lockfilepath.c_str(), O_CREAT | O_WRONLY | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fileHandle == -1)
    {
        if (errno == EEXIST)
            return false;
        else
            throwFileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtFileName(lockfilepath)), L"open", getLastError());
    }
    ScopeGuard guardLockFile = zen::makeGuard([&] { removeFile(lockfilepath); });
    FileOutputUnbuffered fileOut(fileHandle, lockfilepath); //pass handle ownership
#endif

    //write housekeeping info: user, process info, lock GUID
    BinaryStream binStream;
    {
        BinStreamOut streamOut;
        LockInformation(FromCurrentProcess()).toStream(streamOut);
        binStream = streamOut.get();
    }
    if (!binStream.empty())
        fileOut.write(&*binStream.begin(), binStream.size()); //throw FileError

    guardLockFile.dismiss(); //lockfile created successfully
    return true;
}
}


class DirLock::SharedDirLock
{
public:
    SharedDirLock(const Zstring& lockfilepath, DirLockCallback* callback) : //throw FileError
        lockfilepath_(lockfilepath)
    {
        while (!::tryLock(lockfilepath))             //throw FileError
            ::waitOnDirLock(lockfilepath, callback); //

#ifdef TODO_MinFFS_UI
        threadObj = boost::thread(LifeSigns(lockfilepath));
#endif//TODO_MinFFS_UI
    }

    ~SharedDirLock()
    {
#ifdef TODO_MinFFS_UI
        threadObj.interrupt(); //thread lifetime is subset of this instances's life
        threadObj.join(); //we assert precondition "threadObj.joinable()"!!!
#endif//TODO_MinFFS_UI

        ::releaseLock(lockfilepath_); //throw ()
    }

private:
    SharedDirLock           (const DirLock&) = delete;
    SharedDirLock& operator=(const DirLock&) = delete;

    const Zstring lockfilepath_;
#ifdef TODO_MinFFS_UI
    boost::thread threadObj;
#endif//TODO_MinFFS_UI
};


class DirLock::LockAdmin //administrate all locks held by this process to avoid deadlock by recursion
{
public:
    static LockAdmin& instance()
    {
        static LockAdmin inst;
        return inst;
    }

    //create or retrieve a SharedDirLock
    std::shared_ptr<SharedDirLock> retrieve(const Zstring& lockfilepath, DirLockCallback* callback) //throw FileError
    {
        tidyUp();

        //optimization: check if we already own a lock for this path
        auto iterGuid = fileToGuid.find(lockfilepath);
        if (iterGuid != fileToGuid.end())
            if (const std::shared_ptr<SharedDirLock>& activeLock = getActiveLock(iterGuid->second)) //returns null-lock if not found
                return activeLock; //SharedDirLock is still active -> enlarge circle of shared ownership

        try //check based on lock GUID, deadlock prevention: "lockfilepath" may be an alternative name for a lock already owned by this process
        {
            const std::string lockId = retrieveLockId(lockfilepath); //throw FileError
            if (const std::shared_ptr<SharedDirLock>& activeLock = getActiveLock(lockId)) //returns null-lock if not found
            {
                fileToGuid[lockfilepath] = lockId; //found an alias for one of our active locks
                return activeLock;
            }
        }
        catch (FileError&) {} //catch everything, let SharedDirLock constructor deal with errors, e.g. 0-sized/corrupted lock files

        //lock not owned by us => create a new one
        auto newLock = std::make_shared<SharedDirLock>(lockfilepath, callback); //throw FileError
        const std::string& newLockGuid = retrieveLockId(lockfilepath); //throw FileError

        //update registry
        fileToGuid[lockfilepath] = newLockGuid; //throw()
        guidToLock[newLockGuid]  = newLock;     //

        return newLock;
    }

private:
    LockAdmin() {}
    LockAdmin           (const LockAdmin&) = delete;
    LockAdmin& operator=(const LockAdmin&) = delete;

    typedef std::string UniqueId;
    typedef std::map<Zstring, UniqueId, LessFilename>        FileToGuidMap; //n:1 handle uppper/lower case correctly
    typedef std::map<UniqueId, std::weak_ptr<SharedDirLock>> GuidToLockMap; //1:1

    std::shared_ptr<SharedDirLock> getActiveLock(const UniqueId& lockId) //returns null if none found
    {
        auto iterLock = guidToLock.find(lockId);
        return iterLock != guidToLock.end() ? iterLock->second.lock() : nullptr; //try to get shared_ptr; throw()
    }

    void tidyUp() //remove obsolete lock entries
    {
        map_remove_if(guidToLock, [ ](const GuidToLockMap::value_type& v) { return !v.second.lock(); });
        map_remove_if(fileToGuid, [&](const FileToGuidMap::value_type& v) { return guidToLock.find(v.second) == guidToLock.end(); });
    }

    FileToGuidMap fileToGuid; //lockname |-> GUID; locks can be referenced by a lockfilepath or alternatively a GUID
    GuidToLockMap guidToLock; //GUID |-> "shared lock ownership"
};


DirLock::DirLock(const Zstring& lockfilepath, DirLockCallback* callback) //throw FileError
{
    if (callback)
        callback->reportStatus(replaceCpy(_("Creating file %x"), L"%x", fmtFileName(lockfilepath)));

#ifdef ZEN_WIN
    const DWORD bufferSize = 10000;
    std::vector<wchar_t> volName(bufferSize);
    if (::GetVolumePathName(lockfilepath.c_str(), //__in   LPCTSTR lpszFileName,
                            &volName[0],          //__out  LPTSTR lpszVolumePathName,
                            bufferSize))          //__in   DWORD cchBufferLength
    {
        DWORD dt = ::GetDriveType(&volName[0]);
        if (dt == DRIVE_CDROM)
            return; //we don't need a lock for a CD ROM
    }
#endif

    sharedLock = LockAdmin::instance().retrieve(lockfilepath, callback); //throw FileError
}
