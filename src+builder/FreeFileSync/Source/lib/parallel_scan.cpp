// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "parallel_scan.h"
#include <zen/file_error.h>
#include <zen/thread.h>
#include <zen/scope_guard.h>
#include <zen/fixed_list.h>
#include <zen/tick_count.h>
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
    AsyncCallback(size_t reportingIntervalMs) : reportingIntervalTicks(reportingIntervalMs * ticksPerSec() / 1000) {}

    //blocking call: context of worker thread
    FillBufferCallback::HandleError reportError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
    {
        std::unique_lock<std::mutex> dummy(lockErrorInfo);
        interruptibleWait(conditionCanReportError, dummy, [this] { return !errorInfo && !errorResponse; }); //throw ThreadInterruption

        errorInfo = std::make_unique<std::pair<BasicWString, size_t>>(copyStringTo<BasicWString>(msg), retryNumber);

        interruptibleWait(conditionGotResponse, dummy, [this] { return static_cast<bool>(errorResponse); }); //throw ThreadInterruption

        FillBufferCallback::HandleError rv = *errorResponse;

        errorInfo    .reset();
        errorResponse.reset();

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionCanReportError.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796

        return rv;
    }

    void processErrors(FillBufferCallback& callback) //context of main thread, call repreatedly
    {
        std::unique_lock<std::mutex> dummy(lockErrorInfo);
        if (errorInfo.get() && !errorResponse.get())
        {
            FillBufferCallback::HandleError rv = callback.reportError(copyStringTo<std::wstring>(errorInfo->first), errorInfo->second); //throw!
            errorResponse = std::make_unique<FillBufferCallback::HandleError>(rv);

            dummy.unlock(); //optimization for condition_variable::notify_all()
            conditionGotResponse.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        }
    }

    void incrementNotifyingThreadId() { ++notifyingThreadID; } //context of main thread

    //perf optimization: comparison phase is 7% faster by avoiding needless std::wstring contstruction for reportCurrentFile()
    bool mayReportCurrentFile(int threadID, TickVal& lastReportTime) const
    {
        if (threadID != notifyingThreadID) //only one thread at a time may report status
            return false;

        const TickVal now = getTicks(); //0 on error
        if (dist(lastReportTime, now) >= reportingIntervalTicks) //perform ui updates not more often than necessary
        {
            lastReportTime = now; //keep "lastReportTime" at worker thread level to avoid locking!
            return true;
        }
        return false;
    }

    void reportCurrentFile(const std::wstring& filepath) //context of worker thread
    {
        std::lock_guard<std::mutex> dummy(lockCurrentStatus);
        currentFile = copyStringTo<BasicWString>(filepath);
    }

    std::wstring getCurrentStatus() //context of main thread, call repreatedly
    {
        std::wstring filepath;
        {
            std::lock_guard<std::mutex> dummy(lockCurrentStatus);
            filepath = copyStringTo<std::wstring>(currentFile);
        }

        if (filepath.empty())
            return std::wstring();

        std::wstring statusText = copyStringTo<std::wstring>(textScanning);

        const long activeCount = activeWorker;
        if (activeCount >= 2)
            statusText += L" [" + replaceCpy(_P("1 thread", "%x threads", activeCount), L"%x", numberTo<std::wstring>(activeCount)) + L"]";

        statusText += L" ";
        statusText += filepath;
        return statusText;
    }

    void incItemsScanned() { ++itemsScanned; } //perf: irrelevant! scanning is almost entirely file I/O bound, not CPU bound! => no prob having multiple threads poking at the same variable!
    long getItemsScanned() const { return itemsScanned; }

    void incActiveWorker() { ++activeWorker; }
    void decActiveWorker() { --activeWorker; }
    long getActiveWorker() const { return activeWorker; }

private:
    //---- error handling ----
    std::mutex lockErrorInfo;
    std::condition_variable conditionCanReportError;
    std::condition_variable conditionGotResponse;
    std::unique_ptr<std::pair<BasicWString, size_t>> errorInfo; //error message + retry number
    std::unique_ptr<FillBufferCallback::HandleError> errorResponse;

    //---- status updates ----
    std::atomic<int> notifyingThreadID { 0 }; //CAVEAT: do NOT use boost::thread::id: https://svn.boost.org/trac/boost/ticket/5754

    std::mutex lockCurrentStatus; //use a different lock for current file: continue traversing while some thread may process an error
    BasicWString currentFile;
    const std::int64_t reportingIntervalTicks;

    const BasicWString textScanning { copyStringTo<BasicWString>(_("Scanning:")) }; //this one is (currently) not shared and could be made a std::wstring, but we stay consistent and use thread-safe variables in this class only!

    //---- status updates II (lock free) ----
    std::atomic<int> itemsScanned{ 0 }; //std:atomic is uninitialized by default!
    std::atomic<int> activeWorker{ 0 }; //
};

