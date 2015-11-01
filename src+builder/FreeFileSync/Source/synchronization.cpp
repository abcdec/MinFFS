// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 7.5         *
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

#include "synchronization.h"
#include <zen/process_priority.h>
#include <zen/perf.h>
#include "lib/db_file.h"
#include "lib/dir_exist_async.h"
#include "lib/status_handler_impl.h"
#include "lib/versioning.h"
#include "lib/binary.h"
#include "fs/concrete.h"

#ifdef ZEN_WIN
    #include <zen/long_path_prefix.h>
    #include "lib/shadow.h"

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <unistd.h> //fsync
    #include <fcntl.h>  //open
#endif

using namespace zen;


namespace
{
inline
int getCUD(const SyncStatistics& stat)
{
    return stat.getCreate() +
           stat.getUpdate() +
           stat.getDelete();
}
}

void SyncStatistics::init()
{
    createLeft  = 0;
    createRight = 0;
    updateLeft  = 0;
    updateRight = 0;
    deleteLeft  = 0;
    deleteRight = 0;
    dataToProcess = 0;
    rowsTotal   = 0;
}


SyncStatistics::SyncStatistics(const FolderComparison& folderCmp)
{
    init();
    std::for_each(begin(folderCmp), end(folderCmp), [&](const BaseDirPair& baseDirObj) { recurse(baseDirObj); });
}


SyncStatistics::SyncStatistics(const HierarchyObject&  hierObj)
{
    init();
    recurse(hierObj);
}


SyncStatistics::SyncStatistics(const FilePair& fileObj)
{
    init();
    processFile(fileObj);
    rowsTotal += 1;
}


inline
void SyncStatistics::recurse(const HierarchyObject& hierObj)
{
    for (const FilePair& fileObj : hierObj.refSubFiles())
        processFile(fileObj);
    for (const SymlinkPair& linkObj : hierObj.refSubLinks())
        processLink(linkObj);
    for (const DirPair& dirObj : hierObj.refSubDirs())
        processDir(dirObj);

    rowsTotal += hierObj.refSubDirs(). size();
    rowsTotal += hierObj.refSubFiles().size();
    rowsTotal += hierObj.refSubLinks().size();
}


inline
void SyncStatistics::processFile(const FilePair& fileObj)
{
    switch (fileObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft;
            dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight;
            dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
            break;

        case SO_DELETE_LEFT:
            ++deleteLeft;
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight;
            break;

        case SO_MOVE_LEFT_TARGET:
            ++updateLeft;
            break;

        case SO_MOVE_RIGHT_TARGET:
            ++updateRight;
            break;

        case SO_MOVE_LEFT_SOURCE:  //ignore; already counted
        case SO_MOVE_RIGHT_SOURCE: //
            break;

        case SO_OVERWRITE_LEFT:
            ++updateLeft;
            dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
            break;

        case SO_OVERWRITE_RIGHT:
            ++updateRight;
            dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs.emplace_back(fileObj.getPairRelativePath(), fileObj.getSyncOpConflict());
            break;

        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft;
            break;

        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight;
            break;

        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }
}


inline
void SyncStatistics::processLink(const SymlinkPair& linkObj)
{
    switch (linkObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft;
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight;
            break;

        case SO_DELETE_LEFT:
            ++deleteLeft;
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight;
            break;

        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft;
            break;

        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight;
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs.emplace_back(linkObj.getPairRelativePath(), linkObj.getSyncOpConflict());
            break;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }
}


inline
void SyncStatistics::processDir(const DirPair& dirObj)
{
    switch (dirObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft;
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight;
            break;

        case SO_DELETE_LEFT: //if deletion variant == user-defined directory existing on other volume, this results in a full copy + delete operation!
            ++deleteLeft;    //however we cannot (reliably) anticipate this situation, fortunately statistics can be adapted during sync!
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight;
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs.emplace_back(dirObj.getPairRelativePath(), dirObj.getSyncOpConflict());
            break;

        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft;
            break;

        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight;
            break;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }

    recurse(dirObj); //since we model logical stats, we recurse, even if deletion variant is "recycler" or "versioning + same volume", which is a single physical operation!
}

//-----------------------------------------------------------------------------------------------------------

std::vector<zen::FolderPairSyncCfg> zen::extractSyncCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs = { mainCfg.firstPair };
    append(allPairs, mainCfg.additionalPairs);

    std::vector<FolderPairSyncCfg> output;

    //process all pairs
    for (const FolderPairEnh& fp : allPairs)
    {
        SyncConfig syncCfg = fp.altSyncConfig.get() ? *fp.altSyncConfig : mainCfg.syncCfg;

        output.push_back(
            FolderPairSyncCfg(syncCfg.directionCfg.var == DirectionConfig::TWOWAY || detectMovedFilesEnabled(syncCfg.directionCfg),
                              syncCfg.handleDeletion,
                              syncCfg.versioningStyle,
                              syncCfg.versioningFolderPhrase,
                              syncCfg.directionCfg.var));
    }
    return output;
}

//------------------------------------------------------------------------------------------------------------

namespace
{
//test if user accidentally selected the wrong folders to sync
bool significantDifferenceDetected(const SyncStatistics& folderPairStat)
{
    //initial file copying shall not be detected as major difference
    if ((folderPairStat.getCreate<LEFT_SIDE >() == 0 ||
         folderPairStat.getCreate<RIGHT_SIDE>() == 0) &&
        folderPairStat.getUpdate  () == 0 &&
        folderPairStat.getDelete  () == 0 &&
        folderPairStat.getConflict() == 0)
        return false;

    const int nonMatchingRows = folderPairStat.getCreate() +
                                folderPairStat.getDelete();
    //folderPairStat.getUpdate() +  -> not relevant when testing for "wrong folder selected"
    //folderPairStat.getConflict();

    return nonMatchingRows >= 10 && nonMatchingRows > 0.5 * folderPairStat.getRowCount();
}

//#################################################################################################################

class DeletionHandling //abstract deletion variants: permanently, recycle bin, user-defined directory
{
public:
    DeletionHandling(ABF& baseFolder,
                     DeletionPolicy handleDel, //nothrow!
                     const Zstring& versioningFolderPhrase,
                     VersioningStyle versioningStyle,
                     const TimeComp& timeStamp,
                     ProcessCallback& procCallback);
    ~DeletionHandling()
    {
        //always (try to) clean up, even if synchronization is aborted!
        try
        {
            tryCleanup(false); //throw FileError, (throw X)
        }
        catch (FileError&) {}
        catch (...) { assert(false); }  //what is this?
        /*
        may block heavily, but still do not allow user callback:
        -> avoid throwing user cancel exception again, leading to incomplete clean-up!
        */
    }

    //clean-up temporary directory (recycle bin optimization)
    void tryCleanup(bool allowUserCallback); //throw FileError; throw X -> call this in non-exceptional coding, i.e. somewhere after sync!

    template <class Function> void removeFileWithCallback (const AbstractPathRef& filePath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //
    template <class Function> void removeDirWithCallback  (const AbstractPathRef& dirPath,  const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //throw FileError
    template <class Function> void removeLinkWithCallback (const AbstractPathRef& linkPath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //

    const std::wstring& getTxtRemovingFile   () const { return txtRemovingFile;      } //
    const std::wstring& getTxtRemovingSymLink() const { return txtRemovingSymlink;   } //buffered status texts
    const std::wstring& getTxtRemovingDir    () const { return txtRemovingDirectory; } //

private:
    DeletionHandling           (const DeletionHandling&) = delete;
    DeletionHandling& operator=(const DeletionHandling&) = delete;

    FileVersioner& getOrCreateVersioner() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DELETE_TO_VERSIONING);
        if (!versioner.get())
            versioner = std::make_unique<FileVersioner>(std::move(versioningFolder), versioningStyle_, timeStamp_); //throw FileError
        return *versioner;
    };

    ABF::RecycleSession& getOrCreateRecyclerSession() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DELETE_TO_RECYCLER);
        if (!recyclerSession.get())
            recyclerSession =  baseFolder_.createRecyclerSession(); //throw FileError
        return *recyclerSession;
    };

    ProcessCallback& procCallback_;

    const DeletionPolicy deletionPolicy_; //keep it invariant! e.g. consider getOrCreateVersioner() one-time construction!

    ABF& baseFolder_;
    std::unique_ptr<ABF::RecycleSession> recyclerSession;

    //used only for DELETE_TO_VERSIONING:
    const VersioningStyle versioningStyle_;
    const TimeComp timeStamp_;
    std::unique_ptr<AbstractBaseFolder> versioningFolder; //bound until first call to getOrCreateVersioner()!!!
    std::unique_ptr<FileVersioner> versioner; //throw FileError in constructor => create on demand!

    //buffer status texts:
    std::wstring txtRemovingFile;
    std::wstring txtRemovingSymlink;
    std::wstring txtRemovingDirectory;

    const std::wstring txtMovingFile;
    const std::wstring txtMovingFolder;
};


