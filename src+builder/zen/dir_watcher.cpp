// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "dir_watcher.h"
#include <algorithm>
#include <set>
#include "thread.h" //includes <boost/thread.hpp>
#include "scope_guard.h"

#ifdef ZEN_WIN
    #include "notify_removal.h"
    #include "win.h" //includes "windows.h"
    #include "long_path_prefix.h"

#elif defined ZEN_LINUX
    #include <sys/inotify.h>
    #include <fcntl.h>
    #include "file_traverser.h"

#elif defined ZEN_MAC
    #include <CoreServices/CoreServices.h>
    #include "osx_string.h"
#endif

using namespace zen;


#ifdef ZEN_WIN
namespace
{
class SharedData
{
public:
    //context of worker thread
    void addChanges(const char* buffer, DWORD bytesWritten, const Zstring& dirpath) //throw ()
    {
        boost::lock_guard<boost::mutex> dummy(lockAccess);

        if (bytesWritten == 0) //according to docu this may happen in case of internal buffer overflow: report some "dummy" change
            changedFiles.emplace_back(DirWatcher::ACTION_CREATE, L"Overflow.");
        else
        {
            const char* bufPos = &buffer[0];
            for (;;)
            {
                const FILE_NOTIFY_INFORMATION& notifyInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION&>(*bufPos);

                const Zstring fullpath = dirpath + Zstring(notifyInfo.FileName, notifyInfo.FileNameLength / sizeof(WCHAR));

                [&]
                {
                    //skip modifications sent by changed directories: reason for change, child element creation/deletion, will notify separately!
                    //and if this child element is a .ffs_lock file we'll want to ignore all associated events!
                    if (notifyInfo.Action == FILE_ACTION_MODIFIED)
                    {
                        //note: this check will not work if top watched directory has been renamed
                        const DWORD ret = ::GetFileAttributes(applyLongPathPrefix(fullpath).c_str());
                        if (ret != INVALID_FILE_ATTRIBUTES && (ret & FILE_ATTRIBUTE_DIRECTORY)) //returns true for (dir-)symlinks also
                            return;
                    }

                    //note: a move across directories will show up as FILE_ACTION_ADDED/FILE_ACTION_REMOVED!
                    switch (notifyInfo.Action)
                    {
                        case FILE_ACTION_ADDED:
                        case FILE_ACTION_RENAMED_NEW_NAME: //harmonize with "move" which is notified as "create + delete"
                            changedFiles.emplace_back(DirWatcher::ACTION_CREATE, fullpath);
                            break;
                        case FILE_ACTION_REMOVED:
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            changedFiles.emplace_back(DirWatcher::ACTION_DELETE, fullpath);
                            break;
                        case FILE_ACTION_MODIFIED:
                            changedFiles.emplace_back(DirWatcher::ACTION_UPDATE, fullpath);
                            break;
                    }
                }();

                if (notifyInfo.NextEntryOffset == 0)
                    break;
                bufPos += notifyInfo.NextEntryOffset;
            }
        }
    }

    ////context of main thread
    //void addChange(const Zstring& dirpath) //throw ()
    //{
    //    boost::lock_guard<boost::mutex> dummy(lockAccess);
    //    changedFiles.insert(dirpath);
    //}


    //context of main thread
    void fetchChanges(std::vector<DirWatcher::Entry>& output) //throw FileError
    {
        boost::lock_guard<boost::mutex> dummy(lockAccess);

        //first check whether errors occurred in thread
        if (errorInfo)
        {
            const std::wstring msg   = copyStringTo<std::wstring>(errorInfo->msg);
            const std::wstring descr = copyStringTo<std::wstring>(errorInfo->descr);
            throw FileError(msg, descr);
        }

        output.swap(changedFiles);
        changedFiles.clear();
    }


    //context of worker thread
    void reportError(const std::wstring& msg, const std::wstring& description, DWORD errorCode) //throw()
    {
        boost::lock_guard<boost::mutex> dummy(lockAccess);

        ErrorInfo newInfo = { copyStringTo<BasicWString>(msg), copyStringTo<BasicWString>(description), errorCode };
        errorInfo = make_unique<ErrorInfo>(newInfo);
    }

private:
    typedef Zbase<wchar_t> BasicWString; //thread safe string class for UI texts