//-------------------------------------------------------------------------------------------------

struct TraverserConfig
{
public:
    TraverserConfig(int threadID,
                    const ABF& abf,
                    const HardFilter::FilterRef& filter,
                    SymLinkHandling handleSymlinks,
                    std::map<Zstring, std::wstring, LessFilePath>& failedDirReads,
                    std::map<Zstring, std::wstring, LessFilePath>& failedItemReads,
                    AsyncCallback& acb) :
        baseFolder(abf),
        filter_(filter),
        handleSymlinks_(handleSymlinks),
        failedDirReads_ (failedDirReads),
        failedItemReads_(failedItemReads),
        acb_(acb),
        threadID_(threadID) {}

    const ABF& baseFolder;
    const HardFilter::FilterRef filter_; //always bound!
    const SymLinkHandling handleSymlinks_;

    std::map<Zstring, std::wstring, LessFilePath>& failedDirReads_;
    std::map<Zstring, std::wstring, LessFilePath>& failedItemReads_;

    AsyncCallback& acb_;
    const int threadID_;
    TickVal lastReportTime;
};


class DirCallback : public ABF::TraverserCallback
{
public:
    DirCallback(TraverserConfig& config,
                const Zstring& relNameParentPf, //postfixed with FILE_NAME_SEPARATOR!
                DirContainer& output,
                int level) :
        cfg(config),
        relNameParentPf_(relNameParentPf),
        output_(output),
        level_(level) {}

    virtual void                               onFile   (const FileInfo&    fi) override; //
    virtual std::unique_ptr<TraverserCallback> onDir    (const DirInfo&     di) override; //throw ThreadInterruption
    virtual HandleLink                         onSymlink(const SymlinkInfo& li) override; //

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override; //throw ThreadInterruption
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override; //

private:
    TraverserConfig& cfg;
    const Zstring relNameParentPf_;
    DirContainer& output_;
    const int level_;
};


void DirCallback::onFile(const FileInfo& fi) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    //do not list the database file(s) sync.ffs_db, sync.x64.ffs_db, etc. or lock files
    if (endsWith(fi.itemName, SYNC_DB_FILE_ENDING) ||
        endsWith(fi.itemName, LOCK_FILE_ENDING))
        return;

    const Zstring relFilePath = relNameParentPf_ + fi.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime))
        cfg.acb_.reportCurrentFile(ABF::getDisplayPath(cfg.baseFolder.getAbstractPath(relFilePath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    if (!cfg.filter_->passFileFilter(relFilePath))
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

    output_.addSubFile(fi.itemName, FileDescriptor(fi.lastWriteTime, fi.fileSize, fi.id, fi.symlinkInfo != nullptr));

    cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
}


std::unique_ptr<ABF::TraverserCallback> DirCallback::onDir(const DirInfo& di) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& relDirPath = relNameParentPf_ + di.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime))
        cfg.acb_.reportCurrentFile(ABF::getDisplayPath(cfg.baseFolder.getAbstractPath(relDirPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    bool childItemMightMatch = true;
    const bool passFilter = cfg.filter_->passDirFilter(relDirPath, &childItemMightMatch);
    if (!passFilter && !childItemMightMatch)
        return nullptr; //do NOT traverse subdirs
    //else: attention! ensure directory filtering is applied later to exclude actually filtered directories

    DirContainer& subDir = output_.addSubDir(di.itemName);
    if (passFilter)
        cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator

    //------------------------------------------------------------------------------------
    if (level_ > 100) //Win32 traverser: stack overflow approximately at level 1000
        if (!tryReportingItemError([&] //check after DirContainer::addSubDir()
    {
        throw FileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", ABF::getDisplayPath(cfg.baseFolder.getAbstractPath(relDirPath))), L"Endless recursion.");
        }, *this, di.itemName))
    return nullptr;

    return std::make_unique<DirCallback>(cfg, relDirPath + FILE_NAME_SEPARATOR, subDir, level_ + 1); //releaseDirTraverser() is guaranteed to be called in any case
}


DirCallback::HandleLink DirCallback::onSymlink(const SymlinkInfo& si) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& relLinkPath = relNameParentPf_ + si.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime))
        cfg.acb_.reportCurrentFile(ABF::getDisplayPath(cfg.baseFolder.getAbstractPath(relLinkPath)));

    switch (cfg.handleSymlinks_)
    {
        case SYMLINK_EXCLUDE:
            return LINK_SKIP;

        case SYMLINK_DIRECT:
            if (cfg.filter_->passFileFilter(relLinkPath)) //always use file filter: Link type may not be "stable" on Linux!
            {
                output_.addSubLink(si.itemName, LinkDescriptor(si.lastWriteTime));
                cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
            }
            return LINK_SKIP;

        case SYMLINK_FOLLOW:
            //filter symlinks before trying to follow them: handle user-excluded broken symlinks!
            //since we don't know yet what type the symlink will resolve to, only do this when both variants agree:
            if (!cfg.filter_->passFileFilter(relLinkPath))
            {
                bool childItemMightMatch = true;
                if (!cfg.filter_->passDirFilter(relLinkPath, &childItemMightMatch))
                    if (!childItemMightMatch)
                        return LINK_SKIP;
            }
            return LINK_FOLLOW;
    }

    assert(false);
    return LINK_SKIP;
}