DeletionHandling::DeletionHandling(ABF& baseFolder,
                                   DeletionPolicy handleDel, //nothrow!
                                   const Zstring& versioningFolderPhrase,
                                   VersioningStyle versioningStyle,
                                   const TimeComp& timeStamp,
                                   ProcessCallback& procCallback) :
    procCallback_(procCallback),
    deletionPolicy_(handleDel),
    baseFolder_(baseFolder),
    versioningStyle_(versioningStyle),
    timeStamp_(timeStamp),
    txtMovingFile  (_("Moving file %x to %y")),
    txtMovingFolder(_("Moving folder %x to %y"))
{
    switch (deletionPolicy_)
    {
        case DELETE_PERMANENTLY:
            txtRemovingFile      = _("Deleting file %x"         );
            txtRemovingDirectory = _("Deleting folder %x"       );
            txtRemovingSymlink   = _("Deleting symbolic link %x");
            break;

        case DELETE_TO_RECYCLER:
            txtRemovingFile      = _("Moving file %x to the recycle bin"         );
            txtRemovingDirectory = _("Moving folder %x to the recycle bin"       );
            txtRemovingSymlink   = _("Moving symbolic link %x to the recycle bin");
            break;

        case DELETE_TO_VERSIONING:
        {
            versioningFolder = createAbstractBaseFolder(versioningFolderPhrase); //noexcept
            const std::wstring displayPathFmt = fmtPath(ABF::getDisplayPath(versioningFolder->getAbstractPath()));

            txtRemovingFile      = replaceCpy(_("Moving file %x to %y"         ), L"%y", displayPathFmt);
            txtRemovingDirectory = replaceCpy(_("Moving folder %x to %y"       ), L"%y", displayPathFmt);
            txtRemovingSymlink   = replaceCpy(_("Moving symbolic link %x to %y"), L"%y", displayPathFmt);
        }
        break;
    }
}


void DeletionHandling::tryCleanup(bool allowUserCallback) //throw FileError; throw X
{
    switch (deletionPolicy_)
    {
        case DELETE_PERMANENTLY:
            break;

        case DELETE_TO_RECYCLER:
            if (recyclerSession.get())
            {
                auto notifyDeletionStatus = [&](const std::wstring& displayPath)
                {
                    if (!displayPath.empty())
                        procCallback_.reportStatus(replaceCpy(txtRemovingFile, L"%x", fmtPath(displayPath))); //throw ?
                    else
                        procCallback_.requestUiRefresh(); //throw ?
                };

                //move content of temporary directory to recycle bin in a single call
                if (allowUserCallback)
                    getOrCreateRecyclerSession().tryCleanup(notifyDeletionStatus); //throw FileError
                else
                    getOrCreateRecyclerSession().tryCleanup(nullptr); //throw FileError
            }
            break;

        case DELETE_TO_VERSIONING:
            //if (versioner.get())
            //{
            //    if (allowUserCallback)
            //    {
            //        procCallback_.reportStatus(_("Removing old versions...")); //throw ?
            //        versioner->limitVersions([&] { procCallback_.requestUiRefresh(); /*throw ? */ }); //throw FileError
            //    }
            //    else
            //        versioner->limitVersions([] {}); //throw FileError
            //}
            break;
    }
}


