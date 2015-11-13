// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
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
#include "fs/native.h"

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
    return stat.createCount() +
           stat.updateCount() +
           stat.deleteCount();
}
}


SyncStatistics::SyncStatistics(const FolderComparison& folderCmp)
{
    std::for_each(begin(folderCmp), end(folderCmp), [&](const BaseFolderPair& baseFolder) { recurse(baseFolder); });
}


SyncStatistics::SyncStatistics(const HierarchyObject& hierObj)
{
    recurse(hierObj);
}


SyncStatistics::SyncStatistics(const FilePair& file)
{
    processFile(file);
    rowsTotal += 1;
}


inline
void SyncStatistics::recurse(const HierarchyObject& hierObj)
{
    for (const FilePair& file : hierObj.refSubFiles())
        processFile(file);
    for (const SymlinkPair& link : hierObj.refSubLinks())
        processLink(link);
    for (const FolderPair& folder : hierObj.refSubFolders())
        processFolder(folder);

    rowsTotal += hierObj.refSubFolders().size();
    rowsTotal += hierObj.refSubFiles  ().size();
    rowsTotal += hierObj.refSubLinks  ().size();
}


inline
void SyncStatistics::processFile(const FilePair& file)
{
    switch (file.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft;
            dataToProcess += static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight;
            dataToProcess += static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
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
            dataToProcess += static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
            break;

        case SO_OVERWRITE_RIGHT:
            ++updateRight;
            dataToProcess += static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs.emplace_back(file.getPairRelativePath(), file.getSyncOpConflict());
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
void SyncStatistics::processLink(const SymlinkPair& link)
{
    switch (link.getSyncOperation()) //evaluate comparison result and sync direction
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
            conflictMsgs.emplace_back(link.getPairRelativePath(), link.getSyncOpConflict());
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
void SyncStatistics::processFolder(const FolderPair& folder)
{
    switch (folder.getSyncOperation()) //evaluate comparison result and sync direction
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
            conflictMsgs.emplace_back(folder.getPairRelativePath(), folder.getSyncOpConflict());
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

    recurse(folder); //since we model logical stats, we recurse, even if deletion variant is "recycler" or "versioning + same volume", which is a single physical operation!
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
    if ((folderPairStat.createCount<LEFT_SIDE >() == 0 ||
         folderPairStat.createCount<RIGHT_SIDE>() == 0) &&
        folderPairStat.updateCount  () == 0 &&
        folderPairStat.deleteCount  () == 0 &&
        folderPairStat.conflictCount() == 0)
        return false;

    const int nonMatchingRows = folderPairStat.createCount() +
                                folderPairStat.deleteCount();
    //folderPairStat.updateCount() +  -> not relevant when testing for "wrong folder selected"
    //folderPairStat.conflictCount();

    return nonMatchingRows >= 10 && nonMatchingRows > 0.5 * folderPairStat.rowCount();
}

//#################################################################################################################

class DeletionHandling //abstract deletion variants: permanently, recycle bin, user-defined directory
{
public:
    DeletionHandling(const AbstractPath& baseFolderPath,
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

    template <class Function> void removeFileWithCallback (const AbstractPath& filePath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //
    template <class Function> void removeDirWithCallback  (const AbstractPath& dirPath,  const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //throw FileError
    template <class Function> void removeLinkWithCallback (const AbstractPath& linkPath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //

    const std::wstring& getTxtRemovingFile   () const { return txtRemovingFile;      } //
    const std::wstring& getTxtRemovingSymLink() const { return txtRemovingSymlink;   } //buffered status texts
    const std::wstring& getTxtRemovingDir    () const { return txtRemovingDirectory; } //

private:
    DeletionHandling           (const DeletionHandling&) = delete;
    DeletionHandling& operator=(const DeletionHandling&) = delete;

    AFS::RecycleSession& getOrCreateRecyclerSession() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DELETE_TO_RECYCLER);
        if (!recyclerSession.get())
            recyclerSession =  AFS::createRecyclerSession(baseFolderPath_); //throw FileError
        return *recyclerSession;
    }

    FileVersioner& getOrCreateVersioner() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DELETE_TO_VERSIONING);
        if (!versioner.get())
            versioner = std::make_unique<FileVersioner>(versioningFolderPath, versioningStyle_, timeStamp_); //throw FileError
        return *versioner;
    }

    ProcessCallback& procCallback_;

    const DeletionPolicy deletionPolicy_; //keep it invariant! e.g. consider getOrCreateVersioner() one-time construction!

    const AbstractPath baseFolderPath_;
    std::unique_ptr<AFS::RecycleSession> recyclerSession;

    //used only for DELETE_TO_VERSIONING:
    const AbstractPath versioningFolderPath;
    const VersioningStyle versioningStyle_;
    const TimeComp timeStamp_;
    std::unique_ptr<FileVersioner> versioner; //throw FileError in constructor => create on demand!

    //buffer status texts:
    std::wstring txtRemovingFile;
    std::wstring txtRemovingSymlink;
    std::wstring txtRemovingDirectory;

    const std::wstring txtMovingFile;
    const std::wstring txtMovingFolder;
};


DeletionHandling::DeletionHandling(const AbstractPath& baseFolderPath,
                                   DeletionPolicy handleDel, //nothrow!
                                   const Zstring& versioningFolderPhrase,
                                   VersioningStyle versioningStyle,
                                   const TimeComp& timeStamp,
                                   ProcessCallback& procCallback) :
    procCallback_(procCallback),
    deletionPolicy_(handleDel),
    baseFolderPath_(baseFolderPath),
    versioningFolderPath(createAbstractPath(versioningFolderPhrase)),
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
            txtRemovingFile      = replaceCpy(_("Moving file %x to %y"         ), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath)));
            txtRemovingDirectory = replaceCpy(_("Moving folder %x to %y"       ), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath)));
            txtRemovingSymlink   = replaceCpy(_("Moving symbolic link %x to %y"), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath)));
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
void DeletionHandling::removeDirWithCallback(const AbstractPath& folderPath,
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

            AFS::removeFolderRecursively(folderPath, onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
        }
        break;

        case DELETE_TO_RECYCLER:
            if (getOrCreateRecyclerSession().recycleItem(folderPath, relativePath)) //throw FileError; return true if item existed
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

            getOrCreateVersioner().revisionFolder(folderPath, relativePath, onBeforeFileMove, onBeforeFolderMove, onNotifyCopyStatus); //throw FileError
        }
        break;
    }
}