DirCallback::HandleError DirCallback::reportDirError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
{
    switch (cfg.acb_.reportError(msg, retryNumber)) //throw ThreadInterruption
    {
        case FillBufferCallback::ON_ERROR_IGNORE:
            cfg.failedDirReads_[beforeLast(relNameParentPf_, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)] = msg;
            return ON_ERROR_IGNORE;

        case FillBufferCallback::ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_IGNORE;
}


DirCallback::HandleError DirCallback::reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) //throw ThreadInterruption
{
    switch (cfg.acb_.reportError(msg, retryNumber)) //throw ThreadInterruption
    {
        case FillBufferCallback::ON_ERROR_IGNORE:
            cfg.failedItemReads_[relNameParentPf_ + itemName] =  msg;
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
    WorkerThread(int threadID,
                 const std::shared_ptr<AsyncCallback>& acb,
                 std::unique_ptr<ABF>&& baseFolder,   //always bound!
                 const HardFilter::FilterRef& filter, //
                 SymLinkHandling handleSymlinks,
                 DirectoryValue& dirOutput) :
        acb_(acb),
        baseFolder_(std::move(baseFolder)),
        outputContainer(dirOutput.dirCont),
        travCfg(threadID,
                *baseFolder_,
                filter,
                handleSymlinks, //shared by all(!) instances of DirCallback while traversing a folder hierarchy
                dirOutput.failedDirReads,
                dirOutput.failedItemReads,
                *acb_) {}

    void operator()() //thread entry
    {
        acb_->incActiveWorker();
        ZEN_ON_SCOPE_EXIT(acb_->decActiveWorker());

        const AbstractPathRef& baseFolderPath = baseFolder_->getAbstractPath();

        if (acb_->mayReportCurrentFile(travCfg.threadID_, travCfg.lastReportTime))
            acb_->reportCurrentFile(ABF::getDisplayPath(baseFolderPath)); //just in case first directory access is blocking

        DirCallback cb(travCfg, Zstring(), outputContainer, 0);

        ABF::traverseFolder(baseFolderPath, cb); //throw X
    }

private:
    std::shared_ptr<AsyncCallback> acb_;
    std::unique_ptr<ABF> baseFolder_; //always bound!
    DirContainer& outputContainer;
    TraverserConfig travCfg;
};
}


void zen::fillBuffer(const std::set<DirectoryKey>& keysToRead, //in
                     std::map<DirectoryKey, DirectoryValue>& buf, //out
                     FillBufferCallback& callback,
                     size_t updateIntervalMs)
{
    buf.clear();

    FixedList<InterruptibleThread> worker;

    zen::ScopeGuard guardWorker = zen::makeGuard([&]
    {
        for (InterruptibleThread& wt : worker)
            wt.interrupt(); //interrupt all at once first, then join
        for (InterruptibleThread& wt : worker)
            if (wt.joinable()) //= precondition of thread::join(), which throws an exception if violated!
                wt.join();     //in this context it is possible a thread is *not* joinable anymore due to the thread::try_join_for() below!
    });

    auto acb = std::make_shared<AsyncCallback>(updateIntervalMs / 2 /*reportingIntervalMs*/);

    //init worker threads
    for (const DirectoryKey& key : keysToRead)
    {
        assert(buf.find(key) == buf.end());
        DirectoryValue& dirOutput = buf[key];

        const int threadId = static_cast<int>(worker.size());
        worker.emplace_back(WorkerThread(threadId,
                                         acb,
                                         key.baseFolder_->createIndependentCopy(), //copy instance for safe access on any method by a different thread!
                                         key.filter_,
                                         key.handleSymlinks_,
                                         dirOutput));
    }

    //wait until done
    for (InterruptibleThread& wt : worker)
    {
        do
        {
            //update status
            callback.reportStatus(acb->getCurrentStatus(), acb->getItemsScanned()); //throw!

            //process errors
            acb->processErrors(callback);
        }
        while (!wt.tryJoinFor(std::chrono::milliseconds(updateIntervalMs)));

        acb->incrementNotifyingThreadId(); //process info messages of one thread at a time only
    }

    guardWorker.dismiss();
}