    boost::mutex lockAccess;
    std::vector<DirWatcher::Entry> changedFiles;

    struct ErrorInfo
    {
        BasicWString msg;
        BasicWString descr;
        DWORD errorCode;
    };
    std::unique_ptr<ErrorInfo> errorInfo; //non-empty if errors occurred in thread
};


class ReadChangesAsync
{
public:
    //constructed in main thread!
    ReadChangesAsync(const Zstring& directory, //make sure to not leak-in thread-unsafe types!
                     const std::shared_ptr<SharedData>& shared) :
        shared_(shared),
        dirpathPf(appendSeparator(directory)),
        hDir(INVALID_HANDLE_VALUE)
    {
        hDir = ::CreateFile(applyLongPathPrefix(dirpathPf).c_str(),                 //_In_      LPCTSTR lpFileName,
                            FILE_LIST_DIRECTORY,                                    //_In_      DWORD dwDesiredAccess,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                            nullptr,              //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                            OPEN_EXISTING,        //_In_      DWORD dwCreationDisposition,
                            FILE_FLAG_BACKUP_SEMANTICS |
                            FILE_FLAG_OVERLAPPED, //_In_      DWORD dwFlagsAndAttributes,
                            nullptr);             //_In_opt_  HANDLE hTemplateFile
        if (hDir == INVALID_HANDLE_VALUE)
            throwFileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"CreateFile", getLastError());

        //end of constructor, no need to start managing "hDir"
    }

    ReadChangesAsync(ReadChangesAsync&& other) :
        hDir(INVALID_HANDLE_VALUE)
    {
        shared_   = std::move(other.shared_);
        dirpathPf = std::move(other.dirpathPf);
        std::swap(hDir, other.hDir);
    }

    ~ReadChangesAsync()
    {
        if (hDir != INVALID_HANDLE_VALUE) //valid hDir is NOT an invariant, see move constructor!
            ::CloseHandle(hDir);
    }

    void operator()() //thread entry
    {
        try
        {
            std::vector<char> buffer(64 * 1024); //needs to be aligned on a DWORD boundary; maximum buffer size restricted by some networks protocols (according to docu)

            for (;;)
            {
                boost::this_thread::interruption_point();

                //actual work
                OVERLAPPED overlapped = {};
                overlapped.hEvent = ::CreateEvent(nullptr,  //__in_opt  LPSECURITY_ATTRIBUTES lpEventAttributes,
                                                  true,     //__in      BOOL bManualReset,
                                                  false,    //__in      BOOL bInitialState,
                                                  nullptr); //__in_opt  LPCTSTR lpName
                if (overlapped.hEvent == nullptr)
                {
                    const DWORD ec = ::GetLastError(); //copy before directly or indirectly making other system calls!
                    return shared_->reportError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(dirpathPf)), formatSystemError(L"CreateEvent", ec), ec);
                }
                ZEN_ON_SCOPE_EXIT(::CloseHandle(overlapped.hEvent));

                DWORD bytesReturned = 0; //should not be needed for async calls, still pass it to help broken drivers

                //asynchronous variant: runs on this thread's APC queue!
                if (!::ReadDirectoryChangesW(hDir,                              //  __in   HANDLE hDirectory,
                                             &buffer[0],                        //  __out  LPVOID lpBuffer,
                                             static_cast<DWORD>(buffer.size()), //  __in   DWORD nBufferLength,
                                             true,                              //  __in   BOOL bWatchSubtree,
                                             FILE_NOTIFY_CHANGE_FILE_NAME |
                                             FILE_NOTIFY_CHANGE_DIR_NAME  |
                                             FILE_NOTIFY_CHANGE_SIZE      |
                                             FILE_NOTIFY_CHANGE_LAST_WRITE, //  __in         DWORD dwNotifyFilter,
                                             &bytesReturned,                //  __out_opt    LPDWORD lpBytesReturned,
                                             &overlapped,                   //  __inout_opt  LPOVERLAPPED lpOverlapped,
                                             nullptr))                      //  __in_opt     LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
                {
                    const DWORD ec = ::GetLastError(); //copy before directly or indirectly making other system calls!
                    return shared_->reportError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(dirpathPf)), formatSystemError(L"ReadDirectoryChangesW", ec), ec);
                }

                //async I/O is a resource that needs to be guarded since it will write to local variable "buffer"!
                zen::ScopeGuard guardAio = zen::makeGuard([&]
                {
                    //Canceling Pending I/O Operations: http://msdn.microsoft.com/en-us/library/aa363789(v=vs.85).aspx
                    //if (::CancelIoEx(hDir, &overlapped) /*!= FALSE*/ || ::GetLastError() != ERROR_NOT_FOUND) -> Vista only
                    if (::CancelIo(hDir) /*!= FALSE*/ || ::GetLastError() != ERROR_NOT_FOUND)
                    {
                        DWORD bytesWritten = 0;
                        ::GetOverlappedResult(hDir, &overlapped, &bytesWritten, true); //wait until cancellation is complete
                    }
                });

                //wait for results
                DWORD bytesWritten = 0;
                while (!::GetOverlappedResult(hDir,          //__in   HANDLE hFile,
                                              &overlapped,   //__in   LPOVERLAPPED lpOverlapped,
                                              &bytesWritten, //__out  LPDWORD lpNumberOfBytesTransferred,
                                              false))        //__in   BOOL bWait
                {
                    const DWORD ec = ::GetLastError(); //copy before directly or indirectly making other system calls!
                    if (ec != ERROR_IO_INCOMPLETE)
                        return shared_->reportError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(dirpathPf)), formatSystemError(L"GetOverlappedResult", ec), ec);

                    //execute asynchronous procedure calls (APC) queued on this thread
                    ::SleepEx(50,    // __in  DWORD dwMilliseconds,
                              true); // __in  BOOL bAlertable

                    boost::this_thread::interruption_point();
                }
                guardAio.dismiss();

                shared_->addChanges(&buffer[0], bytesWritten, dirpathPf); //throw ()
            }
        }
        catch (boost::thread_interrupted&)
        {
            throw; //this is the only exception expected!
        }
    }

    HANDLE getDirHandle() const { return hDir; } //for reading/monitoring purposes only, don't abuse (e.g. close handle)!

