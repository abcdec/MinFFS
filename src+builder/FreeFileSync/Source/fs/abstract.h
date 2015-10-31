// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl.html       *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ABSTRACT_FS_873450978453042524534234
#define ABSTRACT_FS_873450978453042524534234

#include <functional>
#include <zen/file_error.h>
#include <zen/zstring.h>
#include <zen/optional.h>
#include "../lib/icon_holder.h"


namespace zen
{
struct AbstractBaseFolder;


struct AbstractPathRef
{
    struct ItemId;
    ItemId getUniqueId() const; //non-referencing identifier value

private:
    AbstractPathRef(const AbstractBaseFolder& abfIn, const Zstring& itemPathImplIn) : abf(&abfIn), itemPathImpl(itemPathImplIn) {}
    friend struct AbstractBaseFolder;

    const AbstractBaseFolder* abf; //always bound
    Zstring itemPathImpl; //valid only in context of a specific AbstractBaseFolder *instance*, e.g. FTP session!
    //referenced item needs not exist; no file I/O!!!
};

//==============================================================================================================
//==============================================================================================================

struct AbstractBaseFolder
{
    virtual Zstring getInitPathPhrase() const = 0; //noexcept

    //copy this instance for life-time management and safe access on any method by a different thread:
    virtual std::unique_ptr<AbstractBaseFolder> createIndependentCopy() const = 0; //noexcept

    static std::wstring getDisplayPath(const AbstractPathRef& ap) { return ap.abf->getDisplayPath(ap.itemPathImpl); }

    static Zstring getFileShortName(const AbstractPathRef& ap) { return ap.abf->getFileShortName(ap.itemPathImpl); }

    struct LessItemPath;
    static bool equalItemPath(const AbstractPathRef& lhs, const AbstractPathRef& rhs);

    static bool havePathDependency(const AbstractBaseFolder& lhs, const AbstractBaseFolder& rhs);

    static AbstractPathRef appendRelPath(const AbstractPathRef& ap, const Zstring& relPath) { return AbstractPathRef(*ap.abf, ap.abf->appendRelPathToItemPathImpl(ap.itemPathImpl, relPath)); }

    static Opt<Zstring> getNativeItemPath(const AbstractPathRef& ap) { return ap.abf->isNativeFileSystem() ? Opt<Zstring>(ap.itemPathImpl) : NoValue(); }

    static std::unique_ptr<AbstractPathRef> getParentFolderPath(const AbstractPathRef& ap)
    {
        if (const Opt<Zstring> parentPathImpl = ap.abf->getParentFolderPathImpl(ap.itemPathImpl))
            return std::unique_ptr<AbstractPathRef>(new AbstractPathRef(*ap.abf, *parentPathImpl));
        return nullptr;
    }
    //limitation: zen::Opt requires default-constructibility => we need to use std::unique_ptr:

    //----------------------------------------------------------------------------------------------------------------
    static bool fileExists     (const AbstractPathRef& ap) { return ap.abf->fileExists     (ap.itemPathImpl); } //noexcept; check whether file      or file-symlink exists
    static bool folderExists   (const AbstractPathRef& ap) { return ap.abf->folderExists   (ap.itemPathImpl); } //noexcept; check whether directory or dir-symlink exists
    static bool symlinkExists  (const AbstractPathRef& ap) { return ap.abf->symlinkExists  (ap.itemPathImpl); } //noexcept; check whether a symbolic link exists
    static bool somethingExists(const AbstractPathRef& ap) { return ap.abf->somethingExists(ap.itemPathImpl); } //noexcept; check whether any object with this name exists
    //----------------------------------------------------------------------------------------------------------------

    //should provide for single ATOMIC folder creation!
    static void createFolderSimple(const AbstractPathRef& ap) { ap.abf->createFolderSimple(ap.itemPathImpl); }; //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing

    //non-recursive folder deletion:
    static void removeFolderSimple(const AbstractPathRef& ap) { ap.abf->removeFolderSimple(ap.itemPathImpl); }; //throw FileError

    //- no error if already existing
    //- create recursively if parent directory is not existing
    static void createFolderRecursively(const AbstractPathRef& ap); //throw FileError

    static void removeFolderRecursively(const AbstractPathRef& ap, //throw FileError
                                        const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion,    //optional
                                        const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion); //one call for each *existing* object!

    static bool removeFile(const AbstractPathRef& ap) { return ap.abf->removeFile(ap.itemPathImpl); }; //throw FileError; return "false" if file is not existing

