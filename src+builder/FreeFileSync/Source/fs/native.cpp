// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl.html       *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "native.h"
#include <zen/file_access.h>
#include <zen/symlink_target.h>
#include <zen/file_io.h>
#include <zen/file_id_def.h>
#include <zen/stl_tools.h>
#include <zen/recycler.h>
#include "../lib/resolve_path.h"
#include "../lib/icon_loader.h"
#include "native_traverser_impl.h"

#ifdef ZEN_WIN
    #include <zen/int64.h>
    #include <zen/com_tools.h>

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <fcntl.h> //fallocate, fcntl
#endif


namespace
{
#ifdef ZEN_WIN
    //manage COM-cleanup at per-thread-scope:
    thread_local std::unique_ptr<ComInitializer> nativeComInitThread;
#endif


void initComForThread() //throw FileError
{
#ifdef ZEN_WIN
    try
    {
        if (!nativeComInitThread) nativeComInitThread = std::make_unique<ComInitializer>(); //throw SysError
    }
    catch (const SysError& e) { throw FileError(e.toString()); } //there's little value in giving additional/misleading context info => just convert SysError to FileError
#endif
}


class RecycleSessionNative : public AbstractFileSystem::RecycleSession
{
public:
    RecycleSessionNative(const Zstring folderPathPf) : baseFolderPathPf_(folderPathPf) {}

    bool recycleItem(const AbstractPath& itemPath, const Zstring& logicalRelPath) override; //throw FileError
    void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) override; //throw FileError

private:
#ifdef ZEN_WIN
    Zstring getOrCreateRecyclerTempDirPf(); //throw FileError

    std::vector<Zstring> toBeRecycled; //full path of files located in temporary folder, waiting for batch-recycling
    Zstring recyclerTmpDir; //temporary folder holding files/folders for *deferred* recycling
#endif

    const Zstring baseFolderPathPf_; //ends with path separator
};

//===========================================================================================================================

void preAllocateSpaceBestEffort(FileHandle fh, const std::uint64_t streamSize, const Zstring& displayPath) //throw FileError
{
#ifdef ZEN_WIN
    LARGE_INTEGER fileSize = {};
    fileSize.QuadPart = streamSize;
    if (!::SetFilePointerEx(fh,          //__in       HANDLE hFile,
                            fileSize,    //__in       LARGE_INTEGER liDistanceToMove,
                            nullptr,     //__out_opt  PLARGE_INTEGER lpNewFilePointer,
                            FILE_BEGIN)) //__in       DWORD dwMoveMethod
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayPath)), L"SetFilePointerEx");

    if (!::SetEndOfFile(fh))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayPath)), L"SetEndOfFile");

    if (!::SetFilePointerEx(fh, {}, nullptr, FILE_BEGIN))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayPath)), L"SetFilePointerEx");

#elif defined ZEN_LINUX
    //don't use potentially inefficient ::posix_fallocate!
    const int rv = ::fallocate(fh,          //int fd,
                               0,           //int mode,
                               0,           //off_t offset
                               streamSize); //off_t len
    if (rv != 0)
        return; //may fail with EOPNOTSUPP, unlike posix_fallocate

#elif defined ZEN_MAC
    struct ::fstore store = {};
    store.fst_flags = F_ALLOCATECONTIG;
    store.fst_posmode = F_PEOFPOSMODE; //allocate from physical end of file
    //store.fst_offset     -> start of the region
    store.fst_length = streamSize;
    //store.fst_bytesalloc -> out: number of bytes allocated

    if (::fcntl(fh, F_PREALLOCATE, &store) == -1) //fcntl needs not return 0 on success!
    {
        store.fst_flags = F_ALLOCATEALL; //retry, allowing non-contiguous storage
        if (::fcntl(fh, F_PREALLOCATE, &store) == -1)
            return; //may fail with ENOTSUP!
    }
    //https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/ftruncate.2.html
    //=> file is extended with zeros, file offset is not changed
    if (::ftruncate(fh, streamSize) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(displayPath)), L"ftruncate");

    //F_PREALLOCATE + ftruncate seems optimal: http://adityaramesh.com/io_benchmark/
#endif
}


#ifdef ZEN_WIN
    using FileAttribs = BY_HANDLE_FILE_INFORMATION;
#elif defined ZEN_LINUX || defined ZEN_MAC
    typedef struct ::stat FileAttribs; //GCC 5.2 fails when "::" is used in "using FileAttribs = struct ::stat"