private:
    ReadChangesAsync           (const ReadChangesAsync&) = delete;
    ReadChangesAsync& operator=(const ReadChangesAsync&) = delete;

    //shared between main and worker:
    std::shared_ptr<SharedData> shared_;
    //worker thread only:
    Zstring dirpathPf; //thread safe!
    HANDLE hDir;
};


class HandleVolumeRemoval : public NotifyRequestDeviceRemoval
{
public:
    HandleVolumeRemoval(HANDLE hDir,
                        boost::thread& worker) :
        NotifyRequestDeviceRemoval(hDir), //throw FileError
        worker_(worker),
        removalRequested(false),
        operationComplete(false) {}

    //all functions are called by main thread!

    bool requestReceived() const { return removalRequested; }
    bool finished() const { return operationComplete; }

private:
    void onRequestRemoval(HANDLE hnd) override
    {
        //must release hDir immediately => stop monitoring!
        if (worker_.joinable()) //= join() precondition: play safe; can't trust Windows to only call-back once
        {
            worker_.interrupt();
            worker_.join(); //we assume precondition "worker.joinable()"!!!
            //now hDir should have been released
        }

        removalRequested = true;
    } //don't throw!

    void onRemovalFinished(HANDLE hnd, bool successful) override { operationComplete = true; } //throw()!

    boost::thread& worker_;
    bool removalRequested;
    bool operationComplete;
};
}


struct DirWatcher::Pimpl
{
    boost::thread worker;
    std::shared_ptr<SharedData> shared;

    Zstring dirpath;
    std::unique_ptr<HandleVolumeRemoval> volRemoval;
};


DirWatcher::DirWatcher(const Zstring& directory) : //throw FileError
    pimpl_(zen::make_unique<Pimpl>())
{
    pimpl_->shared = std::make_shared<SharedData>();
    pimpl_->dirpath = directory;

    ReadChangesAsync reader(directory, pimpl_->shared); //throw FileError
    pimpl_->volRemoval = zen::make_unique<HandleVolumeRemoval>(reader.getDirHandle(), pimpl_->worker); //throw FileError
    pimpl_->worker = boost::thread(std::move(reader));
}