    //----------------------------------------------------------------------------------------------------------------

    static void setModTime       (const AbstractPathRef& ap, std::int64_t modificationTime) { ap.abf->setModTime       (ap.itemPathImpl, modificationTime); } //throw FileError, follows symlinks
    static void setModTimeSymlink(const AbstractPathRef& ap, std::int64_t modificationTime) { ap.abf->setModTimeSymlink(ap.itemPathImpl, modificationTime); } //throw FileError

    static AbstractPathRef getResolvedSymlinkPath(const AbstractPathRef& ap) { return ap.abf->getResolvedSymlinkPath(ap.itemPathImpl); } //throw FileError

    static Zstring getSymlinkContentBuffer(const AbstractPathRef& ap) { return ap.abf->getSymlinkContentBuffer(ap.itemPathImpl); } //throw FileError

    //----------------------------------------------------------------------------------------------------------------

    struct IconLoader
    {
        std::function<ImageHolder(int pixelSize)> getThumbnailImage; //optional, noexcept!
        std::function<ImageHolder(int pixelSize)> getFileIcon;       //
    };

    //Async operations: retrieve function objects for asynchronous loading
    //- THREAD-SAFETY: must be thread-safe like an int! => no dangling references to this instance!
    static IconLoader getAsyncIconLoader(const AbstractPathRef& ap) { return ap.abf->getAsyncIconLoader(ap.itemPathImpl); } //noexcept!
    virtual std::function<void()> /*throw FileError*/ getAsyncConnectFolder(bool allowUserInteraction) const = 0; //noexcept, optional return value
    static std::function<bool()> /*throw FileError*/ getAsyncCheckFolderExists(const AbstractPathRef& ap) { return ap.abf->getAsyncCheckFolderExists(ap.itemPathImpl); } //noexcept
    //----------------------------------------------------------------------------------------------------------------

    using FileId = Zbase<char>;

    //----------------------------------------------------------------------------------------------------------------
    struct InputStream
    {
        virtual ~InputStream() {}
        virtual size_t read(void* buffer, size_t bytesToRead) = 0; //throw FileError; returns "bytesToRead", unless end of file!
        virtual FileId         getFileId          () = 0; //throw FileError
        virtual std::int64_t   getModificationTime() = 0; //throw FileError
        virtual std::uint64_t  getFileSize        () = 0; //throw FileError
        virtual size_t optimalBlockSize() const = 0; //non-zero block size is ABF contract! it's implementers job to always give a reasonable buffer size!
    protected:
        InputStream() {}
    private:
        InputStream           (InputStream&) = delete;
        InputStream& operator=(InputStream&) = delete;
    };

    //TRANSACTIONAL output stream! => call finalize when done!
    //- takes ownership and deletes on errors!!!! => transactionally create output stream first if not existing!!
    //- AbstractPathRef member => implicit contract: ABF instance needs to out-live OutputStream!
    struct OutputStream
    {
        virtual ~OutputStream();
        void write(const void* buffer, size_t bytesToWrite); //throw FileError
        FileId finalize(const std::function<void()>& onUpdateStatus); //throw FileError
        virtual size_t optimalBlockSize() const = 0; //non-zero block size is ABF contract!

    protected:
        OutputStream(const AbstractPathRef& filePath, const std::uint64_t* streamSize);
        bool finalizeHasSucceeded() const { return finalizeSucceeded; }
    private:
        virtual void   writeImpl   (const void* buffer, size_t bytesToWrite    ) = 0; //throw FileError
        virtual FileId finalizeImpl(const std::function<void()>& onUpdateStatus) = 0; //throw FileError

        OutputStream           (OutputStream&) = delete;
        OutputStream& operator=(OutputStream&) = delete;

        const AbstractPathRef filePath_;
        bool finalizeSucceeded = false;
        Opt<std::uint64_t> bytesExpected;
        std::uint64_t bytesWritten = 0;
    };

    //return value always bound:
    static std::unique_ptr<InputStream > getInputStream (const AbstractPathRef& ap) { return ap.abf->getInputStream (ap.itemPathImpl); } //throw FileError, ErrorFileLocked
    static std::unique_ptr<OutputStream> getOutputStream(const AbstractPathRef& ap,
                                                         const std::uint64_t* streamSize,      //optional
                                                         const std::int64_t* modificationTime) //
    { return ap.abf->getOutputStream(ap.itemPathImpl, streamSize, modificationTime); } //throw FileError, ErrorTargetExisting
    //----------------------------------------------------------------------------------------------------------------