#endif


inline
FileAttribs getFileAttributes(FileHandle fh, const Zstring& filePath) //throw FileError
{
#ifdef ZEN_WIN
    BY_HANDLE_FILE_INFORMATION fileAttr = {};
    if (!::GetFileInformationByHandle(fh, &fileAttr))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"GetFileInformationByHandle");

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileAttr = {};
    if (::fstat(fh, &fileAttr) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"fstat");
#endif
    return fileAttr;
}


struct InputStreamNative : public AbstractFileSystem::InputStream
{
    InputStreamNative(const Zstring& filePath) : fi(filePath) {} //throw FileError, ErrorFileLocked

    size_t read(void* buffer, size_t bytesToRead) override { return fi.read(buffer, bytesToRead); } //throw FileError; returns "bytesToRead", unless end of file!
    AFS::FileId   getFileId          () override; //throw FileError
    std::int64_t  getModificationTime() override; //throw FileError
    std::uint64_t getFileSize        () override; //throw FileError
    size_t optimalBlockSize() const override { return fi.optimalBlockSize(); } //non-zero block size is AFS contract!

private:
    const FileAttribs& getBufferedAttributes() //throw FileError
    {
        if (!fileAttr)
            fileAttr = getFileAttributes(fi.getHandle(), fi.getFilePath()); //throw FileError
        return *fileAttr;
    }

    FileInput fi;
    Opt<FileAttribs> fileAttr;
};


inline
AFS::FileId InputStreamNative::getFileId()
{
    return convertToAbstractFileId(extractFileId(getBufferedAttributes())); //throw FileError
}


inline
std::int64_t InputStreamNative::getModificationTime()
{
#ifdef ZEN_WIN
    return filetimeToTimeT(getBufferedAttributes().ftLastWriteTime); //throw FileError
#elif defined ZEN_LINUX || defined ZEN_MAC
    return getBufferedAttributes().st_mtime; //throw FileError
#endif
}


inline
std::uint64_t InputStreamNative::getFileSize()
{
    const FileAttribs& fa = getBufferedAttributes(); //throw FileError
#ifdef ZEN_WIN
    return get64BitUInt(fa.nFileSizeLow, fa.nFileSizeHigh);
#elif defined ZEN_LINUX || defined ZEN_MAC
    return makeUnsigned(fa.st_size); //throw FileError
#endif
}

//===========================================================================================================================

struct OutputStreamNative : public AbstractFileSystem::OutputStreamImpl
{
    OutputStreamNative(const Zstring& filePath, const std::uint64_t* streamSize, const std::int64_t* modTime) :
        fo(filePath, FileOutput::ACC_CREATE_NEW) //throw FileError, ErrorTargetExisting
    {
        if (modTime)
            modTime_ = *modTime;

        if (streamSize) //pre-allocate file space, because we can
            preAllocateSpaceBestEffort(fo.getHandle(), *streamSize, fo.getFilePath()); //throw FileError
    }

    size_t optimalBlockSize() const override { return fo.optimalBlockSize(); } //non-zero block size is AFS contract!

private:
    void write(const void* buffer, size_t bytesToWrite) override { fo.write(buffer, bytesToWrite); } //throw FileError

    AFS::FileId finalize(const std::function<void()>& onUpdateStatus) override //throw FileError
    {
        const AFS::FileId fileId = convertToAbstractFileId(extractFileId(getFileAttributes(fo.getHandle(), fo.getFilePath()))); //throw FileError
        if (onUpdateStatus) onUpdateStatus(); //throw X!

        fo.close(); //throw FileError
        if (onUpdateStatus) onUpdateStatus(); //throw X!

        try
        {
            if (modTime_)
                zen::setFileTime(fo.getFilePath(), *modTime_, ProcSymlink::FOLLOW); //throw FileError
        }
        catch (FileError&)
        {
            throw;
            //Failing to set modification time is not a serious problem from synchronization
            //perspective (treated like external update), except for the inconvenience.
            //Support additional scenarios like writing to FTP on Linux? Keep strict handling for now.
        }

        return fileId;
    }

    FileOutput fo;
    Opt<std::int64_t> modTime_;
};

//===========================================================================================================================

class NativeFileSystem : public AbstractFileSystem
{
public:
    //itemPathImpl := native full item path as used by OS APIs