DirWatcher::~DirWatcher()
{
    if (pimpl_->worker.joinable()) //= thread::detach() precondition! -> may already be joined by HandleVolumeRemoval::onRequestRemoval()
    {
        pimpl_->worker.interrupt();
        //if (pimpl_->worker.joinable()) pimpl_->worker.join(); -> we don't have time to wait... will take ~50ms anyway
        pimpl_->worker.detach(); //we have to be explicit since C++11: [thread.thread.destr] ~thread() calls std::terminate() if joinable()!!!
    }

    //caveat: exitting the app may simply kill this thread!
}


std::vector<DirWatcher::Entry> DirWatcher::getChanges(const std::function<void()>& processGuiMessages) //throw FileError
{
    std::vector<Entry> output;

    //wait until device removal is confirmed, to prevent locking hDir again by some new watch!
    if (pimpl_->volRemoval->requestReceived())
    {
        const boost::system_time maxwait = boost::get_system_time() + boost::posix_time::seconds(15);
        //HandleVolumeRemoval::finished() not guaranteed! note: Windows gives unresponsive applications ca. 10 seconds until unmounting the usb stick in worst case

        while (!pimpl_->volRemoval->finished() && boost::get_system_time() < maxwait)
        {
            processGuiMessages(); //DBT_DEVICEREMOVECOMPLETE message is sent here!
            boost::thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
        }

        output.emplace_back(ACTION_DELETE, pimpl_->dirpath); //report removal as change to main directory
    }
    else //the normal case...
        pimpl_->shared->fetchChanges(output); //throw FileError
    return output;
}


#elif defined ZEN_LINUX
struct DirWatcher::Pimpl
{
    Pimpl() : notifDescr() {}

    Zstring basedirpath;
    int notifDescr;
    std::map<int, Zstring> watchDescrs; //watch descriptor and (sub-)directory name (postfixed with separator) -> owned by "notifDescr"
};


DirWatcher::DirWatcher(const Zstring& directory) : //throw FileError
    pimpl_(zen::make_unique<Pimpl>())
{
    //get all subdirectories
    Zstring dirpathFmt = directory;
    if (endsWith(dirpathFmt, FILE_NAME_SEPARATOR))
        dirpathFmt.resize(dirpathFmt.size() - 1);

    std::vector<Zstring> fullDirList { dirpathFmt };

traverseFolder(dirpathFmt, nullptr, 
				[&](const DirInfo& di ){ fullDirList.push_back(di.fullPath); }, 
				nullptr, //don't traverse into symlinks (analog to windows build)
[&](const std::wstring& errorMsg){ throw FileError(errorMsg); });

    //init
    pimpl_->basedirpath = directory;
    pimpl_->notifDescr = ::inotify_init();
    if (pimpl_->notifDescr == -1)
        throwFileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"inotify_init", getLastError());

    zen::ScopeGuard guardDescr = zen::makeGuard([&] { ::close(pimpl_->notifDescr); });

    //set non-blocking mode
    bool initSuccess = false;
    {
        int flags = ::fcntl(pimpl_->notifDescr, F_GETFL);
        if (flags != -1)
            initSuccess = ::fcntl(pimpl_->notifDescr, F_SETFL, flags | O_NONBLOCK) != -1;
    }
    if (!initSuccess)
        throwFileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"fcntl", getLastError());

    //add watches
    for (const Zstring& subdir : fullDirList)
    {
        int wd = ::inotify_add_watch(pimpl_->notifDescr, subdir.c_str(),
                                     IN_ONLYDIR     | //"Only watch pathname if it is a directory."
                                     IN_DONT_FOLLOW | //don't follow symbolic links
                                     IN_CREATE   	|
                                     IN_MODIFY 	    |
                                     IN_CLOSE_WRITE |
                                     IN_DELETE 	    |
                                     IN_DELETE_SELF |
                                     IN_MOVED_FROM  |
                                     IN_MOVED_TO    |
                                     IN_MOVE_SELF);
        if (wd == -1)
        {
            const auto ec = getLastError();
            if (ec == ENOSPC) //fix misleading system message "No space left on device"
                throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(subdir)), formatSystemError(L"inotify_add_watch", ec, L"The user limit on the total number of inotify watches was reached or the kernel failed to allocate a needed resource."));

            throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(subdir)), formatSystemError(L"inotify_add_watch", ec));
        }

        pimpl_->watchDescrs.emplace(wd, appendSeparator(subdir));
    }

    guardDescr.dismiss();
}