    struct TraverserCallback
    {
        virtual ~TraverserCallback() {}

        struct SymlinkInfo
        {
            const Zstring& itemName;
            std::int64_t lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
        };

        struct FileInfo
        {
            const Zstring& itemName;
            std::uint64_t fileSize;      //unit: bytes!
            std::int64_t  lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
            const FileId  id;            //optional: empty if not supported!
            const SymlinkInfo* symlinkInfo; //only filled if file is a followed symlink
        };

        struct DirInfo
        {
            const Zstring& itemName;
        };

        enum HandleLink
        {
            LINK_FOLLOW, //dereferences link, then calls "onDir()" or "onFile()"
            LINK_SKIP
        };

        enum HandleError
        {
            ON_ERROR_RETRY,
            ON_ERROR_IGNORE
        };

        virtual void                               onFile   (const FileInfo&    fi) = 0;
        virtual HandleLink                         onSymlink(const SymlinkInfo& si) = 0;
        virtual std::unique_ptr<TraverserCallback> onDir    (const DirInfo&     di) = 0;
        //nullptr: ignore directory, non-nullptr: traverse into, using the (new) callback

        virtual HandleError reportDirError (const std::wstring& msg, size_t retryNumber) = 0; //failed directory traversal -> consider directory data at current level as incomplete!
        virtual HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) = 0; //failed to get data for single file/dir/symlink only!
    };

    //- client needs to handle duplicate file reports! (FilePlusTraverser fallback, retrying to read directory contents, ...)
    static void traverseFolder(const AbstractPathRef& ap, TraverserCallback& sink) { ap.abf->traverseFolder(ap.itemPathImpl, sink); }
    //----------------------------------------------------------------------------------------------------------------

    static bool supportPermissionCopy(const AbstractBaseFolder& baseFolderLeft, const AbstractBaseFolder& baseFolderRight); //throw FileError

    struct FileAttribAfterCopy
    {
        std::uint64_t fileSize;
        std::int64_t modificationTime; //time_t UTC compatible
        FileId sourceFileId;
        FileId targetFileId;
    };
    //return current attributes at the time of copy
    //symlink handling: dereference source
    static FileAttribAfterCopy copyFileAsStream(const AbstractPathRef& apSource, const AbstractPathRef& apTarget, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                                //accummulated delta != file size! consider ADS, sparse, compressed files
                                                const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //may be nullptr; throw X!

    //Note: it MAY happen that copyFileTransactional() leaves temp files behind, e.g. temporary network drop.
    // => clean them up at an appropriate time (automatically set sync directions to delete them). They have the following ending:
    static const Zchar* TEMP_FILE_ENDING; //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

    static FileAttribAfterCopy copyFileTransactional(const AbstractPathRef& apSource, const AbstractPathRef& apTarget, //throw FileError, ErrorFileLocked
                                                     bool copyFilePermissions,
                                                     bool transactionalCopy,
                                                     //if target is existing user needs to implement deletion: copyFile() NEVER overwrites target if already existing!
                                                     //if transactionalCopy == true, full read access on source had been proven at this point, so it's safe to delete it.
                                                     const std::function<void()>& onDeleteTargetFile,
                                                     const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus);

    static void copyNewFolder(const AbstractPathRef& apSource, const AbstractPathRef& apTarget, bool copyFilePermissions); //throw FileError
    static void copySymlink  (const AbstractPathRef& apSource, const AbstractPathRef& apTarget, bool copyFilePermissions); //throw FileError
    static void renameItem   (const AbstractPathRef& apSource, const AbstractPathRef& apTarget); //throw FileError, ErrorTargetExisting, ErrorDifferentVolume

    //----------------------------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------------------------------

    virtual ~AbstractBaseFolder() {}

    AbstractPathRef getAbstractPath(const Zstring& relPath) const
    {
#ifdef ZEN_WIN
        assert(!contains(relPath, L"/")); //relPath is expected to use FILE_NAME_SEPARATOR!
#endif
        assert(relPath.empty() || (!startsWith(relPath, FILE_NAME_SEPARATOR) && !endsWith(relPath, FILE_NAME_SEPARATOR)));

        return AbstractPathRef(*this, appendRelPathToItemPathImpl(getBasePathImpl(), relPath));
    }
    AbstractPathRef getAbstractPath()                       const { return AbstractPathRef(*this, getBasePathImpl()); }

    //limitation: zen::Opt requires default-constructibility => we need to use std::unique_ptr:
    std::unique_ptr<AbstractPathRef> getAbstractPathFromNativePath(const Zstring& nativePath) const { return isNativeFileSystem() ? std::unique_ptr<AbstractPathRef>(new AbstractPathRef(*this, nativePath)) : nullptr; }

    virtual bool emptyBaseFolderPath() const = 0;

    virtual std::uint64_t getFreeDiskSpace() const = 0; //throw FileError, returns 0 if not available

    //----------------------------------------------------------------------------------------------------------------
    virtual bool supportsRecycleBin(const std::function<void ()>& onUpdateGui) const  = 0; //throw FileError

    struct RecycleSession
    {
        virtual ~RecycleSession() {};
        virtual bool recycleItem(const AbstractPathRef& ap, const Zstring& logicalRelPath) = 0; //throw FileError; return true if item existed
        virtual void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus /*optional; currentItem may be empty*/) = 0; //throw FileError
    };
    virtual std::unique_ptr<RecycleSession> createRecyclerSession() const = 0; //throw FileError, precondition: supportsRecycleBin(); return value must be bound!

