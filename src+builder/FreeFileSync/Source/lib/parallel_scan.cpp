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

#include "parallel_scan.h"
#include <zen/file_traverser.h>
#include <zen/file_error.h>
#include <zen/thread.h> //includes <boost/thread.hpp>
#include <zen/scope_guard.h>
#include <zen/fixed_list.h>
#include <boost/detail/atomic_count.hpp>
#include "db_file.h"
#include "lock_holder.h"

using namespace zen;


namespace
{
/*
#ifdef ZEN_WIN

struct DiskInfo
{
    DiskInfo() :
        driveType(DRIVE_UNKNOWN),
        diskID(-1) {}

    UINT driveType;
    int diskID; // -1 if id could not be determined, this one is filled if driveType == DRIVE_FIXED or DRIVE_REMOVABLE;
};

inline
bool operator<(const DiskInfo& lhs, const DiskInfo& rhs)
{
    if (lhs.driveType != rhs.driveType)
        return lhs.driveType < rhs.driveType;

    if (lhs.diskID < 0 || rhs.diskID < 0)
        return false;
    //consider "same", reason: one volume may be uniquely associated with one disk, while the other volume is associated to the same disk AND another one!
    //volume <-> disk is 0..N:1..N

    return lhs.diskID < rhs.diskID ;
}


DiskInfo retrieveDiskInfo(const Zstring& pathName)
{
    std::vector<wchar_t> volName(std::max(pathName.size(), static_cast<size_t>(10000)));

    DiskInfo output;

    //full pathName need not yet exist!
    if (!::GetVolumePathName(pathName.c_str(),  //__in   LPCTSTR lpszFileName,
                             &volName[0],       //__out  LPTSTR lpszVolumePathName,
                             static_cast<DWORD>(volName.size()))) //__in   DWORD cchBufferLength
        return output;

    const Zstring rootPathName = &volName[0];

    output.driveType = ::GetDriveType(rootPathName.c_str());

    if (output.driveType == DRIVE_NO_ROOT_DIR) //these two should be the same error category
        output.driveType = DRIVE_UNKNOWN;

    if (output.driveType != DRIVE_FIXED && output.driveType != DRIVE_REMOVABLE)
        return output; //no reason to get disk ID

    //go and find disk id:

    //format into form: "\\.\C:"
    Zstring volnameFmt = rootPathName;
    if (endsWith(volnameFmt, FILE_NAME_SEPARATOR))
        volnameFmt.resize(volnameFmt.size() - 1);
    volnameFmt = L"\\\\.\\" + volnameFmt;

    HANDLE hVolume = ::CreateFile(volnameFmt.c_str(),
                                  0,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
    if (hVolume == INVALID_HANDLE_VALUE)
        return output;
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hVolume));

    std::vector<char> buffer(sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT)); //reserve buffer for at most one disk! call below will then fail if volume spans multiple disks!

    DWORD bytesReturned = 0;
    if (!::DeviceIoControl(hVolume,                              //_In_         HANDLE hDevice,
                           IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, //_In_         DWORD dwIoControlCode,
                           nullptr,                              //_In_opt_     LPVOID lpInBuffer,
                           0,                                    //_In_         DWORD nInBufferSize,
                           &buffer[0],                           //_Out_opt_    LPVOID lpOutBuffer,
                           static_cast<DWORD>(buffer.size()),    //_In_         DWORD nOutBufferSize,
                           &bytesReturned,                       //_Out_opt_    LPDWORD lpBytesReturned
                           nullptr))                             //_Inout_opt_  LPOVERLAPPED lpOverlapped
        return output;

    const VOLUME_DISK_EXTENTS& volDisks = *reinterpret_cast<VOLUME_DISK_EXTENTS*>(&buffer[0]);

    if (volDisks.NumberOfDiskExtents != 1)
        return output;

    output.diskID = volDisks.Extents[0].DiskNumber;

    return output;
}
#endif
*/

/*
PERF NOTE

--------------------------------------------
|Testcase: Reading from two different disks|
--------------------------------------------
Windows 7:
            1st(unbuffered) |2nd (OS buffered)
			----------------------------------
1 Thread:          57s      |        8s
2 Threads:         39s      |        7s

--------------------------------------------------
|Testcase: Reading two directories from same disk|
--------------------------------------------------
Windows 7:                                        Windows XP:
            1st(unbuffered) |2nd (OS buffered)                   1st(unbuffered) |2nd (OS buffered)
			----------------------------------                   ----------------------------------
1 Thread:          41s      |        13s             1 Thread:          45s      |        13s
2 Threads:         42s      |        11s             2 Threads:         38s      |         8s

=> Traversing does not take any advantage of file locality so that even multiple threads operating on the same disk impose no performance overhead! (even faster on XP)

std::vector<std::set<DirectoryKey>> separateByDistinctDisk(const std::set<DirectoryKey>& dirkeys)
{
    //use one thread per physical disk:
    typedef std::map<DiskInfo, std::set<DirectoryKey>> DiskKeyMapping;
    DiskKeyMapping tmp;
    std::for_each(dirkeys.begin(), dirkeys.end(),
    [&](const DirectoryKey& key) { tmp[retrieveDiskInfo(key.dirpathFull_)].insert(key); });

    std::vector<std::set<DirectoryKey>> buckets;
    std::transform(tmp.begin(), tmp.end(), std::back_inserter(buckets),
    [&](const DiskKeyMapping::value_type& diskToKey) { return diskToKey.second; });
    return buckets;
}
*/

//------------------------------------------------------------------------------------------
typedef Zbase<wchar_t, StorageRefCountThreadSafe> BasicWString; //thread-safe string class for UI texts


class AsyncCallback //actor pattern
{
public:
    AsyncCallback() :
        notifyingThreadID(0),
        textScanning(_("Scanning:")),
        itemsScanned(0),
        activeWorker(0) {}