DirWatcher::~DirWatcher()
{
    ::close(pimpl_->notifDescr); //associated watches are removed automatically!
}


std::vector<DirWatcher::Entry> DirWatcher::getChanges(const std::function<void()>&) //throw FileError
{
    std::vector<char> buffer(512 * (sizeof(struct ::inotify_event) + NAME_MAX + 1));

    ssize_t bytesRead = 0;
    do
    {
        //non-blocking call, see O_NONBLOCK
        bytesRead = ::read(pimpl_->notifDescr, &buffer[0], buffer.size());
    }
    while (bytesRead < 0 && errno == EINTR); //"Interrupted function call; When this happens, you should try the call again."

    if (bytesRead < 0)
    {
        if (errno == EAGAIN)  //this error is ignored in all inotify wrappers I found
            return std::vector<Entry>();

        throwFileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(pimpl_->basedirpath)), L"read", getLastError());
    }

    std::vector<Entry> output;

    ssize_t bytePos = 0;
    while (bytePos < bytesRead)
    {
        struct ::inotify_event& evt = reinterpret_cast<struct ::inotify_event&>(buffer[bytePos]);

        if (evt.len != 0) //exclude case: deletion of "self", already reported by parent directory watch
        {
            auto it = pimpl_->watchDescrs.find(evt.wd);
            if (it != pimpl_->watchDescrs.end())
            {
                //Note: evt.len is NOT the size of the evt.name c-string, but the array size including all padding 0 characters!
                //It may be even 0 in which case evt.name must not be used!
                const Zstring fullname = it->second + evt.name;

                if ((evt.mask & IN_CREATE) ||
                    (evt.mask & IN_MOVED_TO))
                    output.emplace_back(ACTION_CREATE, fullname);
                else if ((evt.mask & IN_MODIFY) ||
                         (evt.mask & IN_CLOSE_WRITE))
                    output.emplace_back(ACTION_UPDATE, fullname);
                else if ((evt.mask & IN_DELETE     ) ||
                         (evt.mask & IN_DELETE_SELF) ||
                         (evt.mask & IN_MOVE_SELF  ) ||
                         (evt.mask & IN_MOVED_FROM))
                    output.emplace_back(ACTION_DELETE, fullname);
            }
        }
        bytePos += sizeof(struct ::inotify_event) + evt.len;
    }

    return output;
}

#elif defined ZEN_MAC
namespace
{
void eventCallback(ConstFSEventStreamRef streamRef,
                   void* clientCallBackInfo,
                   size_t numEvents,
                   void* eventPaths,
                   const FSEventStreamEventFlags eventFlags[],
                   const FSEventStreamEventId eventIds[])
{
    std::vector<DirWatcher::Entry>& changedFiles = *static_cast<std::vector<DirWatcher::Entry>*>(clientCallBackInfo);

    auto paths = static_cast<const char**>(eventPaths);
    for (size_t i = 0; i < numEvents; ++i)
    {
        //::printf("0x%08x\t%s\n", static_cast<unsigned int>(eventFlags[i]), paths[i]);

        //events are aggregated => it's possible to see a single event with flags
        //kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemRemoved

		//https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/index.html#//apple_ref/doc/constant_group/FSEventStreamEventFlags
        if (eventFlags[i] & kFSEventStreamEventFlagItemCreated ||
            eventFlags[i] & kFSEventStreamEventFlagMount)
            changedFiles.emplace_back(DirWatcher::ACTION_CREATE, paths[i]);
        if (eventFlags[i] & kFSEventStreamEventFlagItemModified      || //
            eventFlags[i] & kFSEventStreamEventFlagItemXattrMod      || //
            eventFlags[i] & kFSEventStreamEventFlagItemChangeOwner   || //aggregate these into a single event
            eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod  || //
            eventFlags[i] & kFSEventStreamEventFlagItemFinderInfoMod || //
            eventFlags[i] & kFSEventStreamEventFlagItemRenamed       || //OS X sends the same event flag for both old and new names!!!
            eventFlags[i] & kFSEventStreamEventFlagMustScanSubDirs)  //something changed in one of the subdirs: NOT expected due to kFSEventStreamCreateFlagFileEvents
            changedFiles.emplace_back(DirWatcher::ACTION_UPDATE, paths[i]);
        if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved ||
            eventFlags[i] & kFSEventStreamEventFlagRootChanged || //root is (indirectly) deleted or renamed
            eventFlags[i] & kFSEventStreamEventFlagUnmount)
            changedFiles.emplace_back(DirWatcher::ACTION_DELETE, paths[i]);

        //kFSEventStreamEventFlagEventIdsWrapped -> irrelevant!
        //kFSEventStreamEventFlagHistoryDone -> not expected due to kFSEventStreamEventIdSinceNow below
    }
}
}