    static void recycleItemDirectly(const AbstractPathRef& ap) { ap.abf->recycleItemDirectly(ap.itemPathImpl); } //throw FileError

    //================================================================================================================
    //================================================================================================================

    //no need to protect access:
    static const AbstractBaseFolder& getAbf(const AbstractPathRef& ap) { return *ap.abf; }

    static Zstring appendPaths(const Zstring& basePath, const Zstring& relPath, Zchar pathSep);

protected: //grant derived classes access to AbstractPathRef:
    static Zstring getItemPathImpl(const AbstractPathRef& ap) { return ap.itemPathImpl; }
    static AbstractPathRef makeAbstractItem(const AbstractBaseFolder& abfIn, const Zstring& itemPathImplIn) { return AbstractPathRef(abfIn, itemPathImplIn); }

private:
    virtual bool isNativeFileSystem() const { return false; };

    virtual std::wstring getDisplayPath(const Zstring& itemPathImpl) const = 0;

    virtual Zstring appendRelPathToItemPathImpl(const Zstring& itemPathImpl, const Zstring& relPath) const = 0;

    virtual Zstring getBasePathImpl() const = 0;

    //used during folder creation if parent folder is missing
    virtual Opt<Zstring> getParentFolderPathImpl(const Zstring& itemPathImpl) const = 0;

    virtual Zstring getFileShortName(const Zstring& itemPathImpl) const = 0;

    virtual bool lessItemPathSameAbfType(const Zstring& itemPathImplLhs, const AbstractPathRef& apRhs) const = 0;

    virtual bool havePathDependencySameAbfType(const AbstractBaseFolder& other) const = 0;

    //----------------------------------------------------------------------------------------------------------------
    virtual bool fileExists     (const Zstring& itemPathImpl) const = 0; //noexcept
    virtual bool folderExists   (const Zstring& itemPathImpl) const = 0; //noexcept
    virtual bool symlinkExists  (const Zstring& itemPathImpl) const = 0; //noexcept
    virtual bool somethingExists(const Zstring& itemPathImpl) const = 0; //noexcept
    //----------------------------------------------------------------------------------------------------------------

    //should provide for single ATOMIC folder creation!
    virtual void createFolderSimple(const Zstring& itemPathImpl) const = 0; //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing

    //non-recursive folder deletion:
    virtual void removeFolderSimple(const Zstring& itemPathImpl) const = 0; //throw FileError

    virtual bool removeFile(const Zstring& itemPathImpl) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    virtual void setModTime       (const Zstring& itemPathImpl, std::int64_t modificationTime) const = 0; //throw FileError, follows symlinks
    virtual void setModTimeSymlink(const Zstring& itemPathImpl, std::int64_t modificationTime) const = 0; //throw FileError

    virtual AbstractPathRef getResolvedSymlinkPath(const Zstring& itemPathImpl) const = 0; //throw FileError

    virtual Zstring getSymlinkContentBuffer(const Zstring& itemPathImpl) const = 0; //throw FileError