    static Zstring getItemPathImplForRecycler(const AbstractPath& ap)
    {
        if (typeid(getAfs(ap)) != typeid(NativeFileSystem))
            throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        return getItemPathImpl(ap);
    }

private:
    bool isNativeFileSystem() const override { return true; }

    Zstring getInitPathPhrase(const Zstring& itemPathImpl) const override { return itemPathImpl; }

    std::wstring getDisplayPath(const Zstring& itemPathImpl) const override { return utfCvrtTo<std::wstring>(itemPathImpl); }

    bool isNullPath(const Zstring& itemPathImpl) const override { return itemPathImpl.empty(); }

    Zstring appendRelPathToItemPathImpl(const Zstring& itemPathImpl, const Zstring& relPath) const override { return appendPaths(itemPathImpl, relPath, FILE_NAME_SEPARATOR); }

    //used during folder creation if parent folder is missing
    Opt<Zstring> getParentFolderPathImpl(const Zstring& itemPathImpl) const override
    {
#ifdef ZEN_WIN
        //remove trailing separator (even for C:\ root directories)
        const Zstring itemPathFmt = endsWith(itemPathImpl, FILE_NAME_SEPARATOR) ?
                                    beforeLast(itemPathImpl, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE) :
                                    itemPathImpl;

        const Zstring parentDir = beforeLast(itemPathFmt, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (parentDir.empty())
            return NoValue();
        if (parentDir.size() == 2 && isAlpha(parentDir[0]) && parentDir[1] == L':')
            return appendSeparator(parentDir);

#elif defined ZEN_LINUX || defined ZEN_MAC
        if (itemPathImpl == "/")
            return NoValue();

        const Zstring parentDir = beforeLast(itemPathImpl, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (parentDir.empty())
            return Zstring("/");
#endif
        return parentDir;
    }

    Zstring getFileShortName(const Zstring& itemPathImpl) const override { return afterLast(itemPathImpl, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL); }

    bool lessItemPathSameAfsType(const Zstring& itemPathImplLhs, const AbstractPath& apRhs) const override { return LessFilePath()(itemPathImplLhs, getItemPathImpl(apRhs)); }

    bool havePathDependencySameAfsType(const Zstring& itemPathImplLhs, const AbstractPath& apRhs) const override
    {
        const Zstring& lhs = appendSeparator(itemPathImplLhs);
        const Zstring& rhs = appendSeparator(getItemPathImpl(apRhs));

        const size_t lenMin = std::min(lhs.length(), rhs.length());

        return cmpFilePath(lhs.c_str(), lenMin, rhs.c_str(), lenMin) == 0; //note: don't make this an equivalence relation!
    }

    //----------------------------------------------------------------------------------------------------------------
    bool fileExists     (const Zstring& itemPathImpl) const override { return zen::fileExists     (itemPathImpl); } //noexcept
    bool folderExists   (const Zstring& itemPathImpl) const override { return zen::dirExists      (itemPathImpl); } //noexcept
    bool symlinkExists  (const Zstring& itemPathImpl) const override { return zen::symlinkExists  (itemPathImpl); } //noexcept
    bool somethingExists(const Zstring& itemPathImpl) const override { return zen::somethingExists(itemPathImpl); } //noexcept
    //----------------------------------------------------------------------------------------------------------------

    //should provide for single ATOMIC folder creation!
    void createFolderSimple(const Zstring& itemPathImpl) const override //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
    {
        initComForThread(); //throw FileError
        copyNewDirectory(Zstring(), itemPathImpl, false /*copyFilePermissions*/);
    }

    void removeFolderSimple(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::removeDirectorySimple(itemPathImpl); //throw FileError
    }

    bool removeFile(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        return zen::removeFile(itemPathImpl); //throw FileError
    }

    //----------------------------------------------------------------------------------------------------------------

    void setModTime(const Zstring& itemPathImpl, std::int64_t modificationTime) const override //throw FileError, follows symlinks
    {
        initComForThread(); //throw FileError
        zen::setFileTime(itemPathImpl, modificationTime, ProcSymlink::FOLLOW); //throw FileError
    }

    void setModTimeSymlink(const Zstring& itemPathImpl, std::int64_t modificationTime) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::setFileTime(itemPathImpl, modificationTime, ProcSymlink::DIRECT); //throw FileError
    }

    Zstring getResolvedSymlinkPath(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        return zen::getResolvedSymlinkPath(itemPathImpl); //throw FileError
    }

    Zstring getSymlinkContentBuffer(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        return getSymlinkTargetRaw(itemPathImpl); //throw FileError
    }

    //----------------------------------------------------------------------------------------------------------------
    //return value always bound:
    std::unique_ptr<InputStream > getInputStream (const Zstring& itemPathImpl) const override //throw FileError, ErrorFileLocked
    {
        initComForThread(); //throw FileError
        return std::make_unique<InputStreamNative >(itemPathImpl); //throw FileError, ErrorFileLocked
    }

    std::unique_ptr<OutputStreamImpl> getOutputStream(const Zstring& itemPathImpl,  //throw FileError, ErrorTargetExisting
                                                      const std::uint64_t* streamSize,                     //optional
                                                      const std::int64_t* modificationTime) const override //
    {
        initComForThread(); //throw FileError
        return std::make_unique<OutputStreamNative>(itemPathImpl, streamSize, modificationTime); //throw FileError, ErrorTargetExisting
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolder(const Zstring& itemPathImpl, TraverserCallback& sink) const override //noexcept
    {
#ifdef ZEN_WIN
        if (!tryReportingDirError([&] { initComForThread(); /*throw FileError*/ }, sink))
            return;
#endif
        DirTraverser::execute(itemPathImpl, sink); //noexcept
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow link!
    FileAttribAfterCopy copyFileForSameAfsType(const Zstring& itemPathImplSource, const AbstractPath& apTarget, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                               const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const override //may be nullptr; throw X!
    {
        initComForThread(); //throw FileError

        const InSyncAttributes attrNew = copyNewFile(itemPathImplSource, getItemPathImpl(apTarget), //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                                     copyFilePermissions, onNotifyCopyStatus); //may be nullptr; throw X!
        FileAttribAfterCopy attrOut;
        attrOut.fileSize         = attrNew.fileSize;
        attrOut.modificationTime = attrNew.modificationTime;
        attrOut.sourceFileId     = convertToAbstractFileId(attrNew.sourceFileId);
        attrOut.targetFileId     = convertToAbstractFileId(attrNew.targetFileId);
        return attrOut;
    }

    //symlink handling: follow link!
    void copyNewFolderForSameAfsType(const Zstring& itemPathImplSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::copyNewDirectory(itemPathImplSource, getItemPathImpl(apTarget), copyFilePermissions);
    }

    void copySymlinkForSameAfsType(const Zstring& itemPathImplSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::copySymlink(itemPathImplSource, getItemPathImpl(apTarget), copyFilePermissions); //throw FileError
    }

    void renameItemForSameAfsType(const Zstring& itemPathImplSource, const AbstractPath& apTarget) const override //throw FileError, ErrorTargetExisting, ErrorDifferentVolume
    {
        initComForThread(); //throw FileError
        zen::renameFile(itemPathImplSource, getItemPathImpl(apTarget)); //throw FileError, ErrorTargetExisting, ErrorDifferentVolume
    }

    bool supportsPermissions(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        return zen::supportsPermissions(itemPathImpl);
    }

    //----------------------------------------------------------------------------------------------------------------
    ImageHolder getFileIcon(const Zstring& itemPathImpl, int pixelSize) const override //noexcept; optional return value
    {
        try
        {
            initComForThread(); //throw FileError
            return zen::getFileIcon(itemPathImpl, pixelSize);
        }
        catch (FileError&) { assert(false); return ImageHolder(); }
    }

    ImageHolder getThumbnailImage(const Zstring& itemPathImpl, int pixelSize) const override //noexcept; optional return value
    {
        try
        {
            initComForThread(); //throw FileError
            return zen::getThumbnailImage(itemPathImpl, pixelSize);
        }
        catch (FileError&) { assert(false); return ImageHolder(); }
    }

    bool folderExistsThrowing(const Zstring& itemPathImpl) const override //throw FileError
    {
        warn_static("finish file error detection")

        initComForThread(); //throw FileError
        return zen::dirExists(itemPathImpl);
    }

    void connectNetworkFolder(const Zstring& itemPathImpl, bool allowUserInteraction) const override //throw FileError
    {
        warn_static("clean-up/remove/re-think the getAsyncConnectFolder() function")

#ifdef ZEN_WIN
        initComForThread(); //throw FileError

        //login to network share, if necessary
        loginNetworkShare(itemPathImpl, allowUserInteraction);
#endif
    }

    //----------------------------------------------------------------------------------------------------------------

    std::uint64_t getFreeDiskSpace(const Zstring& itemPathImpl) const override //throw FileError, returns 0 if not available
    {
        initComForThread(); //throw FileError
        return zen::getFreeDiskSpace(itemPathImpl); //throw FileError
    }

    bool supportsRecycleBin(const Zstring& itemPathImpl, const std::function<void ()>& onUpdateGui) const override //throw FileError
    {
#ifdef ZEN_WIN
        initComForThread(); //throw FileError
        return recycleBinExists(itemPathImpl, onUpdateGui); //throw FileError

#elif defined ZEN_LINUX || defined ZEN_MAC
        return true; //truth be told: no idea!!!
#endif
    }

    std::unique_ptr<RecycleSession> createRecyclerSession(const Zstring& itemPathImpl) const override //throw FileError, return value must be bound!
    {
        initComForThread(); //throw FileError
        assert(supportsRecycleBin(itemPathImpl, nullptr));
        return std::make_unique<RecycleSessionNative>(appendSeparator(itemPathImpl));
    }

    void recycleItemDirectly(const Zstring& itemPathImpl) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::recycleOrDelete(itemPathImpl); //throw FileError
    }
};

//===========================================================================================================================

#ifdef ZEN_WIN
//create + returns temporary directory postfixed with file name separator
//to support later cleanup if automatic deletion fails for whatever reason
Zstring RecycleSessionNative::getOrCreateRecyclerTempDirPf() //throw FileError
{
    assert(!baseFolderPathPf_.empty());
    if (baseFolderPathPf_.empty())
        return Zstring();

    if (recyclerTmpDir.empty())
        recyclerTmpDir = [&]
    {
        assert(endsWith(baseFolderPathPf_, FILE_NAME_SEPARATOR));
        /*
        -> this naming convention is too cute and confusing for end users:

        //1. generate random directory name
        static std::mt19937 rng(std::time(nullptr)); //don't use std::default_random_engine which leaves the choice to the STL implementer!
        //- the alternative std::random_device may not always be available and can even throw an exception!
        //- seed with second precision is sufficient: collisions are handled below

        const Zstring chars(Zstr("abcdefghijklmnopqrstuvwxyz")
                            Zstr("1234567890"));
        std::uniform_int_distribution<size_t> distrib(0, chars.size() - 1); //takes closed range

        auto generatePath = [&]() -> Zstring //e.g. C:\Source\3vkf74fq.ffs_tmp
        {
            Zstring path = baseDirPf;
            for (int i = 0; i < 8; ++i)
                path += chars[distrib(rng)];
            return path + TEMP_FILE_ENDING;
        };
        */

        //ensure unique ownership:
        Zstring dirpath = baseFolderPathPf_ + Zstr("RecycleBin") + AFS::TEMP_FILE_ENDING;
        for (int i = 0;; ++i)
            try
            {
                copyNewDirectory(Zstring(), dirpath, false /*copyFilePermissions*/); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
                return dirpath;
            }
            catch (ErrorTargetPathMissing&)
            {
                assert(false); //unexpected: base directory should have been created already!
                throw;
            }
            catch (ErrorTargetExisting&)
            {
                if (i == 10) throw; //avoid endless recursion in pathological cases
                dirpath = baseFolderPathPf_ + Zstr("RecycleBin") + Zchar('_') + numberTo<Zstring>(i) + AFS::TEMP_FILE_ENDING;
            }
    }();

    //assemble temporary recycle bin directory with random name and .ffs_tmp ending
    return appendSeparator(recyclerTmpDir);
}
#endif


bool RecycleSessionNative::recycleItem(const AbstractPath& itemPath, const Zstring& logicalRelPath) //throw FileError
{
    const Zstring itemPathImpl = NativeFileSystem::getItemPathImplForRecycler(itemPath);
    assert(!startsWith(logicalRelPath, FILE_NAME_SEPARATOR));

#ifdef ZEN_WIN
    const bool isRemnantRecyclerItem = [&itemPathImpl] //clean-up of recycler temp directory failed during last sync
    {
        //search for path component named "RecycleBin.ffs_tmp" or "RecycleBin_<num>.ffs_tmp":
        const size_t pos = itemPathImpl.find(L"\\RecycleBin");
        if (pos == Zstring::npos)
            return false;

        const size_t pos2 = itemPathImpl.find(L'\\', pos + 1);
        return endsWith(StringRef<const Zchar>(itemPathImpl.begin(), pos2 == Zstring::npos ? itemPathImpl.end() : itemPathImpl.begin() + pos2), AFS::TEMP_FILE_ENDING);
    }();

    //do not create RecycleBin.ffs_tmp directories recursively if recycling a particular item fails forever!
    //=> 1. stack overflow crashes 2. paths longer than 260 chars, undeletable/viewable with Explorer
    if (isRemnantRecyclerItem)
        return recycleOrDelete(itemPathImpl); //throw FileError

    const Zstring tmpPath = getOrCreateRecyclerTempDirPf() + logicalRelPath; //throw FileError
    bool deleted = false;

    auto moveToTempDir = [&]
    {
        //perf: Instead of recycling each object separately, we rename them one by one
        //      into a temporary directory and batch-recycle all at once after sync
        renameFile(itemPathImpl, tmpPath); //throw FileError, ErrorDifferentVolume
        this->toBeRecycled.push_back(tmpPath);
        deleted = true;
    };

    try
    {
        try
        {
            moveToTempDir(); //throw FileError, ErrorDifferentVolume
        }
        catch (ErrorDifferentVolume&) { throw; }
        catch (FileError&)
        {
            if (somethingExists(itemPathImpl))
            {
                const Zstring tmpParentDir = beforeLast(tmpPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE); //what if C:\ ?
                if (!somethingExists(tmpParentDir))
                {
                    makeDirectoryRecursively(tmpParentDir); //throw FileError
                    moveToTempDir(); //throw FileError, ErrorDifferentVolume -> this should work now!
                }
                else
                    throw;
            }
        }
    }
    catch (ErrorDifferentVolume&) //MoveFileEx() returns ERROR_PATH_NOT_FOUND *before* considering ERROR_NOT_SAME_DEVICE! => we have to create tmpParentDir to find out!
    {
        return recycleOrDelete(itemPathImpl); //throw FileError
    }

    return deleted;

#elif defined ZEN_LINUX || defined ZEN_MAC
    return recycleOrDelete(itemPathImpl); //throw FileError
#endif
}


void RecycleSessionNative::tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) //throw FileError
{
#ifdef ZEN_WIN
    if (!toBeRecycled.empty())
    {
        //move content of temporary directory to recycle bin in a single call
        recycleOrDelete(toBeRecycled, notifyDeletionStatus); //throw FileError
        toBeRecycled.clear();
    }

    //clean up temp directory itself (should contain remnant empty directories only)
    if (!recyclerTmpDir.empty())
    {
        removeDirectoryRecursively(recyclerTmpDir); //throw FileError
        recyclerTmpDir.clear();
    }
#endif
}
}


//coordinate changes with getResolvedFilePath()!
bool zen::acceptsItemPathPhraseNative(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = itemPathPhrase;
    path = expandMacros(path); //expand before trimming!
    trim(path);

#ifdef ZEN_WIN
    path = removeLongPathPrefix(path);
#endif

    if (startsWith(path, Zstr("["))) //drive letter by volume name syntax
        return true;

    //don't accept relative paths!!! indistinguishable from Explorer MTP paths!

#ifdef ZEN_WIN
    if (path.size() >= 3 && //path starting with drive letter
        std::iswalpha(path[0]) &&
        path[1] == L':' &&
        path[2] == L'\\')
        return true;

    //UNC path
    if (startsWith(path, L"\\\\"))
    {
        const Zstring server = beforeFirst<Zstring>(path.c_str() + 2, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
        const Zstring share  = afterFirst <Zstring>(path.c_str() + 2, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (!server.empty() && !share.empty())
            return true;
        //don't accept paths missing the shared folder! (see drag & drop validation!)
    }
    return false;

#elif defined ZEN_LINUX || defined ZEN_MAC
    return startsWith(path, Zstr("/"));
#endif
}


AbstractPath zen::createItemPathNative(const Zstring& itemPathPhrase) //noexcept
{
    const Zstring itemPathImpl = getResolvedFilePath(itemPathPhrase);
    return AbstractPath(std::make_shared<NativeFileSystem>(), itemPathImpl);
    warn_static("get volume by name hangs for idle HDD! => run createItemPathNative during getFolderStatusNonBlocking() but getResolvedFilePath currently not thread-safe!")
}