template <class Function>
void DeletionHandling::removeFileWithCallback(const AbstractPath& filePath,
                                              const Zstring& relativePath,
                                              Function onNotifyItemDeletion,
                                              const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //throw FileError
{
    bool deleted = false;

    if (endsWith(relativePath, AFS::TEMP_FILE_ENDING)) //special rule for .ffs_tmp files: always delete permanently!
        deleted = AFS::removeFile(filePath); //throw FileError
    else
        switch (deletionPolicy_)
        {
            case DELETE_PERMANENTLY:
                deleted = AFS::removeFile(filePath); //throw FileError
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
void DeletionHandling::removeLinkWithCallback(const AbstractPath& linkPath, const Zstring& relativePath, Function onNotifyItemDeletion, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //throw FileError
{
    if (AFS::folderExists(linkPath)) //dir symlink
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
    static std::pair<std::int64_t, std::int64_t> calculate(const BaseFolderPair& baseFolder)
    {
        MinimumDiskSpaceNeeded inst;
        inst.recurse(baseFolder);
        return std::make_pair(inst.spaceNeededLeft, inst.spaceNeededRight);
    }

private:
    MinimumDiskSpaceNeeded() : spaceNeededLeft(),  spaceNeededRight() {}

    void recurse(const HierarchyObject& hierObj)
    {
        //don't process directories

        //process files
        for (const FilePair& file : hierObj.refSubFiles())
            switch (file.getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_CREATE_NEW_LEFT:
                    spaceNeededLeft += static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_CREATE_NEW_RIGHT:
                    spaceNeededRight += static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft -= static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight -= static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft -= static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
                    spaceNeededLeft += static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight -= static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>());
                    spaceNeededRight += static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>());
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
        for (const FolderPair& folder : hierObj.refSubFolders())
            recurse(folder);
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
                          shadow::ShadowCopy* shadowCopyHandler,
#endif
                          DeletionHandling& delHandlingLeft,
                          DeletionHandling& delHandlingRight) :
        procCallback_(procCallback),
#ifdef ZEN_WIN
        shadowCopyHandler_(shadowCopyHandler),
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

    void startSync(BaseFolderPair& baseFolder)
    {
        runZeroPass(baseFolder);       //first process file moves
        runPass<PASS_ONE>(baseFolder); //delete files (or overwrite big ones with smaller ones)
        runPass<PASS_TWO>(baseFolder); //copy rest
    }

private:
    enum PassId
    {
        PASS_ONE, //delete files
        PASS_TWO, //create, modify
        PASS_NEVER //skip
    };

    static PassId getPass(const FilePair&    file);
    static PassId getPass(const SymlinkPair& link);
    static PassId getPass(const FolderPair&  folder);

    template <SelectedSide side>
    void prepare2StepMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError
    bool createParentFolder(FileSystemObject& fsObj); //throw FileError
    template <SelectedSide side>
    void manageFileMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError

    void runZeroPass(HierarchyObject& hierObj);
    template <PassId pass>
    void runPass(HierarchyObject& hierObj);

    void synchronizeFile(FilePair& file);
    template <SelectedSide side> void synchronizeFileInt(FilePair& file, SyncOperation syncOp);

    void synchronizeLink(SymlinkPair& link);
    template <SelectedSide sideTrg> void synchronizeLinkInt(SymlinkPair& link, SyncOperation syncOp);

    void synchronizeFolder(FolderPair& folder);
    template <SelectedSide sideTrg> void synchronizeFolderInt(FolderPair& folder, SyncOperation syncOp);

    void reportStatus(const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportStatus(replaceCpy(rawText, L"%x", fmtPath(displayPath))); }
    void reportInfo  (const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportInfo  (replaceCpy(rawText, L"%x", fmtPath(displayPath))); }
    void reportInfo  (const std::wstring& rawText,
                      const std::wstring& displayPath1,
                      const std::wstring& displayPath2) const
    {
        procCallback_.reportInfo(replaceCpy(replaceCpy(rawText, L"%x", L"\n" + fmtPath(displayPath1)), L"%y", L"\n" + fmtPath(displayPath2)));
    }

    AFS::FileAttribAfterCopy copyFileWithCallback(const AbstractPath& sourcePath,
                                                  const AbstractPath& targetPath,
                                                  const std::function<void()>& onDeleteTargetFile,
                                                  const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const; //throw FileError

    template <SelectedSide side>
    DeletionHandling& getDelHandling();

    ProcessCallback& procCallback_;
#ifdef ZEN_WIN
    shadow::ShadowCopy* shadowCopyHandler_; //optional!
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
   -> prepare a 2-step move operation: 1. move source to base and update "move target" accordingly 2. delay move until 2nd pass

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
    [&](const typename List::value_type& obj) { return equalFilePath(obj.getPairItemName(), shortname); });
}


template <SelectedSide side>
void SynchronizeFolderPair::prepare2StepMove(FilePair& sourceObj,
                                             FilePair& targetObj) //throw FileError
{
    Zstring sourceRelPathTmp = sourceObj.getItemName<side>() + AFS::TEMP_FILE_ENDING;
    //this could still lead to a name-clash in obscure cases, if some file exists on the other side with
    //the very same (.ffs_tmp) name and is copied before the second step of the move is executed
    //good news: even in this pathologic case, this may only prevent the copy of the other file, but not the move

    for (int i = 0;; ++i)
        try
        {
            AbstractPath sourcePathTmp = AFS::appendRelPath(sourceObj.base().getAbstractPath<side>(), sourceRelPathTmp);

            reportInfo(txtMovingFile,
                       AFS::getDisplayPath(sourceObj.getAbstractPath<side>()),
                       AFS::getDisplayPath(sourcePathTmp));

            AFS::renameItem(sourceObj.getAbstractPath<side>(), sourcePathTmp); //throw FileError, ErrorTargetExisting, (ErrorDifferentVolume)
            break;
        }
        catch (const ErrorTargetExisting&) //repeat until unique name found: no file system race condition!
        {
            if (i == 10) throw; //avoid endless recursion in pathological cases
            sourceRelPathTmp = sourceObj.getItemName<side>() + Zchar('_') + numberTo<Zstring>(i) + AFS::TEMP_FILE_ENDING;
        }

    warn_static("was wenn diff volume: symlink aliasing!") //throw FileError, ErrorDifferentVolume, ErrorTargetExisting

    //update file hierarchy
    const FileDescriptor descrSource(sourceObj.getLastWriteTime <side>(),
                                     sourceObj.getFileSize      <side>(),
                                     sourceObj.getFileId        <side>(),
                                     sourceObj.isFollowedSymlink<side>());

    FilePair& tempFile = sourceObj.base().addSubFile<side>(afterLast(sourceRelPathTmp, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL), descrSource);
    static_assert(IsSameType<FixedList<FilePair>, HierarchyObject::FileList>::value,
                  "ATTENTION: we're adding to the file list WHILE looping over it! This is only working because FixedList iterators are not invalidated by insertion!");
    sourceObj.removeObject<side>(); //remove only *after* evaluating "sourceObj, side"!

    //prepare move in second pass
    tempFile.setSyncDir(side == LEFT_SIDE ? SyncDirection::LEFT : SyncDirection::RIGHT);

    targetObj.setMoveRef(tempFile .getId());
    tempFile .setMoveRef(targetObj.getId());

    //NO statistics update!
    procCallback_.requestUiRefresh(); //may throw
}


bool SynchronizeFolderPair::createParentFolder(FileSystemObject& fsObj) //throw FileError, "false" on name clash
{
    if (auto parentFolder = dynamic_cast<FolderPair*>(&fsObj.parent()))
    {
        if (!createParentFolder(*parentFolder))
            return false;

        //detect (and try to resolve) file type conflicts: 1. symlinks 2. files
        const Zstring& shortname = parentFolder->getPairItemName();
        if (haveNameClash(shortname, parentFolder->parent().refSubLinks()) ||
            haveNameClash(shortname, parentFolder->parent().refSubFiles()))
            return false;

        //in this context "parentFolder" cannot be scheduled for deletion since it contains a "move target"!
        //note: if parentFolder were deleted, we'd end up destroying "fsObj"!
        assert(parentFolder->getSyncOperation() != SO_DELETE_LEFT &&
               parentFolder->getSyncOperation() != SO_DELETE_RIGHT);

        synchronizeFolder(*parentFolder); //throw FileError
    }
    return true;
}


template <SelectedSide side>
void SynchronizeFolderPair::manageFileMove(FilePair& sourceFile,
                                           FilePair& targetFile) //throw FileError
{
    assert((sourceFile.getSyncOperation() == SO_MOVE_LEFT_SOURCE  && targetFile.getSyncOperation() == SO_MOVE_LEFT_TARGET  && side == LEFT_SIDE) ||
           (sourceFile.getSyncOperation() == SO_MOVE_RIGHT_SOURCE && targetFile.getSyncOperation() == SO_MOVE_RIGHT_TARGET && side == RIGHT_SIDE));

    const bool sourceWillBeDeleted = [&]() -> bool
    {
        if (auto parentFolder = dynamic_cast<const FolderPair*>(&sourceFile.parent()))
        {
            switch (parentFolder->getSyncOperation()) //evaluate comparison result and sync direction
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

    auto haveNameClash = [](const FilePair& file)
    {
        return ::haveNameClash(file.getPairItemName(), file.parent().refSubLinks()) ||
               ::haveNameClash(file.getPairItemName(), file.parent().refSubFolders());
    };

    if (sourceWillBeDeleted || haveNameClash(sourceFile))
    {
        //prepare for move now: - revert to 2-step move on name clashes
        if (haveNameClash(targetFile) ||
            !createParentFolder(targetFile)) //throw FileError
            return prepare2StepMove<side>(sourceFile, targetFile); //throw FileError

        //finally start move! this should work now:
        synchronizeFile(targetFile); //throw FileError
        //SynchronizeFolderPair::synchronizeFileInt() is *not* expecting SO_MOVE_LEFT_SOURCE/SO_MOVE_RIGHT_SOURCE => start move from targetFile, not sourceFile!
    }
    //else: sourceFile will not be deleted, and is not standing in the way => delay to second pass
    //note: this case may include new "move sources" from two-step sub-routine!!!
}


//search for file move-operations
void SynchronizeFolderPair::runZeroPass(HierarchyObject& hierObj)
{
    for (FilePair& file : hierObj.refSubFiles())
    {
        const SyncOperation syncOp = file.getSyncOperation();
        switch (syncOp) //evaluate comparison result and sync direction
        {
            case SO_MOVE_LEFT_SOURCE:
            case SO_MOVE_RIGHT_SOURCE:
                if (FilePair* targetObj = dynamic_cast<FilePair*>(FileSystemObject::retrieve(file.getMoveRef())))
                {
                    FilePair* sourceObj = &file;
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

    for (FolderPair& folder : hierObj.refSubFolders())
        runZeroPass(folder); //recurse
}

//---------------------------------------------------------------------------------------------------------------

//1st, 2nd pass requirements:
// - avoid disk space shortage: 1. delete files, 2. overwrite big with small files first
// - support change in type: overwrite file by directory, symlink by file, ect.

inline
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const FilePair& file)
{
    switch (file.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE;

        case SO_OVERWRITE_LEFT:
            return file.getFileSize<LEFT_SIDE>() > file.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

        case SO_OVERWRITE_RIGHT:
            return file.getFileSize<LEFT_SIDE>() < file.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

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
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const SymlinkPair& link)
{
    switch (link.getSyncOperation()) //evaluate comparison result and sync direction
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
SynchronizeFolderPair::PassId SynchronizeFolderPair::getPass(const FolderPair& folder)
{
    switch (folder.getSyncOperation()) //evaluate comparison result and sync direction
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
    for (FilePair& file : hierObj.refSubFiles())
        if (pass == this->getPass(file)) //"this->" required by two-pass lookup as enforced by GCC 4.7
            tryReportingError([&] { synchronizeFile(file); }, procCallback_); //throw X?

    //synchronize symbolic links:
    for (SymlinkPair& symlink : hierObj.refSubLinks())
        if (pass == this->getPass(symlink))
            tryReportingError([&] { synchronizeLink(symlink); }, procCallback_); //throw X?

    //synchronize folders:
    for (FolderPair& folder : hierObj.refSubFolders())
    {
        if (pass == this->getPass(folder))
            tryReportingError([&] { synchronizeFolder(folder); }, procCallback_); //throw X?

        this->runPass<pass>(folder); //recurse
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
void SynchronizeFolderPair::synchronizeFile(FilePair& file)
{
    const SyncOperation syncOp = file.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFileInt<LEFT_SIDE>(file, syncOp);
        else
            synchronizeFileInt<RIGHT_SIDE>(file, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFileInt(FilePair& file, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&file.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            //can't use "getAbstractPath<sideTrg>()" as file name is not available!
            const AbstractPath targetPath = AFS::appendRelPath(file.base().getAbstractPath<sideTrg>(), file.getRelativePath<sideSrc>());
            reportInfo(txtCreatingFile, AFS::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, file.getFileSize<sideSrc>(), procCallback_);
            try
            {
                auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                const AFS::FileAttribAfterCopy newAttr = copyFileWithCallback(file.getAbstractPath<sideSrc>(),
                                                                              targetPath,
                                                                              nullptr, //no target to delete
                                                                              onNotifyCopyStatus); //throw FileError
                statReporter.reportDelta(1, 0);

                //update FilePair
                file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), newAttr.fileSize,
                                          newAttr.modificationTime, //target time set from source
                                          newAttr.modificationTime,
                                          newAttr.targetFileId,
                                          newAttr.sourceFileId,
                                          false, file.isFollowedSymlink<sideSrc>());
            }
            catch (FileError&)
            {
                warn_static("still an error if base dir is missing!")
                //  const Zstring basedir = beforeLast(file.getBaseDirPf<side>(), FILE_NAME_SEPARATOR); //what about C:\ ???
                //if (!dirExists(basedir) ||


                if (!AFS::somethingExists(file.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should error out!
                {
                    //source deleted meanwhile...nothing was done (logical point of view!)
                    file.removeObject<sideSrc>(); //remove only *after* evaluating "file, sideSrc"!
                }
                else
                    throw;
            }
            statReporter.reportFinished();
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingFile(), AFS::getDisplayPath(file.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeFileWithCallback(file.getAbstractPath<sideTrg>(), file.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                warn_static("what if item not found? still an error if base dir is missing; externally deleted otherwise!")

                file.removeObject<sideTrg>(); //update FilePair

                statReporter.reportFinished();
            }
            break;

        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_TARGET:
            if (FilePair* moveSource = dynamic_cast<FilePair*>(FileSystemObject::retrieve(file.getMoveRef())))
            {
                FilePair* moveTarget = &file;

                assert((moveSource->getSyncOperation() == SO_MOVE_LEFT_SOURCE  && moveTarget->getSyncOperation() == SO_MOVE_LEFT_TARGET  && sideTrg == LEFT_SIDE) ||
                       (moveSource->getSyncOperation() == SO_MOVE_RIGHT_SOURCE && moveTarget->getSyncOperation() == SO_MOVE_RIGHT_TARGET && sideTrg == RIGHT_SIDE));

                const AbstractPath oldPath = moveSource->getAbstractPath<sideTrg>();
                const AbstractPath newPath = AFS::appendRelPath(moveTarget->base().getAbstractPath<sideTrg>(), moveTarget->getRelativePath<sideSrc>());

                reportInfo(txtMovingFile, AFS::getDisplayPath(oldPath), AFS::getDisplayPath(newPath));
                warn_static("was wenn diff volume: symlink aliasing!")
                AFS::renameItem(oldPath, newPath); //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)

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
            //respect differences in case of source object:
            const AbstractPath targetPathLogical = AFS::appendRelPath(file.base().getAbstractPath<sideTrg>(), file.getRelativePath<sideSrc>());

            const AbstractPath targetPathResolved = file.isFollowedSymlink<sideTrg>() ? //follow link when updating file rather than delete it and replace with regular file!!!
                                                    AFS::getResolvedSymlinkPath(file.getAbstractPath<sideTrg>()) : //throw FileError
                                                    targetPathLogical; //respect differences in case of source object

            reportInfo(txtOverwritingFile, AFS::getDisplayPath(targetPathResolved));

            if (file.isFollowedSymlink<sideTrg>()) //since we follow the link, we need to sync case sensitivity of the link manually!
                if (file.getItemName<sideTrg>() != file.getItemName<sideSrc>()) //have difference in case?
                    AFS::renameItem(file.getAbstractPath<sideTrg>(), targetPathLogical); //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)

            StatisticsReporter statReporter(1, file.getFileSize<sideSrc>(), procCallback_);

            auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            auto onDeleteTargetFile = [&] //delete target at appropriate time
            {
                reportStatus(this->getDelHandling<sideTrg>().getTxtRemovingFile(), AFS::getDisplayPath(targetPathResolved));

                this->getDelHandling<sideTrg>().removeFileWithCallback(targetPathResolved, file.getPairRelativePath(), [] {}, onNotifyCopyStatus); //throw FileError;
                //no (logical) item count update desired - but total byte count may change, e.g. move(copy) deleted file to versioning dir

                //file.removeObject<sideTrg>(); -> doesn't make sense for isFollowedSymlink(); "file, sideTrg" evaluated below!

                //if fail-safe file copy is active, then the next operation will be a simple "rename"
                //=> don't risk reportStatus() throwing GuiAbortProcess() leaving the target deleted rather than updated!
                if (!transactionalFileCopy_)
                    reportStatus(txtOverwritingFile, AFS::getDisplayPath(targetPathResolved)); //restore status text copy file
            };

            const AFS::FileAttribAfterCopy newAttr = copyFileWithCallback(file.getAbstractPath<sideSrc>(),
                                                                          targetPathResolved,
                                                                          onDeleteTargetFile,
                                                                          onNotifyCopyStatus); //throw FileError
            statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

            //update FilePair
            file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), newAttr.fileSize,
                                      newAttr.modificationTime, //target time set from source
                                      newAttr.modificationTime,
                                      newAttr.targetFileId,
                                      newAttr.sourceFileId,
                                      file.isFollowedSymlink<sideTrg>(),
                                      file.isFollowedSymlink<sideSrc>());

            statReporter.reportFinished();
        }
        break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            //harmonize with file_hierarchy.cpp::getSyncOpDescription!!

            reportInfo(txtWritingAttributes, AFS::getDisplayPath(file.getAbstractPath<sideTrg>()));

            if (file.getItemName<sideTrg>() != file.getItemName<sideSrc>()) //have difference in case?
                AFS::renameItem(file.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                AFS::appendRelPath(file.base().getAbstractPath<sideTrg>(), file.getRelativePath<sideSrc>()));

            if (file.getLastWriteTime<sideTrg>() != file.getLastWriteTime<sideSrc>())
                //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                AFS::setModTime(file.getAbstractPath<sideTrg>(), file.getLastWriteTime<sideSrc>()); //throw FileError

            //-> both sides *should* be completely equal now...
            assert(file.getFileSize<sideTrg>() == file.getFileSize<sideSrc>());
            file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), file.getFileSize<sideSrc>(),
                                      file.getLastWriteTime<sideSrc>(), //target time set from source
                                      file.getLastWriteTime<sideSrc>(),
                                      file.getFileId       <sideTrg>(),
                                      file.getFileId       <sideSrc>(),
                                      file.isFollowedSymlink<sideTrg>(),
                                      file.isFollowedSymlink<sideSrc>());

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
void SynchronizeFolderPair::synchronizeLink(SymlinkPair& link)
{
    const SyncOperation syncOp = link.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeLinkInt<LEFT_SIDE>(link, syncOp);
        else
            synchronizeLinkInt<RIGHT_SIDE>(link, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeLinkInt(SymlinkPair& symlink, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&symlink.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            const AbstractPath targetPath = AFS::appendRelPath(symlink.base().getAbstractPath<sideTrg>(), symlink.getRelativePath<sideSrc>());
            reportInfo(txtCreatingLink, AFS::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, 0, procCallback_);
            try
            {
                AFS::copySymlink(symlink.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError
                //update SymlinkPair
                symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                             symlink.getLastWriteTime<sideSrc>(), //target time set from source
                                             symlink.getLastWriteTime<sideSrc>());

                statReporter.reportDelta(1, 0);
            }
            catch (FileError&)
            {
                warn_static("still an error if base dir is missing!")

                if (AFS::somethingExists(symlink.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should not be quiet about it!
                    throw;
                //source deleted meanwhile...nothing was done (logical point of view!)
                symlink.removeObject<sideSrc>();
            }
            statReporter.reportFinished();
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingSymLink(), AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeLinkWithCallback(symlink.getAbstractPath<sideTrg>(), symlink.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                symlink.removeObject<sideTrg>(); //update SymlinkPair

                statReporter.reportFinished();
            }
            break;

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
            reportInfo(txtOverwritingLink, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                //reportStatus(getDelHandling<sideTrg>().getTxtRemovingSymLink(), AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
                getDelHandling<sideTrg>().removeLinkWithCallback(symlink.getAbstractPath<sideTrg>(), symlink.getPairRelativePath(), [] {}, onNotifyCopyStatus); //throw FileError

                //symlink.removeObject<sideTrg>(); -> "symlink, sideTrg" evaluated below!

                //=> don't risk reportStatus() throwing GuiAbortProcess() leaving the target deleted rather than updated:
                //reportStatus(txtOverwritingLink, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>())); //restore status text

                AFS::copySymlink(symlink.getAbstractPath<sideSrc>(),
                                 AFS::appendRelPath(symlink.base().getAbstractPath<sideTrg>(), symlink.getRelativePath<sideSrc>()), //respect differences in case of source object
                                 copyFilePermissions_); //throw FileError

                statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

                //update SymlinkPair
                symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                             symlink.getLastWriteTime<sideSrc>(), //target time set from source
                                             symlink.getLastWriteTime<sideSrc>());

                statReporter.reportFinished();
            }
            break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));

            if (symlink.getItemName<sideTrg>() != symlink.getItemName<sideSrc>()) //have difference in case?
                AFS::renameItem(symlink.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                AFS::appendRelPath(symlink.base().getAbstractPath<sideTrg>(), symlink.getRelativePath<sideSrc>()));

            if (symlink.getLastWriteTime<sideTrg>() != symlink.getLastWriteTime<sideSrc>())
                //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                AFS::setModTimeSymlink(symlink.getAbstractPath<sideTrg>(), symlink.getLastWriteTime<sideSrc>()); //throw FileError

            //-> both sides *should* be completely equal now...
            symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                         symlink.getLastWriteTime<sideSrc>(), //target time set from source
                                         symlink.getLastWriteTime<sideSrc>());

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
void SynchronizeFolderPair::synchronizeFolder(FolderPair& folder)
{
    const SyncOperation syncOp = folder.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFolderInt<LEFT_SIDE>(folder, syncOp);
        else
            synchronizeFolderInt<RIGHT_SIDE>(folder, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFolderInt(FolderPair& folder, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&folder.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            warn_static("save this file access?")
            if (AFS::somethingExists(folder.getAbstractPath<sideSrc>())) //do not check on type (symlink, file, folder) -> if there is a type change, FFS should error out!
            {
                const AbstractPath targetPath = AFS::appendRelPath(folder.base().getAbstractPath<sideTrg>(), folder.getRelativePath<sideSrc>());
                reportInfo(txtCreatingFolder, AFS::getDisplayPath(targetPath));

                try
                {
                    AFS::copyNewFolder(folder.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError
                }
                catch (const FileError&) { if (!AFS::folderExists(targetPath)) throw; }

                //update FolderPair
                folder.setSyncedTo(folder.getItemName<sideSrc>());

                procCallback_.updateProcessedData(1, 0);
            }
            else //source deleted meanwhile...nothing was done (logical point of view!) -> uh....what about a temporary network drop???
            {
                warn_static("still an error if base dir is missing!")

                const SyncStatistics subStats(folder);
                procCallback_.updateTotalData(-getCUD(subStats) - 1, -subStats.getDataToProcess());

                //remove only *after* evaluating folder!!
                folder.refSubFiles  ().clear(); //
                folder.refSubLinks  ().clear(); //update FolderPair
                folder.refSubFolders().clear(); //
                folder.removeObject<sideSrc>(); //
            }
            break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingDir(), AFS::getDisplayPath(folder.getAbstractPath<sideTrg>()));
            {
                const SyncStatistics subStats(folder); //counts sub-objects only!

                StatisticsReporter statReporter(1 + getCUD(subStats), subStats.getDataToProcess(), procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto onNotifyCopyStatus   = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeDirWithCallback(folder.getAbstractPath<sideTrg>(), folder.getPairRelativePath(), onNotifyItemDeletion, onNotifyCopyStatus); //throw FileError

                folder.refSubFiles  ().clear(); //
                folder.refSubLinks  ().clear(); //update FolderPair
                folder.refSubFolders().clear(); //
                folder.removeObject<sideTrg>(); //

                statReporter.reportFinished();
            }
            break;

        case SO_OVERWRITE_LEFT:  //possible: e.g. manually-resolved dir-traversal conflict
        case SO_OVERWRITE_RIGHT: //
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, AFS::getDisplayPath(folder.getAbstractPath<sideTrg>()));

            if (folder.getItemName<sideTrg>() != folder.getItemName<sideSrc>()) //have difference in case?
                AFS::renameItem(folder.getAbstractPath<sideTrg>(), //throw FileError, (ErrorTargetExisting, ErrorDifferentVolume)
                                AFS::appendRelPath(folder.base().getAbstractPath<sideTrg>(), folder.getRelativePath<sideSrc>()));
            //copyFileTimes -> useless: modification time changes with each child-object creation/deletion

            //-> both sides *should* be completely equal now...
            folder.setSyncedTo(folder.getItemName<sideSrc>());

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
void verifyFiles(const AbstractPath& sourcePath, const AbstractPath& targetPath, const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)  //throw FileError
{
    try
    {
        //do like "copy /v": 1. flush target file buffers, 2. read again as usual (using OS buffers)
        // => it seems OS buffered are not invalidated by this: snake oil???
        if (Opt<Zstring> nativeTargetPath = AFS::getNativeItemPath(targetPath))
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
            throw FileError(replaceCpy(replaceCpy(_("%x and %y have different content."),
                                                  L"%x", L"\n" + fmtPath(AFS::getDisplayPath(sourcePath))),
                                       L"%y", L"\n" + fmtPath(AFS::getDisplayPath(targetPath))));
    }
    catch (const FileError& e) //add some context to error message
    {
        throw FileError(_("Data verification error:"), e.toString());
    }
}


AFS::FileAttribAfterCopy SynchronizeFolderPair::copyFileWithCallback(const AbstractPath& sourcePath,  //throw FileError
                                                                     const AbstractPath& targetPath,
                                                                     const std::function<void()>& onDeleteTargetFile,
                                                                     const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) const //returns current attributes of source file
{
    auto copyOperation = [this, &targetPath, &onDeleteTargetFile, &onNotifyCopyStatus](const AbstractPath& sourcePathTmp)
    {
        AFS::FileAttribAfterCopy newAttr = AFS::copyFileTransactional(sourcePathTmp, targetPath, //throw FileError, ErrorFileLocked
                                                                      copyFilePermissions_,
                                                                      transactionalFileCopy_,
                                                                      onDeleteTargetFile,
                                                                      onNotifyCopyStatus);

        //#################### Verification #############################
        if (verifyCopiedFiles_)
        {
            ZEN_ON_SCOPE_FAIL( AFS::removeFile(targetPath); ); //delete target if verification fails

            procCallback_.reportInfo(replaceCpy(txtVerifying, L"%x", fmtPath(AFS::getDisplayPath(targetPath))));
            verifyFiles(sourcePathTmp, targetPath, [&](std::int64_t bytesDelta) { procCallback_.requestUiRefresh(); }); //throw FileError
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
        if (shadowCopyHandler_) //if file is locked (try to) use Windows Volume Shadow Copy Service:
            if (Opt<Zstring> nativeSourcePath = AFS::getNativeItemPath(sourcePath))
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
                return copyOperation(createItemPathNative(nativeShadowPath));
            }
        throw;
    }
#else
    return copyOperation(sourcePath);
#endif
}

//###########################################################################################

template <SelectedSide side>
bool baseFolderDrop(BaseFolderPair& baseFolder, ProcessCallback& callback)
{
    const AbstractPath folderPath = baseFolder.getAbstractPath<side>();

    if (baseFolder.isExisting<side>())
        if (Opt<std::wstring> errMsg = tryReportingError([&]
    {
        if (!folderExistsNonBlocking(folderPath, false, callback))
                throw FileError(replaceCpy(_("Cannot find folder %x."), L"%x", fmtPath(AFS::getDisplayPath(folderPath))));
            //should really be logged as a "fatal error" if ignored by the user...
        }, callback)) //throw X?
    return true;

    return false;
}


template <SelectedSide side> //create base directories first (if not yet existing) -> no symlink or attribute copying!
bool createBaseFolder(BaseFolderPair& baseFolder, ProcessCallback& callback) //nothrow; return false if fatal error occurred
{
    const AbstractPath baseFolderPath = baseFolder.getAbstractPath<side>();

    if (AFS::isNullPath(baseFolderPath))
        return true;

    if (!baseFolder.isExisting<side>()) //create target directory: user presumably ignored error "dir existing" in order to have it created automatically
    {
        bool temporaryNetworkDrop = false;
        zen::Opt<std::wstring> errMsg = tryReportingError([&]
        {
            try
            {
                //a nice race-free check and set operation:
                AFS::createFolderSimple(baseFolderPath); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
                baseFolder.setExisting<side>(true); //update our model!
            }
            catch (ErrorTargetPathMissing&)
            {
                AFS::createFolderRecursively(baseFolderPath); //throw FileError
                baseFolder.setExisting<side>(true); //update our model!
            }
            catch (ErrorTargetExisting&)
            {
                //TEMPORARY network drop! base directory not found during comparison, but reappears during synchronization
                //=> sync-directions are based on false assumptions! Abort.
                callback.reportFatalError(replaceCpy(_("Target folder %x already existing."), L"%x", fmtPath(AFS::getDisplayPath(baseFolderPath))));
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
    size_t reads  = 0;
    size_t writes = 0;
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

    //aggregate basic information
    std::vector<SyncStatistics> folderPairStats;
    {
        int     objectsTotal = 0;
        int64_t dataTotal    = 0;
        for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
        {
            SyncStatistics fpStats(*j);
            objectsTotal += getCUD(fpStats);
            dataTotal    += fpStats.getDataToProcess();
            folderPairStats.push_back(fpStats);
        }

        //inform about the total amount of data that will be processed from now on
        //keep at beginning so that all gui elements are initialized properly
        callback.initNewPhase(objectsTotal, //throw X
                              dataTotal,
                              ProcessCallback::PHASE_SYNCHRONIZING);
    }

    std::vector<FolderPairJobType> jobType(folderCmp.size(), FolderPairJobType::PROCESS); //folder pairs may be skipped after fatal errors were found

    //-------------------execute basic checks all at once before starting sync--------------------------------------

    std::vector<SyncStatistics::ConflictInfo> unresolvedConflicts;

    //aggregate information
    std::map<AbstractPath, ReadWriteCount, AFS::LessAbstractPath> dirReadWriteCount; //count read/write accesses
    for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
    {
        //create all entries first! otherwise counting accesses would be too complex during later inserts!

        if (!AFS::isNullPath(j->getAbstractPath<LEFT_SIDE>())) //empty folder is always dependent => exclude!
            dirReadWriteCount[j->getAbstractPath<LEFT_SIDE>()];
        if (!AFS::isNullPath(j->getAbstractPath<RIGHT_SIDE>()))
            dirReadWriteCount[j->getAbstractPath<RIGHT_SIDE>()];
    }

    auto incReadCount = [&](const AbstractPath& baseFolderPath)
    {
        if (!AFS::isNullPath(baseFolderPath))
            for (auto& item : dirReadWriteCount)
                if (AFS::havePathDependency(baseFolderPath, item.first))
                    ++item.second.reads;
    };
    auto incWriteCount = [&](const AbstractPath& baseFolderPath)
    {
        if (!AFS::isNullPath(baseFolderPath))
            for (auto& item : dirReadWriteCount)
                if (AFS::havePathDependency(baseFolderPath, item.first))
                    ++item.second.writes;
    };

    std::vector<std::pair<AbstractPath, AbstractPath>> significantDiffPairs;

    std::vector<std::pair<AbstractPath, std::pair<std::int64_t, std::int64_t>>> diskSpaceMissing; //base folder / space required / space available

    //status of base directories which are set to DELETE_TO_RECYCLER (and contain actual items to be deleted)
    std::map<AbstractPath, bool, AFS::LessAbstractPath> recyclerSupported; //expensive to determine on Win XP => buffer + check recycle bin existence only once per base folder!

    //start checking folder pairs
    for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
    {
        const size_t folderIndex = j - begin(folderCmp);
        const FolderPairSyncCfg& folderPairCfg  = syncConfig     [folderIndex];
        const SyncStatistics&    folderPairStat = folderPairStats[folderIndex];

        //aggregate all conflicts:
        append(unresolvedConflicts, folderPairStat.getConflicts());

        //exclude a few pathological cases (including empty left, right folders)
        if (AFS::equalAbstractPath(j->getAbstractPath<LEFT_SIDE >(),
                                   j->getAbstractPath<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        const bool writeLeft = folderPairStat.createCount<LEFT_SIDE>() +
                               folderPairStat.updateCount<LEFT_SIDE>() +
                               folderPairStat.deleteCount<LEFT_SIDE>() > 0;

        const bool writeRight = folderPairStat.createCount<RIGHT_SIDE>() +
                                folderPairStat.updateCount<RIGHT_SIDE>() +
                                folderPairStat.deleteCount<RIGHT_SIDE>() > 0;

        //skip folder pair if there is nothing to do (except for two-way mode and move-detection, where DB files need to be updated)
        //-> skip creating (not yet existing) base directories in particular if there's no need
        if (!writeLeft && !writeRight)
        {
            jobType[folderIndex] = FolderPairJobType::ALREADY_IN_SYNC;
            continue;
        }

        //aggregate information of folders used by multiple pairs in read/write access
        if (!AFS::havePathDependency(j->getAbstractPath<LEFT_SIDE>(), j->getAbstractPath<RIGHT_SIDE>()))
        {
            if (writeLeft)
                incWriteCount(j->getAbstractPath<LEFT_SIDE>());
            else if (writeRight)
                incReadCount (j->getAbstractPath<LEFT_SIDE>());

            if (writeRight)
                incWriteCount(j->getAbstractPath<RIGHT_SIDE>());
            else if (writeLeft)
                incReadCount (j->getAbstractPath<RIGHT_SIDE>());
        }
        else //if folder pair contains two dependent folders, a warning was already issued after comparison; in this context treat as one write access at most
        {
            if (writeLeft || writeRight)
                incWriteCount(j->getAbstractPath<LEFT_SIDE>());
        }

        //check for empty target folder paths: this only makes sense if empty field is source (and no DB files need to be created)
        if ((AFS::isNullPath(j->getAbstractPath<LEFT_SIDE >()) && (writeLeft  || folderPairCfg.saveSyncDB_)) ||
            (AFS::isNullPath(j->getAbstractPath<RIGHT_SIDE>()) && (writeRight || folderPairCfg.saveSyncDB_)))
        {
            callback.reportFatalError(_("Target folder input field must not be empty."));
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //check for network drops after comparison
        // - convenience: exit sync right here instead of showing tons of errors during file copy
        // - early failure! there's no point in evaluating subsequent warnings
        if (baseFolderDrop<LEFT_SIDE >(*j, callback) ||
            baseFolderDrop<RIGHT_SIDE>(*j, callback))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //allow propagation of deletions only from *null-* or *existing* source folder:
        auto sourceFolderMissing = [&](const AbstractPath& baseFolder, bool wasExisting) //we need to evaluate existence status from time of comparison!
        {
            if (!AFS::isNullPath(baseFolder))
                //PERMANENT network drop: avoid data loss when source directory is not found AND user chose to ignore errors (else we wouldn't arrive here)
                if (folderPairStat.deleteCount() > 0) //check deletions only... (respect filtered items!)
                    //folderPairStat.conflictCount() == 0 && -> there COULD be conflicts for <automatic> if directory existence check fails, but loading sync.ffs_db succeeds
                    //https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3531351&group_id=234430 -> fixed, but still better not consider conflicts!
                    if (!wasExisting) //avoid race-condition: we need to evaluate existence status from time of comparison!
                    {
                        callback.reportFatalError(replaceCpy(_("Source folder %x not found."), L"%x", fmtPath(AFS::getDisplayPath(baseFolder))));
                        return true;
                    }
            return false;
        };
        if (sourceFolderMissing(j->getAbstractPath<LEFT_SIDE >(), j->isExisting<LEFT_SIDE >()) ||
            sourceFolderMissing(j->getAbstractPath<RIGHT_SIDE>(), j->isExisting<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //check if user-defined directory for deletion was specified
        if (folderPairCfg.handleDeletion == zen::DELETE_TO_VERSIONING &&
            folderPairStat.updateCount() + folderPairStat.deleteCount() > 0)
        {
            if (trimCpy(folderPairCfg.versioningFolderPhrase).empty())
            {
                //should never arrive here: already checked in SyncCfgDialog
                callback.reportFatalError(_("Please enter a target folder for versioning."));
                jobType[folderIndex] = FolderPairJobType::SKIP;
                continue;
            }
        }

        //check if more than 50% of total number of files/dirs are to be created/overwritten/deleted
        if (!AFS::isNullPath(j->getAbstractPath<LEFT_SIDE >()) &&
            !AFS::isNullPath(j->getAbstractPath<RIGHT_SIDE>()))
            if (significantDifferenceDetected(folderPairStat))
                significantDiffPairs.emplace_back(j->getAbstractPath<LEFT_SIDE >(),
                                                  j->getAbstractPath<RIGHT_SIDE>());

        //check for sufficient free diskspace
        auto checkSpace = [&](const AbstractPath& baseFolderPath, std::int64_t minSpaceNeeded)
        {
            if (!AFS::isNullPath(baseFolderPath))
                try
                {
                    const std::int64_t freeSpace = AFS::getFreeDiskSpace(baseFolderPath); //throw FileError, returns 0 if not available

                    if (0 < freeSpace && //zero means "request not supported" (e.g. see WebDav)
                        freeSpace < minSpaceNeeded)
                        diskSpaceMissing.emplace_back(baseFolderPath, std::make_pair(minSpaceNeeded, freeSpace));
                }
                catch (FileError&) { assert(false); } //for warning only => no need for tryReportingError()
        };
        const std::pair<std::int64_t, std::int64_t> spaceNeeded = MinimumDiskSpaceNeeded::calculate(*j);
        checkSpace(j->getAbstractPath<LEFT_SIDE >(), spaceNeeded.first);
        checkSpace(j->getAbstractPath<RIGHT_SIDE>(), spaceNeeded.second);

        //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
        auto checkRecycler = [&](const AbstractPath& baseFolderPath)
        {
            assert(!AFS::isNullPath(baseFolderPath));
            if (!AFS::isNullPath(baseFolderPath))
                if (recyclerSupported.find(baseFolderPath) == recyclerSupported.end()) //perf: avoid duplicate checks!
                {
                    callback.reportStatus(replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x",
                                                     fmtPath(AFS::getDisplayPath(baseFolderPath))));
                    bool recSupported = false;
                    tryReportingError([&]
                    {
                        recSupported = AFS::supportsRecycleBin(baseFolderPath, [&]{ callback.requestUiRefresh(); /*may throw*/ }); //throw FileError
                    }, callback); //throw X?

                    recyclerSupported.emplace(baseFolderPath, recSupported);
                }
        };

        if (folderPairCfg.handleDeletion == DELETE_TO_RECYCLER)
        {
            if (folderPairStat.updateCount<LEFT_SIDE>() +
                folderPairStat.deleteCount<LEFT_SIDE>() > 0)
                checkRecycler(j->getAbstractPath<LEFT_SIDE>());

            if (folderPairStat.updateCount<RIGHT_SIDE>() +
                folderPairStat.deleteCount<RIGHT_SIDE>() > 0)
                checkRecycler(j->getAbstractPath<RIGHT_SIDE>());
        }
    }

    //check if unresolved conflicts exist
    if (!unresolvedConflicts.empty())
    {
        std::wstring msg = _("The following items have unresolved conflicts and will not be synchronized:");

        for (const SyncStatistics::ConflictInfo& item : unresolvedConflicts) //show *all* conflicts in warning message
            msg += L"\n\n" + fmtPath(item.first) + L": " + item.second;

        callback.reportWarning(msg, warnings.warningUnresolvedConflicts);
    }

    //check if user accidentally selected wrong directories for sync
    if (!significantDiffPairs.empty())
    {
        std::wstring msg = _("The following folders are significantly different. Make sure you have selected the correct folders for synchronization.");

        for (const auto& item : significantDiffPairs)
            msg += L"\n\n" +
                   AFS::getDisplayPath(item.first) + L" <-> " + L"\n" +
                   AFS::getDisplayPath(item.second);

        callback.reportWarning(msg, warnings.warningSignificantDifference);
    }

    //check for sufficient free diskspace
    if (!diskSpaceMissing.empty())
    {
        std::wstring msg = _("Not enough free disk space available in:");

        for (const auto& item : diskSpaceMissing)
            msg += L"\n\n" + AFS::getDisplayPath(item.first) + L"\n" +
                   _("Required:")  + L" " + filesizeToShortString(item.second.first)  + L"\n" +
                   _("Available:") + L" " + filesizeToShortString(item.second.second);

        callback.reportWarning(msg, warnings.warningNotEnoughDiskSpace);
    }

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    {
        std::wstring dirListMissingRecycler;
        for (const auto& item : recyclerSupported)
            if (!item.second)
                dirListMissingRecycler += L"\n" + AFS::getDisplayPath(item.first);

        if (!dirListMissingRecycler.empty())
            callback.reportWarning(_("The recycle bin is not available for the following folders. Files will be deleted permanently instead:") + L"\n" +
                                   dirListMissingRecycler, warnings.warningRecyclerMissing);
    }

    //check if folders are used by multiple pairs in read/write access
    {
        std::vector<AbstractPath> conflictFolders;
        for (const auto& item : dirReadWriteCount)
            if (item.second.reads + item.second.writes >= 2 && item.second.writes >= 1) //race condition := multiple accesses of which at least one is a write
                conflictFolders.push_back(item.first);

        if (!conflictFolders.empty())
        {
            std::wstring msg = _("Multiple folder pairs write to a common subfolder. Please review your configuration.") + L"\n";
            for (const AbstractPath& folderPath : conflictFolders)
                msg += L"\n" + AFS::getDisplayPath(folderPath);

            callback.reportWarning(msg, warnings.warningFolderPairRaceCondition);
        }
    }

    //-------------------end of basic checks------------------------------------------

#ifdef ZEN_WIN
    //shadow copy buffer: per sync-instance, not folder pair
    std::unique_ptr<shadow::ShadowCopy> shadowCopyHandler;
    if (copyLockedFiles)
        shadowCopyHandler = std::make_unique<shadow::ShadowCopy>();
#endif

    try
    {
        //loop through all directory pairs
        for (auto j = begin(folderCmp); j != end(folderCmp); ++j)
        {
            const size_t folderIndex = j - begin(folderCmp);
            const FolderPairSyncCfg& folderPairCfg  = syncConfig     [folderIndex];
            const SyncStatistics&    folderPairStat = folderPairStats[folderIndex];

            if (jobType[folderIndex] == FolderPairJobType::SKIP) //folder pairs may be skipped after fatal errors were found
                continue;

            //------------------------------------------------------------------------------------------
            callback.reportInfo(_("Synchronizing folder pair:") + L" [" + getVariantName(folderPairCfg.syncVariant_) + L"]\n" +
                                L"    " + AFS::getDisplayPath(j->getAbstractPath<LEFT_SIDE >()) + L"\n" +
                                L"    " + AFS::getDisplayPath(j->getAbstractPath<RIGHT_SIDE>()));
            //------------------------------------------------------------------------------------------

            //checking a second time: (a long time may have passed since the intro checks!)
            if (baseFolderDrop<LEFT_SIDE >(*j, callback) ||
                baseFolderDrop<RIGHT_SIDE>(*j, callback))
                continue;

            //create base folders if not yet existing
            if (folderPairStat.createCount() > 0 || folderPairCfg.saveSyncDB_) //else: temporary network drop leading to deletions already caught by "sourceFolderMissing" check!
                if (!createBaseFolder<LEFT_SIDE >(*j, callback) || //+ detect temporary network drop!!
                    !createBaseFolder<RIGHT_SIDE>(*j, callback))   //
                    continue;

            //------------------------------------------------------------------------------------------
            //execute synchronization recursively

            //update synchronization database in case of errors:
            ZEN_ON_SCOPE_FAIL(try
            {
                if (folderPairCfg.saveSyncDB_)
                    zen::saveLastSynchronousState(*j, nullptr);
            } //throw FileError
            catch (FileError&) {}
                             );

            if (jobType[folderIndex] == FolderPairJobType::PROCESS)
            {
                //guarantee removal of invalid entries (where element is empty on both sides)
                ZEN_ON_SCOPE_EXIT(BaseFolderPair::removeEmpty(*j));

                bool copyPermissionsFp = false;
                tryReportingError([&]
                {
                    copyPermissionsFp = copyFilePermissions && //copy permissions only if asked for and supported by *both* sides!
                    !AFS::isNullPath(j->getAbstractPath<LEFT_SIDE >()) && //scenario: directory selected on one side only
                    !AFS::isNullPath(j->getAbstractPath<RIGHT_SIDE>()) && //
                    AFS::supportPermissionCopy(j->getAbstractPath<LEFT_SIDE>(), j->getAbstractPath<RIGHT_SIDE>()); //throw FileError
                }, callback); //throw X?


                auto getEffectiveDeletionPolicy = [&](const AbstractPath& baseFolderPath) -> DeletionPolicy
                {
                    if (folderPairCfg.handleDeletion == DELETE_TO_RECYCLER)
                    {
                        auto it = recyclerSupported.find(baseFolderPath);
                        if (it != recyclerSupported.end()) //buffer filled during intro checks (but only if deletions are expected)
                            if (!it->second)
                                return DELETE_PERMANENTLY; //Windows' ::SHFileOperation() will do this anyway, but we have a better and faster deletion routine (e.g. on networks)
                    }
                    return folderPairCfg.handleDeletion;
                };


                DeletionHandling delHandlerL(j->getAbstractPath<LEFT_SIDE>(),
                                             getEffectiveDeletionPolicy(j->getAbstractPath<LEFT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);

                DeletionHandling delHandlerR(j->getAbstractPath<RIGHT_SIDE>(),
                                             getEffectiveDeletionPolicy(j->getAbstractPath<RIGHT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);


                SynchronizeFolderPair syncFP(callback, verifyCopiedFiles, copyPermissionsFp, transactionalFileCopy,
#ifdef ZEN_WIN
                                             shadowCopyHandler.get(),
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
            }
        }
    }
    catch (const std::exception& e)
    {
        callback.reportFatalError(utfCvrtTo<std::wstring>(e.what()));
    }
}