    virtual void recycleItemDirectly(const Zstring& itemPathImpl) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    //- THREAD-SAFETY: must be thread-safe like an int! => no dangling references to this instance!
    virtual IconLoader getAsyncIconLoader(const Zstring& itemPathImpl) const = 0; //noexcept!
    virtual std::function<bool()> /*throw FileError*/ getAsyncCheckFolderExists(const Zstring& itemPathImpl) const = 0; //noexcept
    //----------------------------------------------------------------------------------------------------------------
    virtual std::unique_ptr<InputStream > getInputStream (const Zstring& itemPathImpl) const = 0; //throw FileError, ErrorFileLocked
    virtual std::unique_ptr<OutputStream> getOutputStream(const Zstring& itemPathImpl,  //throw FileError, ErrorTargetExisting
                                                          const std::uint64_t* streamSize,                 //optional
                                                          const std::int64_t* modificationTime) const = 0; //
    //----------------------------------------------------------------------------------------------------------------
    virtual void traverseFolder(const Zstring& itemPathImpl, TraverserCallback& sink) const = 0; //noexcept
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow link!
    virtual FileAttribAfterCopy copyFileForSameAbfType(const Zstring& itemPathImplSource, const AbstractPathRef& apTarget, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                                       //accummulated delta != file size! consider ADS, sparse, compressed files
                                                       const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const = 0; //may be nullptr; throw X!

    //symlink handling: follow link!
    virtual void copyNewFolderForSameAbfType(const Zstring& itemPathImplSource, const AbstractPathRef& apTarget, bool copyFilePermissions) const = 0; //throw FileError
    virtual void copySymlinkForSameAbfType  (const Zstring& itemPathImplSource, const AbstractPathRef& apTarget, bool copyFilePermissions) const = 0; //throw FileError
    virtual void renameItemForSameAbfType   (const Zstring& itemPathImplSource, const AbstractPathRef& apTarget) const = 0; //throw FileError, ErrorTargetExisting, ErrorDifferentVolume
    virtual bool supportsPermissions() const = 0; //throw FileError
};


//implement "retry" in a generic way:
template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingDirError(Command cmd, AbstractBaseFolder::TraverserCallback& callback) //return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const FileError& e)
        {
            switch (callback.reportDirError(e.toString(), retryNumber))
            {
                case AbstractBaseFolder::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractBaseFolder::TraverserCallback::ON_ERROR_IGNORE:
                    return false;
            }
        }
}


template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingItemError(Command cmd, AbstractBaseFolder::TraverserCallback& callback, const Zstring& itemName) //return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const FileError& e)
        {
            switch (callback.reportItemError(e.toString(), retryNumber, itemName))
            {
                case AbstractBaseFolder::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractBaseFolder::TraverserCallback::ON_ERROR_IGNORE:
                    return false;
            }
        }
}








//------------------------------------ implementation -----------------------------------------
struct AbstractPathRef::ItemId
{
    ItemId(const AbstractBaseFolder* abfIn, const Zstring& itemPathImplIn) : abf(abfIn), itemPathImpl(itemPathImplIn) {}

    inline friend bool operator<(const ItemId& lhs, const ItemId& rhs) { return lhs.abf != rhs.abf ? lhs.abf < rhs.abf : lhs.itemPathImpl < rhs.itemPathImpl; }
    //don't treat itemPathImpl like regular file path => no case-insensitive comparison on Windows/OS X!

private:
    const void* abf;
    Zstring itemPathImpl;
};

inline
AbstractPathRef::ItemId AbstractPathRef::getUniqueId() const { return ItemId(abf, itemPathImpl); }


struct AbstractBaseFolder::LessItemPath
{
    bool operator()(const AbstractPathRef& lhs, const AbstractPathRef& rhs) const
    {
        //note: in worst case, order is guaranteed to be stable only during each program run
        return typeid(*lhs.abf) != typeid(*rhs.abf) ? typeid(*lhs.abf).before(typeid(*rhs.abf)) : lhs.abf->lessItemPathSameAbfType(lhs.itemPathImpl, rhs);
        //caveat: typeid returns static type for pointers, dynamic type for references!!!
    }
    bool operator()(const AbstractBaseFolder* lhs, const AbstractBaseFolder* rhs) const
    {
        return (*this)(lhs->getAbstractPath(), rhs->getAbstractPath());
    }
    //bool operator()(const std::shared_ptr<AbstractBaseFolder>& lhs, const std::shared_ptr<AbstractBaseFolder>& rhs) const
    //-> avoid overload ambiguity with "const ABF*" which may erroneously convert to std::shared_ptr!!!
};


inline
bool AbstractBaseFolder::equalItemPath(const AbstractPathRef& lhs, const AbstractPathRef& rhs)
{
    return !LessItemPath()(lhs, rhs) && !LessItemPath()(rhs, lhs);
}


