// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl.html       *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "abstract.h"

using namespace zen;
using AFS = AbstractFileSystem;

const Zchar* AFS::TEMP_FILE_ENDING = Zstr(".ffs_tmp");


AFS::FileAttribAfterCopy AFS::copyFileAsStream(const Zstring& itemPathImplSource, const AbstractPath& apTarget, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                               const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const
{
    auto streamIn = getInputStream(itemPathImplSource); //throw FileError, ErrorFileLocked
    if (onNotifyCopyStatus) onNotifyCopyStatus(0); //throw X!

    std::uint64_t      bytesWritten = 0;
    std::uint64_t      fileSizeExpected = streamIn->getFileSize        (); //throw FileError
    const std::int64_t modificationTime = streamIn->getModificationTime(); //throw FileError
    const FileId       sourceFileId     = streamIn->getFileId          (); //throw FileError
    FileId             targetFileId;

    auto streamOut = getOutputStream(apTarget, &fileSizeExpected, &modificationTime); //throw FileError, ErrorTargetExisting
    if (onNotifyCopyStatus) onNotifyCopyStatus(0); //throw X!

    std::vector<char> buffer(std::min(streamIn ->optimalBlockSize(),
                                      streamOut->optimalBlockSize()));
    for (;;)
    {
        const size_t bytesRead = streamIn->read(&buffer[0], buffer.size()); //throw FileError
        assert(bytesRead <= buffer.size());

        streamOut->write(&buffer[0], bytesRead); //throw FileError
        bytesWritten += bytesRead;

        if (onNotifyCopyStatus)
            onNotifyCopyStatus(bytesRead); //throw X!

        if (bytesRead != buffer.size()) //end of file
            break;
    }

    //important check: catches corrupt sftp download with libssh2!
    if (bytesWritten != fileSizeExpected)
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getDisplayPath(itemPathImplSource))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(fileSizeExpected)),
                                   L"%y", numberTo<std::wstring>(bytesWritten)));

    //modification time should be set here!
    targetFileId = streamOut->finalize([&] { if (onNotifyCopyStatus) onNotifyCopyStatus(0); /*throw X*/ }); //throw FileError

    AFS::FileAttribAfterCopy attr;
    attr.fileSize         = bytesWritten;
    attr.modificationTime = modificationTime;
    attr.sourceFileId     = sourceFileId;
    attr.targetFileId     = targetFileId;
    return attr;
}