struct DirWatcher::Pimpl
{
    Pimpl() : eventStream() {}
    FSEventStreamRef eventStream;
    std::vector<DirWatcher::Entry> changedFiles;
};


DirWatcher::DirWatcher(const Zstring& directory) :
    pimpl_(zen::make_unique<Pimpl>())
{
    CFStringRef dirpathCf = osx::createCFString(directory.c_str()); //returns nullptr on error
    if (!dirpathCf)
        throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"Function call failed: createCFString"); //no error code documented!
    ZEN_ON_SCOPE_EXIT(::CFRelease(dirpathCf));

    CFArrayRef dirpathCfArray = ::CFArrayCreate(nullptr,    //CFAllocatorRef allocator,
                                                reinterpret_cast<const void**>(&dirpathCf), //const void** values,
                                                1,          //CFIndex numValues,
                                                nullptr);   //const CFArrayCallBacks* callBacks
    if (!dirpathCfArray)
        throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"Function call failed: CFArrayCreate"); //no error code documented!
    ZEN_ON_SCOPE_EXIT(::CFRelease(dirpathCfArray));

    FSEventStreamContext context = {};
    context.info = &pimpl_->changedFiles;

    pimpl_->eventStream = ::FSEventStreamCreate(nullptr,        //CFAllocatorRef allocator,
                                                &eventCallback, //FSEventStreamCallback callback,
                                                &context,       //FSEventStreamContext* context,
                                                dirpathCfArray, //CFArrayRef pathsToWatch,
                                                kFSEventStreamEventIdSinceNow, //FSEventStreamEventId sinceWhen,
                                                0,              //CFTimeInterval latency, in seconds
                                                kFSEventStreamCreateFlagWatchRoot |
                                                kFSEventStreamCreateFlagFileEvents); //FSEventStreamCreateFlags flags
    //can this fail?? not documented!

    zen::ScopeGuard guardCreate = zen::makeGuard([&] { ::FSEventStreamRelease(pimpl_->eventStream); });

    ::FSEventStreamScheduleWithRunLoop(pimpl_->eventStream,     //FSEventStreamRef streamRef,
                                       ::CFRunLoopGetCurrent(), //CFRunLoopRef runLoop;  CFRunLoopGetCurrent(): failure not documented!
                                       kCFRunLoopDefaultMode);  //CFStringRef runLoopMode
    //no-fail

    zen::ScopeGuard guardRunloop = zen::makeGuard([&] { ::FSEventStreamInvalidate(pimpl_->eventStream); });

    if (!::FSEventStreamStart(pimpl_->eventStream))
        throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtFileName(directory)), L"Function call failed: FSEventStreamStart"); //no error code documented!

    guardCreate .dismiss();
    guardRunloop.dismiss();
}


DirWatcher::~DirWatcher()
{
    ::FSEventStreamStop      (pimpl_->eventStream);
    ::FSEventStreamInvalidate(pimpl_->eventStream);
    ::FSEventStreamRelease   (pimpl_->eventStream);
}


std::vector<DirWatcher::Entry> DirWatcher::getChanges(const std::function<void()>&)
{
    ::FSEventStreamFlushSync(pimpl_->eventStream); //flushes pending events + execs runloop

    std::vector<DirWatcher::Entry> changes;
    changes.swap(pimpl_->changedFiles);
    return changes;
}
#endif