inline
bool AbstractBaseFolder::havePathDependency(const AbstractBaseFolder& lhs, const AbstractBaseFolder& rhs)
{
    return typeid(lhs) != typeid(rhs) ? false : lhs.havePathDependencySameAbfType(rhs);
};


inline
Zstring AbstractBaseFolder::appendPaths(const Zstring& basePath, const Zstring& relPath, Zchar pathSep)
{
    if (relPath.empty())
        return basePath;
    if (basePath.empty())
        return relPath;

    if (startsWith(relPath, pathSep))
    {
        assert(false);
        if (relPath.size() == 1)
            return basePath;

        if (endsWith(basePath, pathSep))
            return basePath + (relPath.c_str() + 1);
    }
    else if (!endsWith(basePath, pathSep))
        return basePath + pathSep + relPath;

    return basePath + relPath;
}

//--------------------------------------------------------------------------

inline
AbstractBaseFolder::OutputStream::OutputStream(const AbstractPathRef& filePath, const std::uint64_t* streamSize) : filePath_(filePath)
{
    if (streamSize)
        bytesExpected = *streamSize;
}


inline
AbstractBaseFolder::OutputStream::~OutputStream()
{
    if (!finalizeSucceeded) //transactional output stream! => clean up!
        try { removeFile(filePath_); /*throw FileError*/ }
        catch (FileError& e) { (void)e; assert(false); }
}


inline
void AbstractBaseFolder::OutputStream::write(const void* buffer, size_t bytesToWrite)
{
    bytesWritten += bytesToWrite;
    writeImpl(buffer, bytesToWrite); //throw FileError
}


inline
AbstractBaseFolder::FileId AbstractBaseFolder::OutputStream::finalize(const std::function<void()>& onUpdateStatus) //throw FileError
{
    if (bytesExpected && bytesWritten != *bytesExpected)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(filePath_))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"), L"%x", numberTo<std::wstring>(*bytesExpected)), L"%y", numberTo<std::wstring>(bytesWritten)));

    const FileId fileId = finalizeImpl(onUpdateStatus); //throw FileError

    finalizeSucceeded = true;
    return fileId;
}

//--------------------------------------------------------------------------

inline
void AbstractBaseFolder::copyNewFolder(const AbstractPathRef& apSource, const AbstractPathRef& apTarget, bool copyFilePermissions) //throw FileError
{
    if (typeid(*apSource.abf) == typeid(*apTarget.abf))
        return apSource.abf->copyNewFolderForSameAbfType(apSource.itemPathImpl, apTarget, copyFilePermissions); //throw FileError

    //fall back:
    if (copyFilePermissions)
        throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(getDisplayPath(apTarget))),
                        _("Operation not supported for different base folder types."));

    createFolderSimple(apTarget); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
}


inline
void AbstractBaseFolder::copySymlink(const AbstractPathRef& apSource, const AbstractPathRef& apTarget, bool copyFilePermissions) //throw FileError
{
    if (typeid(*apSource.abf) == typeid(*apTarget.abf))
        return apSource.abf->copySymlinkForSameAbfType(apSource.itemPathImpl, apTarget, copyFilePermissions); //throw FileError

    throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x",
                                          L"\n" + fmtPath(getDisplayPath(apSource))), L"%y",
                               L"\n" + fmtPath(getDisplayPath(apTarget))), _("Operation not supported for different base folder types."));
}


inline
void AbstractBaseFolder::renameItem(const AbstractPathRef& apSource, const AbstractPathRef& apTarget) //throw FileError, ErrorTargetExisting, ErrorDifferentVolume
{
    if (typeid(*apSource.abf) == typeid(*apTarget.abf))
        return apSource.abf->renameItemForSameAbfType(apSource.itemPathImpl, apTarget); //throw FileError, ErrorTargetExisting, ErrorDifferentVolume

    throw ErrorDifferentVolume(replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x",
                                                     L"\n" + fmtPath(getDisplayPath(apSource))), L"%y",
                                          L"\n" + fmtPath(getDisplayPath(apTarget))), _("Operation not supported for different base folder types."));
}


inline
bool AbstractBaseFolder::supportPermissionCopy(const AbstractBaseFolder& baseFolderLeft, const AbstractBaseFolder& baseFolderRight) //throw FileError
{
    if (typeid(baseFolderLeft) == typeid(baseFolderRight))
        return baseFolderLeft .supportsPermissions() && //throw FileError
               baseFolderRight.supportsPermissions();
    return false;
}
}

#endif //ABSTRACT_FS_873450978453042524534234