AFS::FileAttribAfterCopy AFS::copyFileTransactional(const AbstractPath& apSource, const AbstractPath& apTarget, //throw FileError, ErrorFileLocked
                                                    bool copyFilePermissions,
                                                    bool transactionalCopy,
                                                    const std::function<void()>& onDeleteTargetFile,
                                                    const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus)
{
    auto copyFileBestEffort = [&](const AbstractPath& apTargetTmp)
    {
        //caveat: typeid returns static type for pointers, dynamic type for references!!!
        if (typeid(*apSource.afs) == typeid(*apTarget.afs))
            return apSource.afs->copyFileForSameAfsType(apSource.itemPathImpl, apTargetTmp, copyFilePermissions, onNotifyCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked

        //fall back to stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTargetTmp))),
                            _("Operation not supported for different base folder types."));

        return AFS::copyFileAsStream(apSource, apTargetTmp, onNotifyCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    };

    if (transactionalCopy)
    {
        AbstractPath apTargetTmp(apTarget.afs, apTarget.itemPathImpl + TEMP_FILE_ENDING);
        AFS::FileAttribAfterCopy attr;

        for (int i = 0;; ++i)
            try
            {
                attr = copyFileBestEffort(apTargetTmp); //throw FileError, ErrorTargetExisting, ErrorFileLocked
                break;
            }
            catch (const ErrorTargetExisting&) //optimistic strategy: assume everything goes well, but recover on error -> minimize file accesses
            {
                if (i == 10) throw; //avoid endless recursion in pathological cases, e.g. https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/36adac33
                apTargetTmp.itemPathImpl = apTarget.itemPathImpl + Zchar('_') + numberTo<Zstring>(i) + TEMP_FILE_ENDING;
            }

        //transactional behavior: ensure cleanup; not needed before copyFileBestEffort() which is already transactional
        ZEN_ON_SCOPE_FAIL( try { AFS::removeFile(apTargetTmp); }
        catch (FileError&) {} );

        //have target file deleted (after read access on source and target has been confirmed) => allow for almost transactional overwrite
        if (onDeleteTargetFile)
            onDeleteTargetFile(); //throw X

        //perf: this call is REALLY expensive on unbuffered volumes! ~40% performance decrease on FAT USB stick!
        renameItem(apTargetTmp, apTarget); //throw FileError

        /*
        CAVEAT on FAT/FAT32: the sequence of deleting the target file and renaming "file.txt.ffs_tmp" to "file.txt" does
        NOT PRESERVE the creation time of the .ffs_tmp file, but SILENTLY "reuses" whatever creation time the old "file.txt" had!
        This "feature" is called "File System Tunneling":
        http://blogs.msdn.com/b/oldnewthing/archive/2005/07/15/439261.aspx
        http://support.microsoft.com/kb/172190/en-us
        */
        return attr;
    }
    else
    {
        /*
           Note: non-transactional file copy solves at least four problems:
                -> skydrive - doesn't allow for .ffs_tmp extension and returns ERROR_INVALID_PARAMETER
                -> network renaming issues
                -> allow for true delete before copy to handle low disk space problems
                -> higher performance on non-buffered drives (e.g. usb sticks)
        */
        if (onDeleteTargetFile)
            onDeleteTargetFile();

        return copyFileBestEffort(apTarget); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
}


void AFS::createFolderRecursively(const AbstractPath& ap) //throw FileError
{
    try
    {
        AFS::createFolderSimple(ap); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
    }
    catch (ErrorTargetExisting&) {}
    catch (ErrorTargetPathMissing&)
    {
        if (Opt<AbstractPath> parentPath = AFS::getParentFolderPath(ap))
        {
            //recurse...
            createFolderRecursively(*parentPath); //throw FileError

            //now try again...
            AFS::createFolderSimple(ap); //throw FileError, (ErrorTargetExisting), (ErrorTargetPathMissing)
            return;
        }
        throw;
    }
}


namespace
{
struct FlatTraverserCallback: public AFS::TraverserCallback
{
    FlatTraverserCallback(const AbstractPath& folderPath) : folderPath_(folderPath) {}

    void                               onFile   (const FileInfo&    fi) override { fileNames_  .push_back(fi.itemName); }
    std::unique_ptr<TraverserCallback> onDir    (const DirInfo&     di) override { folderNames_.push_back(di.itemName); return nullptr; }
    HandleLink                         onSymlink(const SymlinkInfo& si) override
    {
        if (AFS::folderExists(AFS::appendRelPath(folderPath_, si.itemName))) //dir symlink
            folderLinkNames_.push_back(si.itemName);
        else //file symlink, broken symlink
            fileNames_.push_back(si.itemName);
        return TraverserCallback::LINK_SKIP;
    }
    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { throw FileError(msg); }

    const std::vector<Zstring>& refFileNames      () const { return fileNames_; }
    const std::vector<Zstring>& refFolderNames    () const { return folderNames_; }
    const std::vector<Zstring>& refFolderLinkNames() const { return folderLinkNames_; }

private:
    const AbstractPath folderPath_;
    std::vector<Zstring> fileNames_;
    std::vector<Zstring> folderNames_;
    std::vector<Zstring> folderLinkNames_;
};


void removeFolderRecursivelyImpl(const AbstractPath& folderPath, //throw FileError
                                 const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion, //optional
                                 const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) //one call for each *existing* object!
{
    assert(!AFS::symlinkExists(folderPath)); //[!] no symlinks in this context!!!
    assert(AFS::folderExists(folderPath));   //Do NOT traverse into it deleting contained files!!!

    FlatTraverserCallback ft(folderPath); //deferred recursion => save stack space and allow deletion of extremely deep hierarchies!
    AFS::traverseFolder(folderPath, ft); //throw FileError

    for (const Zstring& fileName : ft.refFileNames())
    {
        const AbstractPath filePath = AFS::appendRelPath(folderPath, fileName);
        if (onBeforeFileDeletion)
            onBeforeFileDeletion(AFS::getDisplayPath(filePath));

        AFS::removeFile(filePath); //throw FileError
    }

    for (const Zstring& folderLinkName : ft.refFolderLinkNames())
    {
        const AbstractPath linkPath = AFS::appendRelPath(folderPath, folderLinkName);
        if (onBeforeFolderDeletion)
            onBeforeFolderDeletion(AFS::getDisplayPath(linkPath));

        AFS::removeFolderSimple(linkPath); //throw FileError
    }

    for (const Zstring& folderName : ft.refFolderNames())
        removeFolderRecursivelyImpl(AFS::appendRelPath(folderPath, folderName), //throw FileError
                                    onBeforeFileDeletion, onBeforeFolderDeletion);

    if (onBeforeFolderDeletion)
        onBeforeFolderDeletion(AFS::getDisplayPath(folderPath));

    AFS::removeFolderSimple(folderPath); //throw FileError
}
}


void AFS::removeFolderRecursively(const AbstractPath& ap, //throw FileError
                                  const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion, //optional
                                  const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) //one call for each *existing* object!
{
    if (AFS::symlinkExists(ap))
    {
        if (onBeforeFolderDeletion)
            onBeforeFolderDeletion(AFS::getDisplayPath(ap));

        AFS::removeFolderSimple(ap); //throw FileError
    }
    else
    {
        //no error situation if directory is not existing! manual deletion relies on it!
        if (AFS::somethingExists(ap))
            removeFolderRecursivelyImpl(ap, onBeforeFileDeletion, onBeforeFolderDeletion); //throw FileError
    }
}