    FillBufferCallback::HandleError reportError(const std::wstring& msg, size_t retryNumber) //blocking call: context of worker thread
    {
#ifdef TODO_MinFFS_UI
        boost::unique_lock<boost::mutex> dummy(lockErrorInfo);
        while (errorInfo.get() || errorResponse.get())
            conditionCanReportError.timed_wait(dummy, boost::posix_time::milliseconds(50)); //interruption point!
#endif//TODO_MinFFS_UI

        errorInfo = make_unique<std::pair<BasicWString, size_t>>(BasicWString(msg), retryNumber);

#ifdef TODO_MinFFS_UI
        while (!errorResponse.get())
            conditionGotResponse.timed_wait(dummy, boost::posix_time::milliseconds(50)); //interruption point!
#endif//TODO_MinFFS_UI

        FillBufferCallback::HandleError rv = *errorResponse;

        errorInfo.reset();
        errorResponse.reset();

#ifdef TODO_MinFFS_UI
        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionCanReportError.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
#endif//TODO_MinFFS_UI

        return rv;
    }

    void processErrors(FillBufferCallback& callback) //context of main thread, call repreatedly
    {
#ifdef TODO_MinFFS_UI
        boost::unique_lock<boost::mutex> dummy(lockErrorInfo);
        if (errorInfo.get() && !errorResponse.get())
        {
            FillBufferCallback::HandleError rv = callback.reportError(copyStringTo<std::wstring>(errorInfo->first), errorInfo->second); //throw!
            errorResponse = make_unique<FillBufferCallback::HandleError>(rv);

            dummy.unlock(); //optimization for condition_variable::notify_all()
            conditionGotResponse.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        }
#endif//TODO_MinFFS_UI
    }

    void incrementNotifyingThreadId() { ++notifyingThreadID; } //context of main thread

    void reportCurrentFile(const Zstring& filepath, long threadID) //context of worker thread
    {
        if (threadID != notifyingThreadID) return; //only one thread at a time may report status

#ifdef TODO_MinFFS_UI
        boost::lock_guard<boost::mutex> dummy(lockCurrentStatus);
#endif//TODO_MinFFS_UI
        currentFile = filepath;
        currentStatus.clear();
    }

    void reportCurrentStatus(const std::wstring& status, long threadID) //context of worker thread
    {
        if (threadID != notifyingThreadID) return; //only one thread may report status

#ifdef TODO_MinFFS_UI
        boost::lock_guard<boost::mutex> dummy(lockCurrentStatus);
#endif//TODO_MinFFS_UI
        currentFile.clear();
        currentStatus = BasicWString(status); //we cannot assume std::wstring to be thread safe (yet)!
    }