template <class Function>
void DeletionHandling::removeDirWithCallback(const AbstractPathRef& dirPath,
                                             const Zstring& relativePath,
                                             Function onNotifyItemDeletion,
                                             const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //throw FileError
{
    switch (deletionPolicy_)
    {
        case DELETE_PERMANENTLY:
        {
            auto notifyDeletion = [&](const std::wstring& statusText, const std::wstring& displayPath)
            {
                onNotifyItemDeletion(); //it would be more correct to report *after* work was done!
                procCallback_.reportStatus(replaceCpy(statusText, L"%x", fmtPath(displayPath)));
            };
            auto onBeforeFileDeletion = [&](const std::wstring& displayPath) { notifyDeletion(txtRemovingFile,      displayPath); };
            auto onBeforeDirDeletion  = [&](const std::wstring& displayPath) { notifyDeletion(txtRemovingDirectory, displayPath); };

            ABF::removeFolderRecursively(dirPath, onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
        }
        break;

        case DELETE_TO_RECYCLER:
            if (getOrCreateRecyclerSession().recycleItem(dirPath, relativePath)) //throw FileError; return true if item existed
                onNotifyItemDeletion(); //moving to recycler is ONE logical operation, irrespective of the number of child elements!
            break;

        case DELETE_TO_VERSIONING:
        {
            auto notifyMove = [&](const std::wstring& statusText, const std::wstring& displayPathFrom, const std::wstring& displayPathTo)
            {
                onNotifyItemDeletion(); //it would be more correct to report *after* work was done!
                procCallback_.reportStatus(replaceCpy(replaceCpy(statusText, L"%x", L"\n" + fmtPath(displayPathFrom)), L"%y", L"\n" + fmtPath(displayPathTo)));
            };
            auto onBeforeFileMove   = [&](const std::wstring& displayPathFrom, const std::wstring& displayPathTo) { notifyMove(txtMovingFile,   displayPathFrom, displayPathTo); };
            auto onBeforeFolderMove = [&](const std::wstring& displayPathFrom, const std::wstring& displayPathTo) { notifyMove(txtMovingFolder, displayPathFrom, displayPathTo); };

            getOrCreateVersioner().revisionFolder(dirPath, relativePath, onBeforeFileMove, onBeforeFolderMove, onNotifyCopyStatus); //throw FileError
        }
        break;
    }
}


template <class Function>
void DeletionHandling::removeFileWithCallback(const AbstractPathRef& filePath,
                                              const Zstring& relativePath,
                                              Function onNotifyItemDeletion,
                                              const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //throw FileError
{
    bool deleted = false;

    if (endsWith(relativePath, ABF::TEMP_FILE_ENDING)) //special rule for .ffs_tmp files: always delete permanently!
        deleted = ABF::removeFile(filePath); //throw FileError
    else
        switch (deletionPolicy_)
        {
            case DELETE_PERMANENTLY:
                deleted = ABF::removeFile(filePath); //throw FileError
                break;

            case DELETE_TO_RECYCLER:
                deleted = getOrCreateRecyclerSession().recycleItem(filePath, relativePath); //throw FileError; return true if item existed
                break;

            case DELETE_TO_VERSIONING:
                deleted = getOrCreateVersioner().revisionFile(filePath, relativePath, onNotifyCopyStatus); //throw FileError
                break;
        }
    if (deleted)
        onNotifyItemDeletion();
}


template <class Function> inline
void DeletionHandling::removeLinkWithCallback(const AbstractPathRef& linkPath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //throw FileError
{
    if (ABF::folderExists(linkPath)) //dir symlink
        return removeDirWithCallback(linkPath, relativePath, onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError
    else //file symlink, broken symlink
        return removeFileWithCallback(linkPath, relativePath, onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError
}

//------------------------------------------------------------------------------------------------------------

/*
  DELETE_PERMANENTLY:   deletion frees space
  DELETE_TO_RECYCLER:   won't free space until recycler is full, but then frees space
  DELETE_TO_VERSIONING: depends on whether versioning folder is on a different volume
-> if deleted item is a followed symlink, no space is freed
-> created/updated/deleted item may be on a different volume than base directory: consider symlinks, junctions!

=> generally assume deletion frees space; may avoid false positive disk space warnings for recycler and versioning
*/
class MinimumDiskSpaceNeeded
{
public:
    static std::pair<std::int64_t, std::int64_t> calculate(const BaseDirPair& baseObj)
    {
        MinimumDiskSpaceNeeded inst;
        inst.recurse(baseObj);
        return std::make_pair(inst.spaceNeededLeft, inst.spaceNeededRight);
    }

private:
    MinimumDiskSpaceNeeded() : spaceNeededLeft(),  spaceNeededRight() {}

    void recurse(const HierarchyObject& hierObj)
    {
        //don't process directories

        //process files
        for (const FilePair& fileObj : hierObj.refSubFiles())
            switch (fileObj.getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_CREATE_NEW_LEFT:
                    spaceNeededLeft += static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_CREATE_NEW_RIGHT:
                    spaceNeededRight += static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft -= static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight -= static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft -= static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
                    spaceNeededLeft += static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight -= static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>());
                    spaceNeededRight += static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DO_NOTHING:
                case SO_EQUAL:
                case SO_UNRESOLVED_CONFLICT:
                case SO_COPY_METADATA_TO_LEFT:
                case SO_COPY_METADATA_TO_RIGHT:
                case SO_MOVE_LEFT_SOURCE:
                case SO_MOVE_RIGHT_SOURCE:
                case SO_MOVE_LEFT_TARGET:
                case SO_MOVE_RIGHT_TARGET:
                    break;
            }

        //symbolic links
        //[...]

        //recurse into sub-dirs
        for (auto& subDir : hierObj.refSubDirs())
            recurse(subDir);
    }

    std::int64_t spaceNeededLeft;
    std::int64_t spaceNeededRight;
};

//----------------------------------------------------------------------------------------

class SynchronizeFolderPair
{
public:
    SynchronizeFolderPair(ProcessCallback& procCallback,
                          bool verifyCopiedFiles,
                          bool copyFilePermissions,
                          bool transactionalFileCopy,
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
                          shadow::ShadowCopy* shadowCopyHandler,
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
#endif
                          DeletionHandling& delHandlingLeft,
                          DeletionHandling& delHandlingRight) :
        procCallback_(procCallback),
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
        shadowCopyHandler_(shadowCopyHandler),
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
#endif
        delHandlingLeft_(delHandlingLeft),
        delHandlingRight_(delHandlingRight),
        verifyCopiedFiles_(verifyCopiedFiles),
        copyFilePermissions_(copyFilePermissions),
        transactionalFileCopy_(transactionalFileCopy),
        txtCreatingFile     (_("Creating file %x"            )),
        txtCreatingLink     (_("Creating symbolic link %x"   )),
        txtCreatingFolder   (_("Creating folder %x"          )),
        txtOverwritingFile  (_("Updating file %x"         )),
        txtOverwritingLink  (_("Updating symbolic link %x")),
        txtVerifying        (_("Verifying file %x"           )),
        txtWritingAttributes(_("Updating attributes of %x"   )),
        txtMovingFile       (_("Moving file %x to %y"))
    {}

    void startSync(BaseDirPair& baseDirObj)
    {
        runZeroPass(baseDirObj);       //first process file moves
        runPass<PASS_ONE>(baseDirObj); //delete files (or overwrite big ones with smaller ones)
        runPass<PASS_TWO>(baseDirObj); //copy rest
    }

private:
    enum PassId
    {
        PASS_ONE, //delete files
        PASS_TWO, //create, modify
        PASS_NEVER //skip
    };

    static PassId getPass(const FilePair&    fileObj);
    static PassId getPass(const SymlinkPair& linkObj);
    static PassId getPass(const DirPair&     dirObj);

    template <SelectedSide side>
    void prepare2StepMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError
    bool createParentDir(FileSystemObject& fsObj); //throw FileError
    template <SelectedSide side>
    void manageFileMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError

    void runZeroPass(HierarchyObject& hierObj);
    template <PassId pass>
    void runPass(HierarchyObject& hierObj);

    void synchronizeFile(FilePair& fileObj);
    template <SelectedSide side> void synchronizeFileInt(FilePair& fileObj, SyncOperation syncOp);

    void synchronizeLink(SymlinkPair& linkObj);
    template <SelectedSide sideTrg> void synchronizeLinkInt(SymlinkPair& linkObj, SyncOperation syncOp);

    void synchronizeFolder(DirPair& dirObj);
    template <SelectedSide sideTrg> void synchronizeFolderInt(DirPair& dirObj, SyncOperation syncOp);

    void reportStatus(const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportStatus(replaceCpy(rawText, L"%x", fmtPath(displayPath))); };
    void reportInfo  (const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportInfo  (replaceCpy(rawText, L"%x", fmtPath(displayPath))); };
    void reportInfo  (const std::wstring& rawText,
                      const std::wstring& displayPath1,
                      const std::wstring& displayPath2) const
    {
        procCallback_.reportInfo(replaceCpy(replaceCpy(rawText, L"%x", L"\n" + fmtPath(displayPath1)), L"%y", L"\n" + fmtPath(displayPath2)));
    };

    ABF::FileAttribAfterCopy copyFileWithCallback(const AbstractPathRef& sourcePath,
                                                  const AbstractPathRef& targetPath,
                                                  const std::function<void()>& onDeleteTargetFile,
                                                  const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const; //throw FileError

    template <SelectedSide side>
    DeletionHandling& getDelHandling();

    ProcessCallback& procCallback_;
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
    shadow::ShadowCopy* shadowCopyHandler_; //optional!
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
#endif
    DeletionHandling& delHandlingLeft_;
    DeletionHandling& delHandlingRight_;

    const bool verifyCopiedFiles_;
    const bool copyFilePermissions_;
    const bool transactionalFileCopy_;

    //preload status texts
    const std::wstring txtCreatingFile;
    const std::wstring txtCreatingLink;
    const std::wstring txtCreatingFolder;
    const std::wstring txtOverwritingFile;
    const std::wstring txtOverwritingLink;
    const std::wstring txtVerifying;
    const std::wstring txtWritingAttributes;
    const std::wstring txtMovingFile;
};

//---------------------------------------------------------------------------------------------------------------

template <> inline
DeletionHandling& SynchronizeFolderPair::getDelHandling<LEFT_SIDE>() { return delHandlingLeft_; }

template <> inline
DeletionHandling& SynchronizeFolderPair::getDelHandling<RIGHT_SIDE>() { return delHandlingRight_; }

/*
__________________________
|Move algorithm, 0th pass|
--------------------------
1. loop over hierarchy and find "move source"

2. check whether parent directory of "move source" is going to be deleted or location of "move source" may lead to name clash with other dir/symlink
   -> no:  delay move until 2nd pass

3. create move target's parent directory recursively + execute move
   do we have name clash?
   -> prepare a 2-step move operation: 1. move source to root and update "move target" accordingly 2. delay move until 2nd pass

4. If any of the operations above did not succeed (even after retry), update statistics and revert to "copy + delete"
   Note: first pass may delete "move source"!!!

__________________
|killer-scenarios|
------------------
propagate the following move sequences:
I) a -> a/a      caveat sync'ing parent directory first leads to circular dependency!

II) a/a -> a     caveat: fixing name clash will remove source!

III) c -> d      caveat: move-sequence needs to be processed in correct order!
     b -> c/b
     a -> b/a
*/

template <class List> inline
bool haveNameClash(const Zstring& shortname, List& m)
{
    return std::any_of(m.begin(), m.end(),
    [&](const typename List::value_type& obj) { return EqualFilePath()(obj.getPairShortName(), shortname); });
}


template <SelectedSide side>
void SynchronizeFolderPair::prepare2StepMove(FilePair& sourceObj,
                                             FilePair& targetObj) //throw FileError
{
    Zstring sourceRelPathTmp = sourceObj.getItemName<side>() + ABF::TEMP_FILE_ENDING;
    //this could still lead to a name-clash in obscure cases, if some file exists on the other side with
    //the very same (.ffs_tmp) name and is copied before the second step of the move is executed
    //good news: even in this pathologic case, this may only prevent the copy of the other file, but not the move

    for (int i = 0;; ++i)
        try
        {
            AbstractPathRef sourcePathTmp = sourceObj.getABF<side>().getAbstractPath(sourceRelPathTmp);

            reportInfo(txtMovingFile,
                       ABF::getDisplayPath(sourceObj.getAbstractPath<side>()),
                       ABF::getDisplayPath(sourcePathTmp));

            ABF::renameItem(sourceObj.getAbstractPath<side>(), sourcePathTmp); //throw FileError, ErrorTargetExisting, (ErrorDifferentVolume)
            break;
        }
        catch (const ErrorTargetExisting&) //repeat until unique name found: no file system race condition!
        {
            if (i == 10) throw; //avoid endless recursion in pathological cases
            sourceRelPathTmp = sourceObj.getItemName<side>() + Zchar('_') + numberTo<Zstring>(i) + ABF::TEMP_FILE_ENDING;
        }

    warn_static("was wenn diff volume: symlink aliasing!") //throw FileError, ErrorDifferentVolume, ErrorTargetExisting

    //update file hierarchy
    const FileDescriptor descrSource(sourceObj.getLastWriteTime <side>(),
                                     sourceObj.getFileSize      <side>(),
                                     sourceObj.getFileId        <side>(),
                                     sourceObj.isFollowedSymlink<side>());

    FilePair& tempFile = sourceObj.root().addSubFile<side>(afterLast(sourceRelPathTmp, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL), descrSource);
    static_assert(IsSameType<FixedList<FilePair>, HierarchyObject::SubFileVec>::value,
                  "ATTENTION: we're adding to the file list WHILE looping over it! This is only working because FixedList iterators are not invalidated by insertion!");
    sourceObj.removeObject<side>(); //remove only *after* evaluating "sourceObj, side"!

    //prepare move in second pass
    tempFile.setSyncDir(side == LEFT_SIDE ? SyncDirection::LEFT : SyncDirection::RIGHT);

    targetObj.setMoveRef(tempFile .getId());
    tempFile .setMoveRef(targetObj.getId());

    //NO statistics update!
    procCallback_.requestUiRefresh(); //may throw
}


bool SynchronizeFolderPair::createParentDir(FileSystemObject& fsObj) //throw FileError, "false" on name clash
{
    if (DirPair* parentDir = dynamic_cast<DirPair*>(&fsObj.parent()))
    {
        if (!createParentDir(*parentDir))
            return false;

        //detect (and try to resolve) file type conflicts: 1. symlinks 2. files
        const Zstring& shortname = parentDir->getPairShortName();
        if (haveNameClash(shortname, parentDir->parent().refSubLinks()) ||
            haveNameClash(shortname, parentDir->parent().refSubFiles()))
            return false;

        //in this context "parentDir" cannot be scheduled for deletion since it contains a "move target"!
        //note: if parentDir were deleted, we'd end up destroying "fsObj"!
        assert(parentDir->getSyncOperation() != SO_DELETE_LEFT &&
               parentDir->getSyncOperation() != SO_DELETE_RIGHT);

        synchronizeFolder(*parentDir); //throw FileError
    }
    return true;
}


template <SelectedSide side>
void SynchronizeFolderPair::manageFileMove(FilePair& sourceObj,
                                           FilePair& targetObj) //throw FileError
{
    assert((sourceObj.getSyncOperation() == SO_MOVE_LEFT_SOURCE  && targetObj.getSyncOperation() == SO_MOVE_LEFT_TARGET  && side == LEFT_SIDE) ||
           (sourceObj.getSyncOperation() == SO_MOVE_RIGHT_SOURCE && targetObj.getSyncOperation() == SO_MOVE_RIGHT_TARGET && side == RIGHT_SIDE));

    const bool sourceWillBeDeleted = [&]() -> bool
    {
        if (DirPair* parentDir = dynamic_cast<DirPair*>(&sourceObj.parent()))
        {
            switch (parentDir->getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_DELETE_LEFT:
                case SO_DELETE_RIGHT:
                    return true; //we need to do something about it
                case SO_MOVE_LEFT_SOURCE:
                case SO_MOVE_RIGHT_SOURCE:
                case SO_MOVE_LEFT_TARGET:
                case SO_MOVE_RIGHT_TARGET:
                case SO_OVERWRITE_LEFT:
                case SO_OVERWRITE_RIGHT:
                case SO_CREATE_NEW_LEFT:
                case SO_CREATE_NEW_RIGHT:
                case SO_DO_NOTHING:
                case SO_EQUAL:
                case SO_UNRESOLVED_CONFLICT:
                case SO_COPY_METADATA_TO_LEFT:
                case SO_COPY_METADATA_TO_RIGHT:
                    break;
            }
        }
        return false;
    }();

    auto haveNameClash = [](const FilePair& fileObj)
    {
        return ::haveNameClash(fileObj.getPairShortName(), fileObj.parent().refSubLinks()) ||
               ::haveNameClash(fileObj.getPairShortName(), fileObj.parent().refSubDirs());
    };

    if (sourceWillBeDeleted || haveNameClash(sourceObj))
    {
        //prepare for move now: - revert to 2-step move on name clashes
        if (haveNameClash(targetObj) ||
            !createParentDir(targetObj)) //throw FileError
            return prepare2StepMove<side>(sourceObj, targetObj); //throw FileError

        //finally start move! this should work now:
        synchronizeFile(targetObj); //throw FileError
        //SynchronizeFolderPair::synchronizeFileInt() is *not* expecting SO_MOVE_LEFT_SOURCE/SO_MOVE_RIGHT_SOURCE => start move from targetObj, not sourceObj!
    }
    //else: sourceObj will not be deleted, and is not standing in the way => delay to second pass
    //note: this case may include new "move sources" from two-step sub-routine!!!
}


//search for file move-operations
void SynchronizeFolderPair::runZeroPass(HierarchyObject& hierObj)
{
    for (FilePair& fileObj : hierObj.refSubFiles())
    {
        const SyncOperation syncOp = fileObj.getSyncOperation();
        switch (syncOp) //evaluate comparison result and sync direction
        {
            case SO_MOVE_LEFT_SOURCE:
            case SO_MOVE_RIGHT_SOURCE:
                if (FilePair* targetObj = dynamic_cast<FilePair*>(FileSystemObject::retrieve(fileObj.getMoveRef())))
                {
                    FilePair* sourceObj = &fileObj;
                    assert(dynamic_cast<FilePair*>(FileSystemObject::retrieve(targetObj->getMoveRef())) == sourceObj);

                    zen::Opt<std::wstring> errMsg = tryReportingError([&]
                    {
                        if (syncOp == SO_MOVE_LEFT_SOURCE)
                            this->manageFileMove<LEFT_SIDE>(*sourceObj, *targetObj); //throw FileError
                        else
                            this->manageFileMove<RIGHT_SIDE>(*sourceObj, *targetObj); //
                    }, procCallback_); //throw X?

                    if (errMsg)
                    {
                        //move operation has failed! We cannot allow to continue and have move source's parent directory deleted, messing up statistics!
                        // => revert to ordinary "copy + delete"

                        auto getStats = [&]() -> std::pair<int, std::int64_t>
                        {
                            SyncStatistics statSrc(*sourceObj);
                            SyncStatistics statTrg(*targetObj);
                            return std::make_pair(getCUD(statSrc) + getCUD(statTrg),
                            statSrc.getDataToProcess() + statTrg.getDataToProcess());
                        };

                        const auto statBefore = getStats();
                        sourceObj->setMoveRef(nullptr);
                        targetObj->setMoveRef(nullptr);
                        const auto statAfter = getStats();
                        //fix statistics total to match "copy + delete"
                        procCallback_.updateTotalData(statAfter.first - statBefore.first, statAfter.second - statBefore.second);
                    }
                }
                else assert(false);
                break;

            case SO_MOVE_LEFT_TARGET:  //it's enough to try each move-pair *once*
            case SO_MOVE_RIGHT_TARGET: //
            case SO_DELETE_LEFT:
            case SO_DELETE_RIGHT:
            case SO_OVERWRITE_LEFT:
            case SO_OVERWRITE_RIGHT:
            case SO_CREATE_NEW_LEFT:
            case SO_CREATE_NEW_RIGHT:
            case SO_DO_NOTHING:
            case SO_EQUAL:
            case SO_UNRESOLVED_CONFLICT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_COPY_METADATA_TO_RIGHT:
                break;
        }
    }

    for (DirPair& dirObj : hierObj.refSubDirs())
        runZeroPass(dirObj); //recurse
}

//---------------------------------------------------------------------------------------------------------------

//1st, 2nd pass requirements:
// - avoid disk space shortage: 1. delete files, 2. overwrite big with small files first
// - support change in type: overwrite file by directory, symlink by file, ect.

inline
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const FilePair& fileObj)
{
    switch (fileObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE;

        case SO_OVERWRITE_LEFT:
            return fileObj.getFileSize<LEFT_SIDE>() > fileObj.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

        case SO_OVERWRITE_RIGHT:
            return fileObj.getFileSize<LEFT_SIDE>() < fileObj.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

        case SO_MOVE_LEFT_SOURCE:  //
        case SO_MOVE_RIGHT_SOURCE: // [!]
            return PASS_NEVER;
        case SO_MOVE_LEFT_TARGET:  //
        case SO_MOVE_RIGHT_TARGET: //make sure 2-step move is processed in second pass, after move *target* parent directory was created!
            return PASS_TWO;

        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


inline
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const SymlinkPair& linkObj)
{
    switch (linkObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE; //make sure to delete symlinks in first pass, and equally named file or dir in second pass: usecase "overwrite symlink with regular file"!

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


inline
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const DirPair& dirObj)
{
    switch (dirObj.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE;

        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


template <SynchronizeFolderPair::PassId pass>
void SynchronizeFolderPair::runPass(HierarchyObject& hierObj)
{
    //synchronize files:
    for (FilePair& fileObj : hierObj.refSubFiles())
        if (pass == this->getPass(fileObj)) //"this->" required by two-pass lookup as enforced by GCC 4.7
            tryReportingError([&] { synchronizeFile(fileObj); }, procCallback_); //throw X?

    //synchronize symbolic links:
    for (SymlinkPair& linkObj : hierObj.refSubLinks())
        if (pass == this->getPass(linkObj))
            tryReportingError([&] { synchronizeLink(linkObj); }, procCallback_); //throw X?

    //synchronize folders:
    for (DirPair& dirObj : hierObj.refSubDirs())
    {
        if (pass == this->getPass(dirObj))
            tryReportingError([&] { synchronizeFolder(dirObj); }, procCallback_); //throw X?

        this->runPass<pass>(dirObj); //recurse
    }
}

//---------------------------------------------------------------------------------------------------------------

inline
Opt<SelectedSide> getTargetDirection(SyncOperation syncOp)
{
    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_DELETE_LEFT:
        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
            return LEFT_SIDE;

        case SO_CREATE_NEW_RIGHT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_RIGHT_TARGET:
            return RIGHT_SIDE;

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            break; //nothing to do
    }
    return NoValue();
}


inline
void SynchronizeFolderPair::synchronizeFile(FilePair& fileObj)
{
    const SyncOperation syncOp = fileObj.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFileInt<LEFT_SIDE>(fileObj, syncOp);
        else
            synchronizeFileInt<RIGHT_SIDE>(fileObj, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFileInt(FilePair& fileObj, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (const DirPair* parentDir = dynamic_cast<DirPair*>(&fileObj.parent()))
                if (parentDir->isEmpty<sideTrg>()) //BaseDirPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            //can't use "getAbstractPath<sideTrg>()" as file name is not available!
            const AbstractPathRef targetPath = fileObj.getABF<sideTrg>().getAbstractPath(fileObj.getRelativePath<sideSrc>());
            reportInfo(txtCreatingFile, ABF::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, fileObj.getFileSize<sideSrc>(), procCallback_);
            try
            {
                auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                const ABF::FileAttribAfterCopy newAttr = copyFileWithCallback(fileObj.getAbstractPath<sideSrc>(),
                                                                              targetPath,
                                                                              nullptr, //no target to delete
                                                                              onNotifyCopyStatus); //throw FileError
                statReporter.reportDelta(1, 0);

                //update FilePair
                fileObj.setSyncedTo<sideTrg>(fileObj.getItemName<sideSrc>(), newAttr.fileSize,
                                             newAttr.modificationTime, //target time set from source
                                             newAttr.modificationTime,
                                             newAttr.targetFileId,
                                             newAttr.sourceFileId,
                                             false, fileObj.isFollowedSymlink<sideSrc>());
            }
            catch (FileError&)
            {
                warn_static("still an error if base dir is missing!")
                //  const Zstring basedir = beforeLast(fileObj.getBaseDirPf<side>(), FILE_NAME_SEPARATOR); //what about C:\ ???
                //if (!dirExists(basedir) ||


                if (!ABF::somethingExists(fileObj.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should error out!
                {
                    //source deleted meanwhile...nothing was done (logical point of view!)
                    fileObj.removeObject<sideSrc>(); //remove only *after* evaluating "fileObj, sideSrc"!
                }
                else
                    throw;
            }
            statReporter.reportFinished();
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingFile(), ABF::getDisplayPath(fileObj.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeFileWithCallback(fileObj.getAbstractPath<sideTrg>(), fileObj.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                warn_static("what if item not found? still an error if base dir is missing; externally deleted otherwise!")

                fileObj.removeObject<sideTrg>(); //update FilePair

                statReporter.reportFinished();
            }
            break;

        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            if (FilePair* moveSource = dynamic_cast<FilePair*>(FileSystemObject::retrieve(fileObj.getMoveRef())))
            {
                FilePair* moveTarget = &fileObj;

                assert((moveSource->getSyncOperation() == SO_MOVE_LEFT_SOURCE  && moveTarget->getSyncOperation() == SO_MOVE_LEFT_TARGET  && sideTrg == LEFT_SIDE) ||
                       (moveSource->getSyncOperation() == SO_MOVE_RIGHT_SOURCE && moveTarget->getSyncOperation() == SO_MOVE_RIGHT_TARGET && sideTrg == RIGHT_SIDE));

                const AbstractPathRef oldItem = moveSource->getAbstractPath<sideTrg>();
                const AbstractPathRef newItem = moveTarget->getABF<sideTrg>().getAbstractPath(moveTarget->getRelativePath<sideSrc>());

                reportInfo(txtMovingFile, ABF::getDisplayPath(oldItem), ABF::getDisplayPath(newItem));
                warn_static("was wenn diff volume: symlink aliasing!")
                ABF::renameItem(oldItem, newItem); //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)

                //update FilePair
                assert(moveSource->getFileSize<sideTrg>() == moveTarget->getFileSize<sideSrc>());
                moveTarget->setSyncedTo<sideTrg>(moveTarget->getItemName<sideSrc>(), moveTarget->getFileSize<sideSrc>(),
                                                 moveSource->getLastWriteTime<sideTrg>(), //awkward naming! moveSource is renamed on "sideTrg" side!
                                                 moveTarget->getLastWriteTime<sideSrc>(),
                                                 moveSource->getFileId<sideTrg>(),
                                                 moveTarget->getFileId<sideSrc>(),
                                                 moveSource->isFollowedSymlink<sideTrg>(),
                                                 moveTarget->isFollowedSymlink<sideSrc>());
                moveSource->removeObject<sideTrg>(); //remove only *after* evaluating "moveSource, sideTrg"!

                procCallback_.updateProcessedData(1, 0);
            }
            else (assert(false));
            break;

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        {
            const AbstractPathRef resolvedTargetPath = fileObj.isFollowedSymlink<sideTrg>() ? //follow link when updating file rather than delete it and replace with regular file!!!
                                                       ABF::getResolvedSymlinkPath(fileObj.getAbstractPath<sideTrg>()) : //throw FileError
                                                       fileObj.getABF<sideTrg>().getAbstractPath(fileObj.getRelativePath<sideSrc>()); //respect differences in case of source object

            reportInfo(txtOverwritingFile, ABF::getDisplayPath(resolvedTargetPath));

            if (fileObj.isFollowedSymlink<sideTrg>()) //since we follow the link, we need to sync case sensitivity of the link manually!
                if (fileObj.getItemName<sideTrg>() != fileObj.getItemName<sideSrc>()) //have difference in case?
                    ABF::renameItem(fileObj.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                    fileObj.getABF<sideTrg>().getAbstractPath(fileObj.getRelativePath<sideSrc>()));

            StatisticsReporter statReporter(1, fileObj.getFileSize<sideSrc>(), procCallback_);

            auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            auto onDeleteTargetFile = [&] //delete target at appropriate time
            {
                reportStatus(this->getDelHandling<sideTrg>().getTxtRemovingFile(), ABF::getDisplayPath(resolvedTargetPath));

                this->getDelHandling<sideTrg>().removeFileWithCallback(resolvedTargetPath, fileObj.getPairRelativePath(), [] {}, onNotifyCopyStatus); //throw FileError;
                //no (logical) item count update desired - but total byte count may change, e.g. move(copy) deleted file to versioning dir

                //fileObj.removeObject<sideTrg>(); -> doesn't make sense for isFollowedSymlink(); "fileObj, sideTrg" evaluated below!

                //if fail-safe file copy is active, then the next operation will be a simple "rename"
                //=> don't risk reportStatus() throwing GuiAbortProcess() leaving the target deleted rather than updated!
                if (!transactionalFileCopy_)
                    reportStatus(txtOverwritingFile, ABF::getDisplayPath(resolvedTargetPath)); //restore status text copy file
            };

            const ABF::FileAttribAfterCopy newAttr = copyFileWithCallback(fileObj.getAbstractPath<sideSrc>(),
                                                                          resolvedTargetPath,
                                                                          onDeleteTargetFile,
                                                                          onNotifyCopyStatus); //throw FileError
            statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

            //update FilePair
            fileObj.setSyncedTo<sideTrg>(fileObj.getItemName<sideSrc>(), newAttr.fileSize,
                                         newAttr.modificationTime, //target time set from source
                                         newAttr.modificationTime,
                                         newAttr.targetFileId,
                                         newAttr.sourceFileId,
                                         fileObj.isFollowedSymlink<sideTrg>(),
                                         fileObj.isFollowedSymlink<sideSrc>());

            statReporter.reportFinished();
        }
        break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            //harmonize with file_hierarchy.cpp::getSyncOpDescription!!

            reportInfo(txtWritingAttributes, ABF::getDisplayPath(fileObj.getAbstractPath<sideTrg>()));

            if (fileObj.getItemName<sideTrg>() != fileObj.getItemName<sideSrc>()) //have difference in case?
                ABF::renameItem(fileObj.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                fileObj.getABF<sideTrg>().getAbstractPath(fileObj.getRelativePath<sideSrc>()));

            if (fileObj.getLastWriteTime<sideTrg>() != fileObj.getLastWriteTime<sideSrc>())
                //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                ABF::setModTime(fileObj.getAbstractPath<sideTrg>(), fileObj.getLastWriteTime<sideSrc>()); //throw FileError

            //-> both sides *should* be completely equal now...
            assert(fileObj.getFileSize<sideTrg>() == fileObj.getFileSize<sideSrc>());
            fileObj.setSyncedTo<sideTrg>(fileObj.getItemName<sideSrc>(), fileObj.getFileSize<sideSrc>(),
                                         fileObj.getLastWriteTime<sideSrc>(), //target time set from source
                                         fileObj.getLastWriteTime<sideSrc>(),
                                         fileObj.getFileId       <sideTrg>(),
                                         fileObj.getFileId       <sideSrc>(),
                                         fileObj.isFollowedSymlink<sideTrg>(),
                                         fileObj.isFollowedSymlink<sideSrc>());

            procCallback_.updateProcessedData(1, 0);
            break;

        case SO_MOVE_LEFT_SOURCE:  //use SO_MOVE_LEFT_TARGET/SO_MOVE_RIGHT_TARGET to execute move:
        case SO_MOVE_RIGHT_SOURCE: //=> makes sure parent directory has been created
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //may throw
}


inline
void SynchronizeFolderPair::synchronizeLink(SymlinkPair& linkObj)
{
    const SyncOperation syncOp = linkObj.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeLinkInt<LEFT_SIDE>(linkObj, syncOp);
        else
            synchronizeLinkInt<RIGHT_SIDE>(linkObj, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeLinkInt(SymlinkPair& linkObj, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (const DirPair* parentDir = dynamic_cast<DirPair*>(&linkObj.parent()))
                if (parentDir->isEmpty<sideTrg>()) //BaseDirPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            const AbstractPathRef targetPath = linkObj.getABF<sideTrg>().getAbstractPath(linkObj.getRelativePath<sideSrc>());
            reportInfo(txtCreatingLink, ABF::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, 0, procCallback_);
            try
            {
                ABF::copySymlink(linkObj.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError
                //update SymlinkPair
                linkObj.setSyncedTo<sideTrg>(linkObj.getItemName<sideSrc>(),
                                             linkObj.getLastWriteTime<sideSrc>(), //target time set from source
                                             linkObj.getLastWriteTime<sideSrc>());

                statReporter.reportDelta(1, 0);
            }
            catch (FileError&)
            {
                warn_static("still an error if base dir is missing!")

                if (ABF::somethingExists(linkObj.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should not be quiet about it!
                    throw;
                //source deleted meanwhile...nothing was done (logical point of view!)
                linkObj.removeObject<sideSrc>();
            }
            statReporter.reportFinished();
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingSymLink(), ABF::getDisplayPath(linkObj.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeLinkWithCallback(linkObj.getAbstractPath<sideTrg>(), linkObj.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                linkObj.removeObject<sideTrg>(); //update SymlinkPair

                statReporter.reportFinished();
            }
            break;

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
            reportInfo(txtOverwritingLink, ABF::getDisplayPath(linkObj.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                //reportStatus(getDelHandling<sideTrg>().getTxtRemovingSymLink(), ABF::getDisplayPath(linkObj.getAbstractPath<sideTrg>()));
                getDelHandling<sideTrg>().removeLinkWithCallback(linkObj.getAbstractPath<sideTrg>(), linkObj.getPairRelativePath(), [] {}, onNotifyCopyStatus); //throw FileError

                //linkObj.removeObject<sideTrg>(); -> "linkObj, sideTrg" evaluated below!

                //=> don't risk reportStatus() throwing GuiAbortProcess() leaving the target deleted rather than updated:
                //reportStatus(txtOverwritingLink, ABF::getDisplayPath(linkObj.getAbstractPath<sideTrg>())); //restore status text

                ABF::copySymlink(linkObj.getAbstractPath<sideSrc>(),
                                 linkObj.getABF<sideTrg>().getAbstractPath(linkObj.getRelativePath<sideSrc>()), //respect differences in case of source object
                                 copyFilePermissions_); //throw FileError

                statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

                //update SymlinkPair
                linkObj.setSyncedTo<sideTrg>(linkObj.getItemName<sideSrc>(),
                                             linkObj.getLastWriteTime<sideSrc>(), //target time set from source
                                             linkObj.getLastWriteTime<sideSrc>());

                statReporter.reportFinished();
            }
            break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, ABF::getDisplayPath(linkObj.getAbstractPath<sideTrg>()));

            if (linkObj.getItemName<sideTrg>() != linkObj.getItemName<sideSrc>()) //have difference in case?
                ABF::renameItem(linkObj.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                linkObj.getABF<sideTrg>().getAbstractPath(linkObj.getRelativePath<sideSrc>()));

            if (linkObj.getLastWriteTime<sideTrg>() != linkObj.getLastWriteTime<sideSrc>())
                //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                ABF::setModTimeSymlink(linkObj.getAbstractPath<sideTrg>(), linkObj.getLastWriteTime<sideSrc>()); //throw FileError

            //-> both sides *should* be completely equal now...
            linkObj.setSyncedTo<sideTrg>(linkObj.getItemName<sideSrc>(),
                                         linkObj.getLastWriteTime<sideSrc>(), //target time set from source
                                         linkObj.getLastWriteTime<sideSrc>());

            procCallback_.updateProcessedData(1, 0);
            break;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //may throw
}


inline
void SynchronizeFolderPair::synchronizeFolder(DirPair& dirObj)
{
    const SyncOperation syncOp = dirObj.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFolderInt<LEFT_SIDE>(dirObj, syncOp);
        else
            synchronizeFolderInt<RIGHT_SIDE>(dirObj, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFolderInt(DirPair& dirObj, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
            if (const DirPair* parentDir = dynamic_cast<DirPair*>(&dirObj.parent()))
                if (parentDir->isEmpty<sideTrg>()) //BaseDirPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            warn_static("save this file access?")
            if (ABF::somethingExists(dirObj.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should error out!
            {
                const AbstractPathRef targetPath = dirObj.getABF<sideTrg>().getAbstractPath(dirObj.getRelativePath<sideSrc>());
                reportInfo(txtCreatingFolder, ABF::getDisplayPath(targetPath));

                try
                {
                    ABF::copyNewFolder(dirObj.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError
                }
                catch (const FileError&) { if (!ABF::folderExists(targetPath)) throw; }

                //update DirPair
                dirObj.setSyncedTo(dirObj.getItemName<sideSrc>());

                procCallback_.updateProcessedData(1, 0);
            }
            else //source deleted meanwhile...nothing was done (logical point of view!) -> uh....what about a temporary network drop???
            {
                warn_static("still an error if base dir is missing!")

                const SyncStatistics subStats(dirObj);
                procCallback_.updateTotalData(-getCUD(subStats) - 1, -subStats.getDataToProcess());

                //remove only *after* evaluating dirObj!!
                dirObj.refSubFiles().clear();   //
                dirObj.refSubLinks().clear();   //update DirPair
                dirObj.refSubDirs ().clear();   //
                dirObj.removeObject<sideSrc>(); //
            }
            break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingDir(), ABF::getDisplayPath(dirObj.getAbstractPath<sideTrg>()));
            {
                const SyncStatistics subStats(dirObj); //counts sub-objects only!

                StatisticsReporter statReporter(1 + getCUD(subStats), subStats.getDataToProcess(), procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeDirWithCallback(dirObj.getAbstractPath<sideTrg>(), dirObj.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                dirObj.refSubFiles().clear();   //
                dirObj.refSubLinks().clear();   //update DirPair
                dirObj.refSubDirs ().clear();   //
                dirObj.removeObject<sideTrg>(); //

                statReporter.reportFinished();
            }
            break;

        case SO_OVERWRITE_LEFT:  //possible: e.g. manually-resolved dir-traversal conflict
        case SO_OVERWRITE_RIGHT: //
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, ABF::getDisplayPath(dirObj.getAbstractPath<sideTrg>()));

            if (dirObj.getItemName<sideTrg>() != dirObj.getItemName<sideSrc>()) //have difference in case?
                ABF::renameItem(dirObj.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                dirObj.getABF<sideTrg>().getAbstractPath(dirObj.getRelativePath<sideSrc>()));
            //copyFileTimes -> useless: modification time changes with each child-object creation/deletion

            //-> both sides *should* be completely equal now...
            dirObj.setSyncedTo(dirObj.getItemName<sideSrc>());

            procCallback_.updateProcessedData(1, 0);
            break;

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //may throw
}

//###########################################################################################

//--------------------- data verification -------------------------
void verifyFiles(const AbstractPathRef& sourcePath, const AbstractPathRef& targetPath, const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)  //throw FileError
{
    try
    {
        //do like "copy /v": 1. flush target file buffers, 2. read again as usual (using OS buffers)
        // => it seems OS buffered are not invalidated by this: snake oil???
        if (Opt<Zstring> nativeTargetPath = ABF::getNativeItemPath(targetPath))
        {
#ifdef ZEN_WIN
            const HANDLE fileHandle = ::CreateFile(applyLongPathPrefix(*nativeTargetPath).c_str(), //_In_      LPCTSTR lpFileName,
                                                   GENERIC_WRITE |        //_In_      DWORD dwDesiredAccess,
                                                   GENERIC_READ,          //=> request read-access, too, just like "copy /v" command
                                                   FILE_SHARE_READ | FILE_SHARE_DELETE,     //_In_      DWORD dwShareMode,
                                                   nullptr,               //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                                   OPEN_EXISTING,         //_In_      DWORD dwCreationDisposition,
                                                   FILE_ATTRIBUTE_NORMAL, //_In_      DWORD dwFlagsAndAttributes,
                                                   nullptr);              //_In_opt_  HANDLE hTemplateFile
            if (fileHandle == INVALID_HANDLE_VALUE)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(*nativeTargetPath)), L"CreateFile");
            ZEN_ON_SCOPE_EXIT(::CloseHandle(fileHandle));

            if (!::FlushFileBuffers(fileHandle))
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(*nativeTargetPath)), L"FlushFileBuffers");

#elif defined ZEN_LINUX || defined ZEN_MAC
            const int fileHandle = ::open(nativeTargetPath->c_str(), O_WRONLY);
            if (fileHandle == -1)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(*nativeTargetPath)), L"open");
            ZEN_ON_SCOPE_EXIT(::close(fileHandle));

            if (::fsync(fileHandle) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(*nativeTargetPath)), L"fsync");
#endif
        } //close file handles!

        if (onUpdateStatus) onUpdateStatus(0);

        if (!filesHaveSameContent(sourcePath, targetPath, onUpdateStatus)) //throw FileError
            throw FileError(replaceCpy(replaceCpy(_("%x and %y have different content."), L"%x", L"\n" +
                                                  fmtPath(ABF::getDisplayPath(sourcePath))), L"%y", L"\n" +
                                       fmtPath(ABF::getDisplayPath(targetPath))));
    }
    catch (const FileError& e) //add some context to error message
    {
        throw FileError(_("Data verification error:"), e.toString());
    }
}


ABF::FileAttribAfterCopy SynchronizeFolderPair::copyFileWithCallback(const AbstractPathRef& sourcePath,  //throw FileError
                                                                     const AbstractPathRef& targetPath,
                                                                     const std::function<void()>& onDeleteTargetFile,
                                                                     const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const //returns current attributes of source file
{
    auto copyOperation = [this, &targetPath, &onDeleteTargetFile, &onNotifyCopyStatus](const AbstractPathRef& sourcePathTmp)
    {
        ABF::FileAttribAfterCopy newAttr = ABF::copyFileTransactional(sourcePathTmp, targetPath, //throw FileError, ErrorFileLocked
                                                                      copyFilePermissions_,
                                                                      transactionalFileCopy_,
                                                                      onDeleteTargetFile,
                                                                      onNotifyCopyStatus);

        //#################### Verification #############################
        if (verifyCopiedFiles_)
        {
            auto guardTarget = makeGuard([&] { ABF::removeFile(targetPath); }); //delete target if verification fails

            procCallback_.reportInfo(replaceCpy(txtVerifying, L"%x", fmtPath(ABF::getDisplayPath(targetPath))));
            verifyFiles(sourcePathTmp, targetPath, [&](std::int64_t bytesDelta) { procCallback_.requestUiRefresh(); }); //throw FileError

            guardTarget.dismiss();
        }
        //#################### /Verification #############################

        return newAttr;
    };

#ifdef ZEN_WIN
    try
    {
        return copyOperation(sourcePath);
    }
    catch (ErrorFileLocked& e1)
    {
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
        //if file is locked (try to) use Windows Volume Shadow Copy Service
        if (shadowCopyHandler_) //if file is locked (try to) use Windows Volume Shadow Copy Service:
            if (Opt<Zstring> nativeSourcePath = ABF::getNativeItemPath(sourcePath))
            {
                Zstring nativeShadowPath; //contains prefix: E.g. "\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy1\Program Files\FFS\sample.dat"
                try
                {
                    nativeShadowPath = shadowCopyHandler_->makeShadowCopy(*nativeSourcePath, //throw FileError
                                                                          [&](const Zstring& volumeName)
                    {
                        procCallback_.reportStatus(replaceCpy(_("Creating a Volume Shadow Copy for %x..."), L"%x", fmtPath(volumeName)));
                    });
                }
                catch (const FileError& e2) //enhance error message
                {
                    throw FileError(e1.toString(), e2.toString());
                }

                //now try again
                if (std::unique_ptr<AbstractPathRef> shadowPath = ABF::getAbf(sourcePath).getAbstractPathFromNativePath(nativeShadowPath))
                    return copyOperation(*shadowPath);
                else assert(false);
            }
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
        throw;
    }
#else
    return copyOperation(sourcePath);
#endif
}

//###########################################################################################

template <SelectedSide side> //create base directories first (if not yet existing) -> no symlink or attribute copying!
bool createBaseDirectory(BaseDirPair& baseDirObj, ProcessCallback& callback) //nothrow; return false if fatal error occurred
{
    if (baseDirObj.getABF<side>().emptyBaseFolderPath())
        return true;

    if (!baseDirObj.isExisting<side>()) //create target directory: user presumably ignored error "dir existing" in order to have it created automatically
    {
        bool temporaryNetworkDrop = false;
        zen::Opt<std::wstring> errMsg = tryReportingError([&]
        {
            const AbstractPathRef baseFolderPath = baseDirObj.getABF<side>().getAbstractPath();
            try
            {
                //a nice race-free check and set operation:
                ABF::createFolderSimple(baseFolderPath); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
                baseDirObj.setExisting<side>(true); //update our model!
            }
            catch (ErrorTargetPathMissing&)
            {
                ABF::createFolderRecursively(baseFolderPath); //throw FileError
                baseDirObj.setExisting<side>(true); //update our model!
            }
            catch (ErrorTargetExisting&)
            {
                //TEMPORARY network drop! base directory not found during comparison, but reappears during synchronization
                //=> sync-directions are based on false assumptions! Abort.
                callback.reportFatalError(replaceCpy(_("Target folder %x already existing."), L"%x", fmtPath(ABF::getDisplayPath(baseFolderPath))));
                temporaryNetworkDrop = true;

                //Is it possible we're catching a "false-positive" here, could FFS have created the directory indirectly after comparison?
                //  1. deletion handling: recycler       -> no, temp directory created only at first deletion
                //  2. deletion handling: versioning     -> "
                //  3. log file creates containing folder -> no, log only created in batch mode, and only *before* comparison
            }
        }, callback); //throw X?
        return !errMsg && !temporaryNetworkDrop;
    }

    return true;
}

struct ReadWriteCount
{
    ReadWriteCount() : reads(), writes() {}
    size_t reads;
    size_t writes;
};

enum class FolderPairJobType
{
    PROCESS,
    ALREADY_IN_SYNC,
    SKIP,
};
}


void zen::synchronize(const TimeComp& timeStamp,
                      xmlAccess::OptionalDialogs& warnings,
                      bool verifyCopiedFiles,
                      bool copyLockedFiles,
                      bool copyFilePermissions,
                      bool transactionalFileCopy,
                      bool runWithBackgroundPriority,
                      const std::vector<FolderPairSyncCfg>& syncConfig,
                      FolderComparison& folderCmp,
                      ProcessCallback& callback)
{
    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        try
        {
            backgroundPrio = std::make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
        }
        catch (const FileError& e) //not an error in this context
        {
            callback.reportInfo(e.toString()); //may throw!
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //not an error in this context
    {
        callback.reportInfo(e.toString()); //may throw!
    }

    //PERF_START;

    if (syncConfig.size() != folderCmp.size())
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //inform about the total amount of data that will be processed from now on
    const SyncStatistics statisticsTotal(folderCmp);

    //keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(getCUD(statisticsTotal), //may throw
                          statisticsTotal.getDataToProcess(),
                          ProcessCallback::PHASE_SYNCHRONIZING);


    std::vector<FolderPairJobType> jobType(folderCmp.size(), FolderPairJobType::PROCESS); //folder pairs may be skipped after fatal errors were found

    //-------------------execute basic checks all at once before starting sync--------------------------------------

    auto dirNotFoundAnymore = [&](const ABF& baseFolder, bool wasExisting)
    {
        if (wasExisting)
            if (Opt<std::wstring> errMsg = tryReportingError([&]
        {
            if (!folderExistsUpdating(baseFolder, false, callback))
                    throw FileError(replaceCpy(_("Cannot find folder %x."), L"%x", fmtPath(ABF::getDisplayPath(baseFolder.getAbstractPath()))));
                //should be logged as a "fatal error" if ignored by the user...
            }, callback)) //throw X?
        return true;

        return false;
    };

    //aggregate information
    std::map<const ABF*, ReadWriteCount, ABF::LessItemPath> dirReadWriteCount; //count read/write accesses
    for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
    {
        //create all entries first! otherwise counting accesses is too complex during later inserts!

        if (!j->getABF<LEFT_SIDE >().emptyBaseFolderPath()) //empty folder is always dependent => exclude!
            dirReadWriteCount[&j->getABF<LEFT_SIDE>()];
        if (!j->getABF<RIGHT_SIDE >().emptyBaseFolderPath())
            dirReadWriteCount[&j->getABF<RIGHT_SIDE>()];
    }

    auto incReadCount = [&](const ABF& baseFolder)
    {
        if (!baseFolder.emptyBaseFolderPath())
            for (auto& item : dirReadWriteCount)
                if (ABF::havePathDependency(baseFolder, *item.first))
                    ++item.second.reads;
    };
    auto incWriteCount = [&](const ABF& baseFolder)
    {
        if (!baseFolder.emptyBaseFolderPath())
            for (auto& item : dirReadWriteCount)
                if (ABF::havePathDependency(baseFolder, *item.first))
                    ++item.second.writes;
    };

    std::vector<std::pair<const ABF*, const ABF*>> significantDiffPairs;

    std::vector<std::pair<const ABF*, std::pair<std::int64_t, std::int64_t>>> diskSpaceMissing; //base folder / space required / space available

    //status of base directories which are set to DELETE_TO_RECYCLER (and contain actual items to be deleted)
    std::map<const ABF*, bool, ABF::LessItemPath> recyclerSupported; //expensive to determine on Win XP => buffer + check recycle bin existence only once per base folder!

    //start checking folder pairs
    for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
    {
        const size_t folderIndex = j - begin(folderCmp);
        const FolderPairSyncCfg& folderPairCfg = syncConfig[folderIndex];

        //exclude some pathological case (leftdir, rightdir are empty)
        if (ABF::equalItemPath(j->getABF<LEFT_SIDE >().getAbstractPath(),
                               j->getABF<RIGHT_SIDE>().getAbstractPath()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //aggregate basic information
        const SyncStatistics folderPairStat(*j);

        const bool writeLeft = folderPairStat.getCreate<LEFT_SIDE>() +
                               folderPairStat.getUpdate<LEFT_SIDE>() +
                               folderPairStat.getDelete<LEFT_SIDE>() > 0;

        const bool writeRight = folderPairStat.getCreate<RIGHT_SIDE>() +
                                folderPairStat.getUpdate<RIGHT_SIDE>() +
                                folderPairStat.getDelete<RIGHT_SIDE>() > 0;

        //skip folder pair if there is nothing to do (except for two-way mode and move-detection, where DB files need to be updated)
        //-> skip creating (not yet existing) base directories in particular if there's no need
        if (!writeLeft && !writeRight)
        {
            jobType[folderIndex] = FolderPairJobType::ALREADY_IN_SYNC;
            continue;
        }

        //aggregate information of folders used by multiple pairs in read/write access
        if (!ABF::havePathDependency(j->getABF<LEFT_SIDE>(), j->getABF<RIGHT_SIDE>())) //true in general
        {
            if (writeLeft && writeRight)
            {
                incWriteCount(j->getABF<LEFT_SIDE>());
                incWriteCount(j->getABF<RIGHT_SIDE>());
            }
            else if (writeLeft)
            {
                incWriteCount(j->getABF<LEFT_SIDE>());
                incReadCount (j->getABF<RIGHT_SIDE>());
            }
            else if (writeRight)
            {
                incReadCount (j->getABF<LEFT_SIDE>());
                incWriteCount(j->getABF<RIGHT_SIDE>());
            }
        }
        else //if folder pair contains two dependent folders, a warning was already issued after comparison; in this context treat as one write access at most
        {
            if (writeLeft || writeRight)
                incWriteCount(j->getABF<LEFT_SIDE>());
        }

        //check for empty target folder paths: this only makes sense if empty field is source (and no DB files need to be created)
        if ((j->getABF<LEFT_SIDE >().emptyBaseFolderPath() && (writeLeft  || folderPairCfg.saveSyncDB_)) ||
            (j->getABF<RIGHT_SIDE>().emptyBaseFolderPath() && (writeRight || folderPairCfg.saveSyncDB_)))
        {
            callback.reportFatalError(_("Target folder input field must not be empty."));
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //check for network drops after comparison
        // - convenience: exit sync right here instead of showing tons of errors during file copy
        // - early failure! there's no point in evaluating subsequent warnings
        if (dirNotFoundAnymore(j->getABF<LEFT_SIDE >(), j->isExisting<LEFT_SIDE >()) ||
            dirNotFoundAnymore(j->getABF<RIGHT_SIDE>(), j->isExisting<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //the following scenario is covered by base directory creation below in case source directory exists (accessible or not), but latter doesn't cover source created after comparison, but before sync!!!
        auto sourceDirNotFound = [&](const ABF& baseFolder, bool wasExisting) -> bool //avoid race-condition: we need to evaluate existence status from time of comparison!
        {
            if (!baseFolder.emptyBaseFolderPath())
                //PERMANENT network drop: avoid data loss when source directory is not found AND user chose to ignore errors (else we wouldn't arrive here)
                if (folderPairStat.getDelete() > 0) //check deletions only... (respect filtered items!)
                    //folderPairStat.getConflict() == 0 && -> there COULD be conflicts for <automatic> if directory existence check fails, but loading sync.ffs_db succeeds
                    //https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3531351&group_id=234430 -> fixed, but still better not consider conflicts!
                    if (!wasExisting) //avoid race-condition: we need to evaluate existence status from time of comparison!
                    {
                        callback.reportFatalError(replaceCpy(_("Source folder %x not found."), L"%x", fmtPath(ABF::getDisplayPath(baseFolder.getAbstractPath()))));
                        return true;
                    }
            return false;
        };
        if (sourceDirNotFound(j->getABF<LEFT_SIDE >(), j->isExisting<LEFT_SIDE >()) ||
            sourceDirNotFound(j->getABF<RIGHT_SIDE>(), j->isExisting<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //check if user-defined directory for deletion was specified
        if (folderPairCfg.handleDeletion == zen::DELETE_TO_VERSIONING &&
            folderPairStat.getUpdate() + folderPairStat.getDelete() > 0)
        {
            if (trimCpy(folderPairCfg.versioningFolderPhrase).empty())
            {
                //should not arrive here: already checked in SyncCfgDialog
                callback.reportFatalError(_("Please enter a target folder for versioning."));
                jobType[folderIndex] = FolderPairJobType::SKIP;
                continue;
            }
        }

        //check if more than 50% of total number of files/dirs are to be created/overwritten/deleted
        if (!j->getABF<LEFT_SIDE >().emptyBaseFolderPath() &&
            !j->getABF<RIGHT_SIDE>().emptyBaseFolderPath())
            if (significantDifferenceDetected(folderPairStat))
                significantDiffPairs.emplace_back(&j->getABF<LEFT_SIDE>(), &j->getABF<RIGHT_SIDE>());

        //check for sufficient free diskspace
        auto checkSpace = [&](const ABF& baseFolder, std::int64_t minSpaceNeeded)
        {
            if (!baseFolder.emptyBaseFolderPath())
                try
                {
                    const std::int64_t freeSpace = baseFolder.getFreeDiskSpace(); //throw FileError, returns 0 if not available

                    if (0 < freeSpace && //zero means "request not supported" (e.g. see WebDav)
                        freeSpace < minSpaceNeeded)
                        diskSpaceMissing.emplace_back(&baseFolder, std::make_pair(minSpaceNeeded, freeSpace));
                }
                catch (FileError&) { assert(false); } //for warning only => no need for tryReportingError()
        };
        const std::pair<std::int64_t, std::int64_t> spaceNeeded = MinimumDiskSpaceNeeded::calculate(*j);
        checkSpace(j->getABF<LEFT_SIDE >(), spaceNeeded.first);
        checkSpace(j->getABF<RIGHT_SIDE>(), spaceNeeded.second);

        //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
        auto checkRecycler = [&](const ABF& baseFolder)
        {
            assert(!baseFolder.emptyBaseFolderPath());
            if (!baseFolder.emptyBaseFolderPath())
                if (recyclerSupported.find(&baseFolder) == recyclerSupported.end()) //perf: avoid duplicate checks!
                {
                    callback.reportStatus(replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x",
                                                     fmtPath(ABF::getDisplayPath(baseFolder.getAbstractPath()))));
                    bool recSupported = false;
                    tryReportingError([&]
                    {
                        recSupported = baseFolder.supportsRecycleBin([&]{ callback.requestUiRefresh(); /*may throw*/ }); //throw FileError
                    }, callback); //throw X?

                    recyclerSupported.emplace(&baseFolder, recSupported);
                }
        };

        if (folderPairCfg.handleDeletion == DELETE_TO_RECYCLER)
        {
            if (folderPairStat.getUpdate<LEFT_SIDE>() +
                folderPairStat.getDelete<LEFT_SIDE>() > 0)
                checkRecycler(j->getABF<LEFT_SIDE>());

            if (folderPairStat.getUpdate<RIGHT_SIDE>() +
                folderPairStat.getDelete<RIGHT_SIDE>() > 0)
                checkRecycler(j->getABF<RIGHT_SIDE>());
        }
    }

    //check if unresolved conflicts exist
    if (statisticsTotal.getConflict() > 0)
    {
        std::wstring msg = _("The following items have unresolved conflicts and will not be synchronized:");

        for (const auto& item : statisticsTotal.getConflictMessages()) //show *all* conflicts in warning message
            msg += L"\n\n" + fmtPath(item.first) + L": " + item.second;

        callback.reportWarning(msg, warnings.warningUnresolvedConflicts);
    }

    //check if user accidentally selected wrong directories for sync
    if (!significantDiffPairs.empty())
    {
        std::wstring msg = _("The following folders are significantly different. Make sure you have selected the correct folders for synchronization.");

        for (const auto& item : significantDiffPairs)
            msg += L"\n\n" +
                   ABF::getDisplayPath(item.first ->getAbstractPath()) + L" <-> " + L"\n" +
                   ABF::getDisplayPath(item.second->getAbstractPath());

        callback.reportWarning(msg, warnings.warningSignificantDifference);
    }

    //check for sufficient free diskspace
    if (!diskSpaceMissing.empty())
    {
        std::wstring msg = _("Not enough free disk space available in:");

        for (const auto& item : diskSpaceMissing)
            msg += L"\n\n" + ABF::getDisplayPath(item.first->getAbstractPath()) + L"\n" +
                   _("Required:")  + L" " + filesizeToShortString(item.second.first)  + L"\n" +
                   _("Available:") + L" " + filesizeToShortString(item.second.second);

        callback.reportWarning(msg, warnings.warningNotEnoughDiskSpace);
    }

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    {
        std::wstring dirListMissingRecycler;
        for (const auto& item : recyclerSupported)
            if (!item.second)
                dirListMissingRecycler += L"\n" + ABF::getDisplayPath(item.first->getAbstractPath());

        if (!dirListMissingRecycler.empty())
            callback.reportWarning(_("The recycle bin is not available for the following folders. Files will be deleted permanently instead:") + L"\n" +
                                   dirListMissingRecycler, warnings.warningRecyclerMissing);
    }

    //check if folders are used by multiple pairs in read/write access
    {
        std::vector<const ABF*> conflictFolders;
        for (const auto& item : dirReadWriteCount)
            if (item.second.reads + item.second.writes >= 2 && item.second.writes >= 1) //race condition := multiple accesses of which at least one is a write
                conflictFolders.push_back(item.first);

        if (!conflictFolders.empty())
        {
            std::wstring msg = _("Multiple folder pairs write to a common subfolder. Please review your configuration.") + L"\n";
            for (const ABF* baseFolder : conflictFolders)
                msg += L"\n" + ABF::getDisplayPath(baseFolder->getAbstractPath());

            callback.reportWarning(msg, warnings.warningFolderPairRaceCondition);
        }
    }

    //-------------------end of basic checks------------------------------------------

#ifdef ZEN_WIN
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
    //shadow copy buffer: per sync-instance, not folder pair
    std::unique_ptr<shadow::ShadowCopy> shadowCopyHandler;
    if (copyLockedFiles)
        shadowCopyHandler = std::make_unique<shadow::ShadowCopy>();
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
#endif

    try
    {
        //loop through all directory pairs
        for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
        {
            const size_t folderIndex = j - begin(folderCmp);
            const FolderPairSyncCfg& folderPairCfg = syncConfig[folderIndex];

            if (jobType[folderIndex] == FolderPairJobType::SKIP) //folder pairs may be skipped after fatal errors were found
                continue;

            //------------------------------------------------------------------------------------------
            callback.reportInfo(_("Synchronizing folder pair:") + L" [" + getVariantName(folderPairCfg.syncVariant_) + L"]\n" +
                                L"    " + ABF::getDisplayPath(j->getABF<LEFT_SIDE >().getAbstractPath()) + L"\n" +
                                L"    " + ABF::getDisplayPath(j->getABF<RIGHT_SIDE>().getAbstractPath()));
            //------------------------------------------------------------------------------------------

            //checking a second time: (a long time may have passed since the intro checks!)
            if (dirNotFoundAnymore(j->getABF<LEFT_SIDE >(), j->isExisting<LEFT_SIDE >()) ||
                dirNotFoundAnymore(j->getABF<RIGHT_SIDE>(), j->isExisting<RIGHT_SIDE>()))
                continue;

            //create base directories first (if not yet existing) -> no symlink or attribute copying!
            if (!createBaseDirectory<LEFT_SIDE >(*j, callback) ||
                !createBaseDirectory<RIGHT_SIDE>(*j, callback))
                continue;

            //------------------------------------------------------------------------------------------
            //execute synchronization recursively

            //update synchronization database (automatic sync only)
            ScopeGuard guardUpdateDb = makeGuard([&]
            {
                if (folderPairCfg.saveSyncDB_)
                    try { zen::saveLastSynchronousState(*j, nullptr); } //throw FileError
                    catch (FileError&) {}
            });

            if (jobType[folderIndex] == FolderPairJobType::PROCESS)
            {
                //guarantee removal of invalid entries (where element is empty on both sides)
                ZEN_ON_SCOPE_EXIT(BaseDirPair::removeEmpty(*j));

                bool copyPermissionsFp = false;
                tryReportingError([&]
                {
                    copyPermissionsFp = copyFilePermissions && //copy permissions only if asked for and supported by *both* sides!
                    !j->getABF<LEFT_SIDE >().emptyBaseFolderPath() && //scenario: directory selected on one side only
                    !j->getABF<RIGHT_SIDE>().emptyBaseFolderPath() && //
                    ABF::supportPermissionCopy(j->getABF<LEFT_SIDE>(), j->getABF<RIGHT_SIDE>()); //throw FileError
                }, callback); //throw X?


                auto getEffectiveDeletionPolicy = [&](const ABF& baseFolder) -> DeletionPolicy
                {
                    if (folderPairCfg.handleDeletion == DELETE_TO_RECYCLER)
                    {
                        auto it = recyclerSupported.find(&baseFolder);
                        if (it != recyclerSupported.end()) //buffer filled during intro checks (but only if deletions are expected)
                            if (!it->second)
                                return DELETE_PERMANENTLY; //Windows' ::SHFileOperation() will do this anyway, but we have a better and faster deletion routine (e.g. on networks)
                    }
                    return folderPairCfg.handleDeletion;
                };


                DeletionHandling delHandlerL(j->getABF<LEFT_SIDE>(),
                                             getEffectiveDeletionPolicy(j->getABF<LEFT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);

                DeletionHandling delHandlerR(j->getABF<RIGHT_SIDE>(),
                                             getEffectiveDeletionPolicy(j->getABF<RIGHT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);


                SynchronizeFolderPair syncFP(callback, verifyCopiedFiles, copyPermissionsFp, transactionalFileCopy,
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_SHADOW_COPY_ENABLE
                                             shadowCopyHandler.get(),
#endif//TODO_MinFFS_SHADOW_COPY_ENABLE
#endif
                                             delHandlerL, delHandlerR);
                syncFP.startSync(*j);

                //(try to gracefully) cleanup temporary Recycle bin folders and versioning -> will be done in ~DeletionHandling anyway...
                tryReportingError([&] { delHandlerL.tryCleanup(true /*allowUserCallback*/); /*throw FileError*/}, callback); //throw X?
                tryReportingError([&] { delHandlerR.tryCleanup(true                      ); /*throw FileError*/}, callback); //throw X?
            }

            //(try to gracefully) write database file
            if (folderPairCfg.saveSyncDB_)
            {
                const std::wstring dbUpdateMsg = _("Generating database...");

                callback.reportStatus(dbUpdateMsg);
                callback.forceUiRefresh();

                tryReportingError([&]
                {
                    std::int64_t bytesWritten = 0;
                    zen::saveLastSynchronousState(*j, [&](std::int64_t bytesDelta) //throw FileError
                    {
                        bytesWritten += bytesDelta;
                        callback.reportStatus(dbUpdateMsg + L" (" + filesizeToShortString(bytesWritten) + L")"); //throw X
                    });
                }, callback); //throw X?

                guardUpdateDb.dismiss();
            }
        }
    }
    catch (const std::exception& e)
    {
        callback.reportFatalError(utfCvrtTo<std::wstring>(e.what()));
    }
}