    std::wstring getCurrentStatus() //context of main thread, call repreatedly
    {
        Zstring filepath;
        std::wstring statusMsg;
        {
#ifdef TODO_MinFFS_UI
            boost::lock_guard<boost::mutex> dummy(lockCurrentStatus);
#endif//TODO_MinFFS_UI
            if (!currentFile.empty())
                filepath = currentFile;
            else if (!currentStatus.empty())
                statusMsg = copyStringTo<std::wstring>(currentStatus);
        }

        if (!filepath.empty())
        {
            std::wstring statusText = copyStringTo<std::wstring>(textScanning);
            const long activeCount = activeWorker;
            if (activeCount >= 2)
                statusText += L" [" + replaceCpy(_P("1 thread", "%x threads", activeCount), L"%x", numberTo<std::wstring>(activeCount)) + L"]";

            statusText += L" " + fmtFileName(filepath);
            return statusText;
        }
        else
            return statusMsg;
    }

    void incItemsScanned() { ++itemsScanned; } //perf: irrelevant! scanning is almost entirely file I/O bound, not CPU bound! => no prob having multiple threads poking at the same variable!
    long getItemsScanned() const { return itemsScanned; }

    void incActiveWorker() { ++activeWorker; }
    void decActiveWorker() { --activeWorker; }
    long getActiveWorker() const { return activeWorker; }

private:
    //---- error handling ----
#ifdef TODO_MinFFS_UI
    boost::mutex lockErrorInfo;
    boost::condition_variable conditionCanReportError;
    boost::condition_variable conditionGotResponse;
#endif//TODO_MinFFS_UI
    std::unique_ptr<std::pair<BasicWString, size_t>> errorInfo; //error message + retry number
    std::unique_ptr<FillBufferCallback::HandleError> errorResponse;

    //---- status updates ----
    boost::detail::atomic_count notifyingThreadID;
    //CAVEAT: do NOT use boost::thread::id as long as this showstopper exists: https://svn.boost.org/trac/boost/ticket/5754
#ifdef TODO_MinFFS_UI
    boost::mutex lockCurrentStatus; //use a different lock for current file: continue traversing while some thread may process an error
#endif//TODO_MinFFS_UI
    Zstring currentFile;        //only one of these two is filled at a time!
    BasicWString currentStatus; //

    const BasicWString textScanning; //this one is (currently) not shared and could be made a std::wstring, but we stay consistent and use thread-safe variables in this class only!

    //---- status updates II (lock free) ----
    boost::detail::atomic_count itemsScanned;
    boost::detail::atomic_count activeWorker;
};

//-------------------------------------------------------------------------------------------------

struct TraverserShared
{
public:
    TraverserShared(long threadID,
                    SymLinkHandling handleSymlinks,
                    const HardFilter::FilterRef& filter,
                    std::set<Zstring>& failedDirReads,
                    std::set<Zstring>& failedItemReads,
                    AsyncCallback& acb) :
        handleSymlinks_(handleSymlinks),
        filterInstance(filter),
        failedDirReads_(failedDirReads),
        failedItemReads_(failedItemReads),
        acb_(acb),
        threadID_(threadID) {}

    const SymLinkHandling handleSymlinks_;
    const HardFilter::FilterRef filterInstance; //always bound!

    std::set<Zstring>& failedDirReads_;
    std::set<Zstring>& failedItemReads_;

    AsyncCallback& acb_;
    const long threadID_;
};


class DirCallback : public zen::TraverseCallback
{
public:
    DirCallback(TraverserShared& config,
                const Zstring& relNameParentPf, //postfixed with FILE_NAME_SEPARATOR!
                DirContainer& output) :
        cfg(config),
        relNameParentPf_(relNameParentPf),
        output_(output) {}

    void        onFile   (const Zchar* shortName, const Zstring& filepath, const FileInfo&    details) override;
    HandleLink  onSymlink(const Zchar* shortName, const Zstring& linkpath, const SymlinkInfo& details) override;
    TraverseCallback* onDir(const Zchar* shortName, const Zstring& dirpath)                            override;
    void releaseDirTraverser(TraverseCallback* trav)                                                   override;

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                           override;
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zchar* shortName)   override;

private:
    TraverserShared& cfg;
    const Zstring relNameParentPf_;
    DirContainer& output_;
};


void DirCallback::onFile(const Zchar* shortName, const Zstring& filepath, const FileInfo& details)
{
#ifdef TODO_MinFFS_UI
    boost::this_thread::interruption_point();
#endif//TODO_MinFFS_UI

    const Zstring fileNameShort(shortName);

    //do not list the database file(s) sync.ffs_db, sync.x64.ffs_db, etc. or lock files
    if (endsWith(fileNameShort, SYNC_DB_FILE_ENDING) ||
        endsWith(fileNameShort, LOCK_FILE_ENDING))
        return;

    //update status information no matter whether object is excluded or not!
    cfg.acb_.reportCurrentFile(filepath, cfg.threadID_);

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    if (!cfg.filterInstance->passFileFilter(relNameParentPf_ + fileNameShort))
        return;

    //    std::string fileId = details.fileSize >=  1024 * 1024U ? util::retrieveFileID(filepath) : std::string();
    /*
    Perf test Windows 7, SSD, 350k files, 50k dirs, files > 1MB: 7000
    	regular:            6.9s
    	ID per file:       43.9s
    	ID per file > 1MB:  7.2s
    	ID per dir:         8.4s

    	Linux: retrieveFileID takes about 50% longer in VM! (avoidable because of redundant stat() call!)
    */

    output_.addSubFile(fileNameShort, FileDescriptor(details.lastWriteTime, details.fileSize, details.id, details.symlinkInfo != nullptr));

    cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
}


DirCallback::HandleLink DirCallback::onSymlink(const Zchar* shortName, const Zstring& linkpath, const SymlinkInfo& details)
{
#ifdef TODO_MinFFS_UI
    boost::this_thread::interruption_point();
#endif//TODO_MinFFS_UI
    //update status information no matter whether object is excluded or not!
    cfg.acb_.reportCurrentFile(linkpath, cfg.threadID_);

    switch (cfg.handleSymlinks_)
    {
        case SYMLINK_EXCLUDE:
            return LINK_SKIP;

        case SYMLINK_DIRECT:
            if (cfg.filterInstance->passFileFilter(relNameParentPf_ + shortName)) //always use file filter: Link type may not be "stable" on Linux!
            {
                output_.addSubLink(shortName, LinkDescriptor(details.lastWriteTime));
                cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
            }
            return LINK_SKIP;

        case SYMLINK_FOLLOW:
            //filter symlinks before trying to follow them: handle user-excluded broken symlinks!
            //since we don't know what type the symlink will resolve to, only do this when both variants agree:
            if (!cfg.filterInstance->passFileFilter(relNameParentPf_ + shortName))
            {
                bool subObjMightMatch = true;
                if (!cfg.filterInstance->passDirFilter(relNameParentPf_ + shortName, &subObjMightMatch))
                    if (!subObjMightMatch)
                        return LINK_SKIP;
            }
            return LINK_FOLLOW;
    }

    assert(false);
    return LINK_SKIP;
}


TraverseCallback* DirCallback::onDir(const Zchar* shortName, const Zstring& dirpath)
{
#ifdef TODO_MinFFS_UI
    boost::this_thread::interruption_point();
#endif//TODO_MinFFS_UI

    //update status information no matter whether object is excluded or not!
    cfg.acb_.reportCurrentFile(dirpath, cfg.threadID_);

    //------------------------------------------------------------------------------------
    const Zstring& relPath = relNameParentPf_ + shortName;

    //apply filter before processing (use relative name!)
    bool subObjMightMatch = true;
    const bool passFilter = cfg.filterInstance->passDirFilter(relPath, &subObjMightMatch);
    if (!passFilter && !subObjMightMatch)
        return nullptr; //do NOT traverse subdirs
    //else: attention! ensure directory filtering is applied later to exclude actually filtered directories

    DirContainer& subDir = output_.addSubDir(shortName);
    if (passFilter)
        cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator

    return new DirCallback(cfg, relPath + FILE_NAME_SEPARATOR, subDir); //releaseDirTraverser() is guaranteed to be called in any case
}


void DirCallback::releaseDirTraverser(TraverseCallback* trav)
{
    TraverseCallback::releaseDirTraverser(trav); //no-op, introduce compile-time coupling
    delete trav;
}


DirCallback::HandleError DirCallback::reportDirError(const std::wstring& msg, size_t retryNumber)
{
    //AsyncCallback::reportError() blocks while implementing boost::this_thread::interruption_point()
    switch (cfg.acb_.reportError(msg, retryNumber))
    {
        case FillBufferCallback::ON_ERROR_IGNORE:
            cfg.failedDirReads_.insert(relNameParentPf_);
            return ON_ERROR_IGNORE;

        case FillBufferCallback::ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_IGNORE;
}


DirCallback::HandleError DirCallback::reportItemError(const std::wstring& msg, size_t retryNumber, const Zchar* shortName)
{
    //AsyncCallback::reportError() blocks while implementing boost::this_thread::interruption_point()
    switch (cfg.acb_.reportError(msg, retryNumber))
    {
        case FillBufferCallback::ON_ERROR_IGNORE:
            cfg.failedItemReads_.insert(relNameParentPf_ + shortName);
            return ON_ERROR_IGNORE;

        case FillBufferCallback::ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_IGNORE;
}

//------------------------------------------------------------------------------------------

class WorkerThread
{
public:
    WorkerThread(long threadID,
                 const std::shared_ptr<AsyncCallback>& acb,
                 const DirectoryKey& dirKey,
                 DirectoryValue& dirOutput) :
        threadID_(threadID),
        acb_(acb),
        dirKey_(dirKey),
        dirOutput_(dirOutput) {}

    void operator()() //thread entry
    {
        acb_->incActiveWorker();
        ZEN_ON_SCOPE_EXIT(acb_->decActiveWorker(););

        acb_->reportCurrentFile(dirKey_.dirpath_, threadID_); //just in case first directory access is blocking

        TraverserShared travCfg(threadID_,
                                dirKey_.handleSymlinks_, //shared by all(!) instances of DirCallback while traversing a folder hierarchy
                                dirKey_.filter_,
                                dirOutput_.failedDirReads,
                                dirOutput_.failedItemReads,
                                *acb_);

        DirCallback traverser(travCfg,
                              Zstring(),
                              dirOutput_.dirCont);

        //get all files and folders from directoryPostfixed (and subdirectories)
        traverseFolder(dirKey_.dirpath_, traverser); //exceptions may be thrown!
    }

private:
    long threadID_;
    std::shared_ptr<AsyncCallback> acb_;
    const DirectoryKey dirKey_;
    DirectoryValue& dirOutput_;
};
}


void zen::fillBuffer(const std::set<DirectoryKey>& keysToRead, //in
                     std::map<DirectoryKey, DirectoryValue>& buf, //out
                     FillBufferCallback& callback,
                     size_t updateInterval)
{
    buf.clear();

#ifdef TODO_MinFFS_UI
    FixedList<boost::thread> worker; //note: we cannot use std::vector<boost::thread>: compiler error on GCC 4.7, probably a boost screw-up
#endif//TODO_MinFFS_UI

    zen::ScopeGuard guardWorker = zen::makeGuard([&]
    {
#ifdef TODO_MinFFS_UI
        for (boost::thread& wt : worker)
            wt.interrupt(); //interrupt all at once first, then join
        for (boost::thread& wt : worker)
            if (wt.joinable()) //= precondition of thread::join(), which throws an exception if violated!
                wt.join();     //in this context it is possible a thread is *not* joinable anymore due to the thread::timed_join() below!
#endif//TODO_MinFFS_UI
    });

    auto acb = std::make_shared<AsyncCallback>();

    //init worker threads
    for (const DirectoryKey& key : keysToRead)
    {
        assert(buf.find(key) == buf.end());
        DirectoryValue& dirOutput = buf[key];

#ifdef TODO_MinFFS_UI
        const long threadId = static_cast<long>(worker.size());
        worker.emplace_back(WorkerThread(threadId, acb, key, dirOutput));
#endif//TODO_MinFFS_UI
    }

    //wait until done
#ifdef TODO_MinFFS_UI
    for (boost::thread& wt : worker)
    {
        do
        {
            //update status
            callback.reportStatus(acb->getCurrentStatus(), acb->getItemsScanned()); //throw!

            //process errors
            acb->processErrors(callback);
        }
        while (!wt.timed_join(boost::posix_time::milliseconds(updateInterval)));

        acb->incrementNotifyingThreadId(); //process info messages of one thread at a time only
    }
#endif//TODO_MinFFS_UI

    guardWorker.dismiss();
}
