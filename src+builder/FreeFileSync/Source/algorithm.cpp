// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "algorithm.h"
#include <set>
#include <unordered_map>
#include <zen/perf.h>
#include "lib/norm_filter.h"
#include "lib/db_file.h"
#include "lib/cmp_filetime.h"
#include "lib/status_handler_impl.h"
#include "fs/concrete.h"

using namespace zen;
//using namespace std::rel_ops;


void zen::swapGrids(const MainConfiguration& config, FolderComparison& folderCmp)
{
    std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { baseFolder.flip(); });
    redetermineSyncDirection(config, folderCmp,
                             nullptr,  //onReportWarning
                             nullptr); //onUpdateStatus -> status update while loading db file
}

//----------------------------------------------------------------------------------------------

namespace
{
class Redetermine
{
public:
    static void execute(const DirectionSet& dirCfgIn, HierarchyObject& hierObj) { Redetermine(dirCfgIn).recurse(hierObj); }

private:
    Redetermine(const DirectionSet& dirCfgIn) : dirCfg(dirCfgIn) {}

    void recurse(HierarchyObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processFolder(folder);
    }

    void processFile(FilePair& file) const
    {
        const CompareFilesResult cat = file.getCategory();

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        switch (cat)
        {
            case FILE_LEFT_SIDE_ONLY:
                file.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case FILE_RIGHT_SIDE_ONLY:
                file.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case FILE_RIGHT_NEWER:
                file.setSyncDir(dirCfg.rightNewer);
                break;
            case FILE_LEFT_NEWER:
                file.setSyncDir(dirCfg.leftNewer);
                break;
            case FILE_DIFFERENT_CONTENT:
                file.setSyncDir(dirCfg.different);
                break;
            case FILE_CONFLICT:
            case FILE_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    file.setSyncDirConflict(file.getCatExtraDescription()); //take over category conflict
                else
                    file.setSyncDir(dirCfg.conflict);
                break;
            case FILE_EQUAL:
                file.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        switch (symlink.getLinkCategory())
        {
            case SYMLINK_LEFT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case SYMLINK_RIGHT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case SYMLINK_LEFT_NEWER:
                symlink.setSyncDir(dirCfg.leftNewer);
                break;
            case SYMLINK_RIGHT_NEWER:
                symlink.setSyncDir(dirCfg.rightNewer);
                break;
            case SYMLINK_CONFLICT:
            case SYMLINK_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    symlink.setSyncDirConflict(symlink.getCatExtraDescription()); //take over category conflict
                else
                    symlink.setSyncDir(dirCfg.conflict);
                break;
            case SYMLINK_DIFFERENT_CONTENT:
                symlink.setSyncDir(dirCfg.different);
                break;
            case SYMLINK_EQUAL:
                symlink.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processFolder(FolderPair& folder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, folder); //don't recurse below!
        //#######################################################################################

        switch (cat)
        {
            case DIR_LEFT_SIDE_ONLY:
                folder.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case DIR_RIGHT_SIDE_ONLY:
                folder.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case DIR_EQUAL:
                folder.setSyncDir(SyncDirection::NONE);
                break;
            case DIR_CONFLICT:
            case DIR_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    folder.setSyncDirConflict(folder.getCatExtraDescription()); //take over category conflict
                else
                    folder.setSyncDir(dirCfg.conflict);
                break;
        }

        recurse(folder);
    }

    const DirectionSet dirCfg;
};

//---------------------------------------------------------------------------------------------------------------

//test if non-equal items exist in scanned data
bool allItemsCategoryEqual(const HierarchyObject& hierObj)
{
    return std::all_of(hierObj.refSubFiles().begin(), hierObj.refSubFiles().end(),
    [](const FilePair& file) { return file.getCategory() == FILE_EQUAL; })&&   //files

    std::all_of(hierObj.refSubLinks().begin(), hierObj.refSubLinks().end(),
    [](const SymlinkPair& link) { return link.getLinkCategory() == SYMLINK_EQUAL; })&&   //symlinks

    std::all_of(hierObj.refSubFolders(). begin(), hierObj.refSubFolders().end(),
                [](const FolderPair& folder)
    {
        return folder.getDirCategory() == DIR_EQUAL && allItemsCategoryEqual(folder); //short circuit-behavior!
    });    //directories
}
}

bool zen::allElementsEqual(const FolderComparison& folderCmp)
{
    return std::all_of(begin(folderCmp), end(folderCmp), [](const BaseFolderPair& baseFolder) { return allItemsCategoryEqual(baseFolder); });
}

//---------------------------------------------------------------------------------------------------------------

namespace
{
template <SelectedSide side> inline
const InSyncDescrFile& getDescriptor(const InSyncFile& dbFile) { return dbFile.left; }

template <> inline
const InSyncDescrFile& getDescriptor<RIGHT_SIDE>(const InSyncFile& dbFile) { return dbFile.right; }


template <SelectedSide side> inline
bool matchesDbEntry(const FilePair& file, const InSyncFolder::FileList::value_type* dbFile, unsigned int optTimeShiftHours)
{
    if (file.isEmpty<side>())
        return !dbFile;
    else if (!dbFile)
        return false;

    const Zstring&     shortNameDb = dbFile->first;
    const InSyncDescrFile& descrDb = getDescriptor<side>(dbFile->second);

    return file.getItemName<side>() == shortNameDb && //detect changes in case (windows)
           //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes it's modification date by up to 2 seconds
           //we're not interested in "fileTimeTolerance" here!
           sameFileTime(file.getLastWriteTime<side>(), descrDb.lastWriteTimeRaw, 2, optTimeShiftHours) &&
           file.getFileSize<side>() == dbFile->second.fileSize;
    //note: we do *not* consider FileId here, but are only interested in *visual* changes. Consider user moving data to some other medium, this is not a change!
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncFile& dbFile, CompareVariant compareVar, int fileTimeTolerance, unsigned int optTimeShiftHours)
{
    switch (compareVar)
    {
        case CMP_BY_TIME_SIZE:
            if (dbFile.cmpVar == CMP_BY_CONTENT) return true; //special rule: this is certainly "good enough" for CMP_BY_TIME_SIZE!

            return //case-sensitive short name match is a database invariant!
                sameFileTime(dbFile.left.lastWriteTimeRaw, dbFile.right.lastWriteTimeRaw, fileTimeTolerance, optTimeShiftHours);

        case CMP_BY_CONTENT:
            //case-sensitive short name match is a database invariant!
            return dbFile.cmpVar == CMP_BY_CONTENT;
            //in contrast to comparison, we don't care about modification time here!
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

template <SelectedSide side> inline
const InSyncDescrLink& getDescriptor(const InSyncSymlink& dbLink) { return dbLink.left; }

template <> inline
const InSyncDescrLink& getDescriptor<RIGHT_SIDE>(const InSyncSymlink& dbLink) { return dbLink.right; }


//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectedSide side> inline
bool matchesDbEntry(const SymlinkPair& symlink, const InSyncFolder::SymlinkList::value_type* dbSymlink, unsigned int optTimeShiftHours)
{
    if (symlink.isEmpty<side>())
        return !dbSymlink;
    else if (!dbSymlink)
        return false;

    const Zstring&     shortNameDb = dbSymlink->first;
    const InSyncDescrLink& descrDb = getDescriptor<side>(dbSymlink->second);

    return symlink.getItemName<side>() == shortNameDb &&
           //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes its modification date by up to 2 seconds
           sameFileTime(symlink.getLastWriteTime<side>(), descrDb.lastWriteTimeRaw, 2, optTimeShiftHours);
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncSymlink& dbLink, CompareVariant compareVar, int fileTimeTolerance, unsigned int optTimeShiftHours)
{
    switch (compareVar)
    {
        case CMP_BY_TIME_SIZE:
            if (dbLink.cmpVar == CMP_BY_CONTENT) return true; //special rule: this is already "good enough" for CMP_BY_TIME_SIZE!

            return //case-sensitive short name match is a database invariant!
                sameFileTime(dbLink.left.lastWriteTimeRaw, dbLink.right.lastWriteTimeRaw, fileTimeTolerance, optTimeShiftHours);

        case CMP_BY_CONTENT:
            //case-sensitive short name match is a database invariant!
            return dbLink.cmpVar == CMP_BY_CONTENT;
            //in contrast to comparison, we don't care about modification time here!
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectedSide side> inline
bool matchesDbEntry(const FolderPair& folder, const InSyncFolder::FolderList::value_type* dbFolder)
{
    if (folder.isEmpty<side>())
        return !dbFolder || dbFolder->second.status == InSyncFolder::DIR_STATUS_STRAW_MAN;
    else if (!dbFolder || dbFolder->second.status == InSyncFolder::DIR_STATUS_STRAW_MAN)
        return false;

    const Zstring& shortNameDb = dbFolder->first;

    return folder.getItemName<side>() == shortNameDb;
}


inline
bool stillInSync(const InSyncFolder& dbFolder)
{
    //case-sensitive short name match is a database invariant!
    //InSyncFolder::DIR_STATUS_STRAW_MAN considered
    return true;
}

//----------------------------------------------------------------------------------------------

class DetectMovedFiles
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) { DetectMovedFiles(baseFolder, dbFolder); }

private:
    DetectMovedFiles(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) :
        cmpVar           (baseFolder.getCompVariant()),
        fileTimeTolerance(baseFolder.getFileTimeTolerance()),
        optTimeShiftHours(baseFolder.getTimeShift())
    {
        recurse(baseFolder, &dbFolder);

        if ((!exLeftOnlyById .empty() || !exLeftOnlyByPath .empty()) &&
            (!exRightOnlyById.empty() || !exRightOnlyByPath.empty()))
            detectMovePairs(dbFolder);
    }

    void recurse(HierarchyObject& hierObj, const InSyncFolder* dbFolder)
    {
        for (FilePair& file : hierObj.refSubFiles())
        {
            auto getDbFileEntry = [&]() -> const InSyncFile* //evaluate lazily!
            {
                if (dbFolder)
                {
                    auto it = dbFolder->files.find(file.getPairItemName());
                    if (it != dbFolder->files.end())
                        return &it->second;
                }
                return nullptr;
            };

            const CompareFilesResult cat = file.getCategory();

            if (cat == FILE_LEFT_SIDE_ONLY)
            {
                if (const InSyncFile* dbFile = getDbFileEntry())
                    exLeftOnlyByPath.emplace(dbFile, &file);
                else if (!file.getFileId<LEFT_SIDE>().empty())
                {
                    auto rv = exLeftOnlyById.emplace(file.getFileId<LEFT_SIDE>(), &file);
                    if (!rv.second) //duplicate file ID! NTFS hard link/symlink?
                        rv.first->second = nullptr;
                }
            }
            else if (cat == FILE_RIGHT_SIDE_ONLY)
            {
                if (const InSyncFile* dbFile = getDbFileEntry())
                    exRightOnlyByPath.emplace(dbFile, &file);
                else if (!file.getFileId<RIGHT_SIDE>().empty())
                {
                    auto rv = exRightOnlyById.emplace(file.getFileId<RIGHT_SIDE>(), &file);
                    if (!rv.second) //duplicate file ID! NTFS hard link/symlink?
                        rv.first->second = nullptr;
                }
            }
        }

        for (FolderPair& folder : hierObj.refSubFolders())
        {
            const InSyncFolder* dbSubFolder = nullptr; //try to find corresponding database entry
            if (dbFolder)
            {
                auto it = dbFolder->folders.find(folder.getPairItemName());
                if (it != dbFolder->folders.end())
                    dbSubFolder = &it->second;
            }

            recurse(folder, dbSubFolder);
        }
    }

    void detectMovePairs(const InSyncFolder& container) const
    {
        for (auto& dbFile : container.files)
            findAndSetMovePair(dbFile.second);

        for (auto& dbFolder : container.folders)
            detectMovePairs(dbFolder.second);
    }

    template <SelectedSide side>
    static bool sameSizeAndDate(const FilePair& file, const InSyncFile& dbFile)
    {
        return file.getFileSize<side>() == dbFile.fileSize &&
               sameFileTime(file.getLastWriteTime<side>(), getDescriptor<side>(dbFile).lastWriteTimeRaw, 2, 0);
        //- respect 2 second FAT/FAT32 precision!
        //- a "optTimeShiftHours" != 0 may lead to false positive move detections => let's be conservative and not allow it
        //  (time shift is only ever required during FAT DST switches)

        //PS: *never* allow 2 sec tolerance as container predicate!!
        // => no strict weak ordering relation! reason: no transitivity of equivalence!
    }

    template <SelectedSide side>
    static FilePair* getAssocFilePair(const InSyncFile& dbFile,
                                      const std::unordered_map<AFS::FileId, FilePair*, StringHash>& exOneSideById,
                                      const std::unordered_map<const InSyncFile*, FilePair*>& exOneSideByPath)
    {
        {
            auto it = exOneSideByPath.find(&dbFile);
            if (it != exOneSideByPath.end())
                return it->second; //if there is an association by path, don't care if there is also an association by id,
            //even if the association by path doesn't match time and size while the association by id does!
            //- there doesn't seem to be (any?) value in allowing this!
            //- note: exOneSideById isn't filled in this case, see recurse()
        }

        const AFS::FileId fileId = getDescriptor<side>(dbFile).fileId;
        if (!fileId.empty())
        {
            auto it = exOneSideById.find(fileId);
            if (it != exOneSideById.end())
                return it->second; //= nullptr, if duplicate ID!
        }
        return nullptr;
    }

    void findAndSetMovePair(const InSyncFile& dbFile) const
    {
        if (stillInSync(dbFile, cmpVar, fileTimeTolerance, optTimeShiftHours))
            if (FilePair* fileLeftOnly = getAssocFilePair<LEFT_SIDE>(dbFile, exLeftOnlyById, exLeftOnlyByPath))
                if (sameSizeAndDate<LEFT_SIDE>(*fileLeftOnly, dbFile))
                    if (FilePair* fileRightOnly = getAssocFilePair<RIGHT_SIDE>(dbFile, exRightOnlyById, exRightOnlyByPath))
                        if (sameSizeAndDate<RIGHT_SIDE>(*fileRightOnly, dbFile))
                            if (fileLeftOnly ->getMoveRef() == nullptr && //don't let a row participate in two move pairs!
                                fileRightOnly->getMoveRef() == nullptr)   //
                            {
                                fileLeftOnly ->setMoveRef(fileRightOnly->getId()); //found a pair, mark it!
                                fileRightOnly->setMoveRef(fileLeftOnly ->getId()); //
                            }
    }

    const CompareVariant cmpVar;
    const int fileTimeTolerance;
    const unsigned int optTimeShiftHours;

    std::unordered_map<AFS::FileId, FilePair*, StringHash> exLeftOnlyById;  //FilePair* == nullptr for duplicate ids! => consider aliasing through symlinks!
    std::unordered_map<AFS::FileId, FilePair*, StringHash> exRightOnlyById; //=> avoid ambiguity for mixtures of files/symlinks on one side and allow 1-1 mapping only!
    //MSVC: std::unordered_map: about twice as fast as std::map for 1 million items!

    std::unordered_map<const InSyncFile*, FilePair*> exLeftOnlyByPath; //MSVC: only 4% faster than std::map for 1 million items!
    std::unordered_map<const InSyncFile*, FilePair*> exRightOnlyByPath;
    /*
    detect renamed files:

     X  ->  |_|      Create right
    |_| ->   Y       Delete right

    is detected as:

    Rename Y to X on right

    Algorithm:
    ----------
    DB-file left  <--- (name, size, date) --->  DB-file right
          |                                          |
          |  (file ID, size, date)                   |  (file ID, size, date)
          |            or                            |            or
          |  (file path, size, date)                 |  (file path, size, date)
         \|/                                        \|/
    file left only                             file right only

       FAT caveat: File Ids are generally not stable when file is either moved or renamed!
       => 1. Move/rename operations on FAT cannot be detected reliably.
       => 2. database generally contains wrong file ID on FAT after renaming from .ffs_tmp files => correct file Ids in database only after next sync
       => 3. even exFAT screws up (but less than FAT) and changes IDs after file move. Did they learn nothing from the past?
    */
};

//----------------------------------------------------------------------------------------------

class RedetermineTwoWay
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) { RedetermineTwoWay(baseFolder, dbFolder); }

private:
    RedetermineTwoWay(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) :
        txtBothSidesChanged(_("Both sides have changed since last synchronization.")),
        txtNoSideChanged(_("Cannot determine sync-direction:") + L" \n" + _("No change since last synchronization.")),
        txtDbNotInSync(_("Cannot determine sync-direction:") + L" \n" + _("The database entry is not in sync considering current settings.")),
        cmpVar           (baseFolder.getCompVariant()),
        fileTimeTolerance(baseFolder.getFileTimeTolerance()),
        optTimeShiftHours(baseFolder.getTimeShift())
    {
        //-> considering filter not relevant:
        //if narrowing filter: all ok; if widening filter (if file ex on both sides -> conflict, fine; if file ex. on one side: copy to other side: fine)

        recurse(baseFolder, &dbFolder);
    }

    void recurse(HierarchyObject& hierObj, const InSyncFolder* dbFolder) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file, dbFolder);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processSymlink(link, dbFolder);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder, dbFolder);
    }

    void processFile(FilePair& file, const InSyncFolder* dbFolder) const
    {
        const CompareFilesResult cat = file.getCategory();
        if (cat == FILE_EQUAL)
            return;

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        //try to find corresponding database entry
        const InSyncFolder::FileList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->files.find(file.getPairItemName());
            if (it != dbFolder->files.end())
                dbEntry = &*it;
        }

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(file, dbEntry, optTimeShiftHours);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(file, dbEntry, optTimeShiftHours);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> do not set direction based on async status!
            if (dbEntry && !stillInSync(dbEntry->second, cmpVar, fileTimeTolerance, optTimeShiftHours))
                file.setSyncDirConflict(txtDbNotInSync);
            else
                file.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                file.setSyncDirConflict(txtBothSidesChanged);
            else
                file.setSyncDirConflict(txtNoSideChanged);
        }
    }

    void processSymlink(SymlinkPair& symlink, const InSyncFolder* dbFolder) const
    {
        const CompareSymlinkResult cat = symlink.getLinkCategory();
        if (cat == SYMLINK_EQUAL)
            return;

        //try to find corresponding database entry
        const InSyncFolder::SymlinkList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->symlinks.find(symlink.getPairItemName());
            if (it != dbFolder->symlinks.end())
                dbEntry = &*it;
        }

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(symlink, dbEntry, optTimeShiftHours);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(symlink, dbEntry, optTimeShiftHours);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> do not set direction based on async status!
            if (dbEntry && !stillInSync(dbEntry->second, cmpVar, fileTimeTolerance, optTimeShiftHours))
                symlink.setSyncDirConflict(txtDbNotInSync);
            else
                symlink.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                symlink.setSyncDirConflict(txtBothSidesChanged);
            else
                symlink.setSyncDirConflict(txtNoSideChanged);
        }
    }

    void processDir(FolderPair& folder, const InSyncFolder* dbFolder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, folder); //don't recurse below!
        //#######################################################################################

        //try to find corresponding database entry
        const InSyncFolder::FolderList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->folders.find(folder.getPairItemName());
            if (it != dbFolder->folders.end())
                dbEntry = &*it;
        }

        if (cat != DIR_EQUAL)
        {
            //evaluation
            const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(folder, dbEntry);
            const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(folder, dbEntry);

            if (changeOnLeft != changeOnRight)
            {
                //if database entry not in sync according to current settings! -> do not set direction based on async status!
                if (dbEntry && !stillInSync(dbEntry->second))
                    folder.setSyncDirConflict(txtDbNotInSync);
                else
                    folder.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
            }
            else
            {
                if (changeOnLeft)
                    folder.setSyncDirConflict(txtBothSidesChanged);
                else
                    folder.setSyncDirConflict(txtNoSideChanged);
            }
        }

        recurse(folder, dbEntry ? &dbEntry->second : nullptr);
    }

    const std::wstring txtBothSidesChanged;
    const std::wstring txtNoSideChanged;
    const std::wstring txtDbNotInSync;

    const CompareVariant cmpVar;
    const int fileTimeTolerance;
    const unsigned int optTimeShiftHours;
};
}

//---------------------------------------------------------------------------------------------------------------

std::vector<DirectionConfig> zen::extractDirectionCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    std::vector<DirectionConfig> output;
    for (const FolderPairEnh& fp : allPairs)
        output.push_back(fp.altSyncConfig.get() ? fp.altSyncConfig->directionCfg : mainCfg.syncCfg.directionCfg);

    return output;
}


void zen::redetermineSyncDirection(const DirectionConfig& dirCfg,
                                   BaseFolderPair& baseFolder,
                                   const std::function<void(const std::wstring& msg)>& reportWarning,
                                   const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)
{
    //try to load sync-database files
    std::shared_ptr<InSyncFolder> lastSyncState;
    if (dirCfg.var == DirectionConfig::TWOWAY || detectMovedFilesEnabled(dirCfg))
        try
        {
            if (allItemsCategoryEqual(baseFolder))
                return; //nothing to do: abort and don't even try to open db files

            lastSyncState = loadLastSynchronousState(baseFolder, onUpdateStatus); //throw FileError, FileErrorDatabaseNotExisting
        }
        catch (FileErrorDatabaseNotExisting&) {} //let's ignore this error, there's no value in reporting it other than confuse users
        catch (const FileError& e) //e.g. incompatible database version
        {
            if (reportWarning)
                reportWarning(e.toString() +
                              (dirCfg.var == DirectionConfig::TWOWAY ?
                               L" \n\n" + _("Setting default synchronization directions: Old files will be overwritten with newer files.") : std::wstring()));
        }

    //set sync directions
    if (dirCfg.var == DirectionConfig::TWOWAY)
    {
        if (lastSyncState)
            RedetermineTwoWay::execute(baseFolder, *lastSyncState);
        else //default fallback
            Redetermine::execute(getTwoWayUpdateSet(), baseFolder);
    }
    else
        Redetermine::execute(extractDirections(dirCfg), baseFolder);

    //detect renamed files
    if (lastSyncState)
        DetectMovedFiles::execute(baseFolder, *lastSyncState);
}


void zen::redetermineSyncDirection(const MainConfiguration& mainCfg,
                                   FolderComparison& folderCmp,
                                   const std::function<void(const std::wstring& msg)>& reportWarning,
                                   const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)
{
    if (folderCmp.empty())
        return;

    std::vector<DirectionConfig> directCfgs = extractDirectionCfg(mainCfg);

    if (folderCmp.size() != directCfgs.size())
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
    {
        const DirectionConfig& cfg = directCfgs[it - folderCmp.begin()];
        redetermineSyncDirection(cfg, **it, reportWarning, onUpdateStatus);
    }
}

//---------------------------------------------------------------------------------------------------------------

struct SetNewDirection
{
    static void execute(FilePair& file, SyncDirection newDirection)
    {
        if (file.getCategory() != FILE_EQUAL)
            file.setSyncDir(newDirection);
    }

    static void execute(SymlinkPair& symlink, SyncDirection newDirection)
    {
        if (symlink.getLinkCategory() != SYMLINK_EQUAL)
            symlink.setSyncDir(newDirection);
    }

    static void execute(FolderPair& folder, SyncDirection newDirection)
    {
        if (folder.getDirCategory() != DIR_EQUAL)
            folder.setSyncDir(newDirection);

        //recurse:
        for (FilePair& file : folder.refSubFiles())
            execute(file, newDirection);
        for (SymlinkPair& link : folder.refSubLinks())
            execute(link, newDirection);
        for (FolderPair& subFolder : folder.refSubFolders())
            execute(subFolder, newDirection);
    }
};


void zen::setSyncDirectionRec(SyncDirection newDirection, FileSystemObject& fsObj)
{
    //process subdirectories also!
    struct Recurse: public FSObjectVisitor
    {
        Recurse(SyncDirection newDir) : newDir_(newDir) {}
        void visit(const FilePair& file) override
        {
            SetNewDirection::execute(const_cast<FilePair&>(file), newDir_); //phyiscal object is not const in this method anyway
        }
        void visit(const SymlinkPair& symlink) override
        {
            SetNewDirection::execute(const_cast<SymlinkPair&>(symlink), newDir_); //
        }
        void visit(const FolderPair& folder) override
        {
            SetNewDirection::execute(const_cast<FolderPair&>(folder), newDir_); //
        }
    private:
        const SyncDirection newDir_;
    } setDirVisitor(newDirection);
    fsObj.accept(setDirVisitor);
}

//--------------- functions related to filtering ------------------------------------------------------------------------------------

namespace
{
template <bool include>
void inOrExcludeAllRows(zen::HierarchyObject& hierObj)
{
    for (FilePair& file : hierObj.refSubFiles())
        file.setActive(include);
    for (SymlinkPair& link : hierObj.refSubLinks())
        link.setActive(include);
    for (FolderPair& folder : hierObj.refSubFolders())
    {
        folder.setActive(include);
        inOrExcludeAllRows<include>(folder); //recurse
    }
}
}


void zen::setActiveStatus(bool newStatus, zen::FolderComparison& folderCmp)
{
    if (newStatus)
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { inOrExcludeAllRows<true>(baseFolder); }); //include all rows
    else
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { inOrExcludeAllRows<false>(baseFolder); }); //exclude all rows
}


void zen::setActiveStatus(bool newStatus, zen::FileSystemObject& fsObj)
{
    fsObj.setActive(newStatus);

    //process subdirectories also!
    struct Recurse: public FSObjectVisitor
    {
        Recurse(bool newStat) : newStatus_(newStat) {}
        void visit(const FilePair&    file) override {}
        void visit(const SymlinkPair& link) override {}
        void visit(const FolderPair&  folder) override
        {
            if (newStatus_)
                inOrExcludeAllRows<true>(const_cast<FolderPair&>(folder)); //object is not physically const here anyway
            else
                inOrExcludeAllRows<false>(const_cast<FolderPair&>(folder)); //
        }
    private:
        const bool newStatus_;
    } recurse(newStatus);
    fsObj.accept(recurse);
}

namespace
{
enum FilterStrategy
{
    STRATEGY_SET,
    STRATEGY_AND
    //STRATEGY_OR ->  usage of inOrExcludeAllRows doesn't allow for strategy "or"
};

template <FilterStrategy strategy> struct Eval;

template <>
struct Eval<STRATEGY_SET> //process all elements
{
    template <class T>
    static bool process(const T& obj) { return true; }
};

template <>
struct Eval<STRATEGY_AND>
{
    template <class T>
    static bool process(const T& obj) { return obj.isActive(); }
};


template <FilterStrategy strategy>
class ApplyHardFilter
{
public:
    static void execute(HierarchyObject& hierObj, const HardFilter& filterProcIn) { ApplyHardFilter(hierObj, filterProcIn); }

private:
    ApplyHardFilter(HierarchyObject& hierObj, const HardFilter& filterProcIn) : filterProc(filterProcIn)  { recurse(hierObj); }

    void recurse(HierarchyObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
            file.setActive(filterProc.passFileFilter(file.getPairRelativePath()));
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
            symlink.setActive(filterProc.passFileFilter(symlink.getPairRelativePath()));
    }

    void processDir(FolderPair& folder) const
    {
        bool childItemMightMatch = true;
        const bool filterPassed = filterProc.passDirFilter(folder.getPairRelativePath(), &childItemMightMatch);

        if (Eval<strategy>::process(folder))
            folder.setActive(filterPassed);

        if (!childItemMightMatch) //use same logic like directory traversing here: evaluate filter in subdirs only if objects could match
        {
            inOrExcludeAllRows<false>(folder); //exclude all files dirs in subfolders => incompatible with STRATEGY_OR!
            return;
        }

        recurse(folder);
    }

    const HardFilter& filterProc;
};


template <FilterStrategy strategy>
class ApplySoftFilter //falsify only! -> can run directly after "hard/base filter"
{
public:
    static void execute(HierarchyObject& hierObj, const SoftFilter& timeSizeFilter) { ApplySoftFilter(hierObj, timeSizeFilter); }

private:
    ApplySoftFilter(HierarchyObject& hierObj, const SoftFilter& timeSizeFilter) : timeSizeFilter_(timeSizeFilter) { recurse(hierObj); }

    void recurse(zen::HierarchyObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
        {
            if (file.isEmpty<LEFT_SIDE>())
                file.setActive(matchSize<RIGHT_SIDE>(file) &&
                               matchTime<RIGHT_SIDE>(file));
            else if (file.isEmpty<RIGHT_SIDE>())
                file.setActive(matchSize<LEFT_SIDE>(file) &&
                               matchTime<LEFT_SIDE>(file));
            else
            {
                //the only case with partially unclear semantics:
                //file and time filters may match or not match on each side, leaving a total of 16 combinations for both sides!
                /*
                               ST S T -       ST := match size and time
                               ---------       S := match size only
                            ST |I|I|I|I|       T := match time only
                            ------------       - := no match
                             S |I|E|?|E|
                            ------------       I := include row
                             T |I|?|E|E|       E := exclude row
                            ------------       ? := unclear
                             - |I|E|E|E|
                            ------------
                */
                //let's set ? := E
                file.setActive((matchSize<RIGHT_SIDE>(file) &&
                                matchTime<RIGHT_SIDE>(file)) ||
                               (matchSize<LEFT_SIDE>(file) &&
                                matchTime<LEFT_SIDE>(file)));
            }
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
        {
            if (symlink.isEmpty<LEFT_SIDE>())
                symlink.setActive(matchTime<RIGHT_SIDE>(symlink));
            else if (symlink.isEmpty<RIGHT_SIDE>())
                symlink.setActive(matchTime<LEFT_SIDE>(symlink));
            else
                symlink.setActive(matchTime<RIGHT_SIDE>(symlink) ||
                                  matchTime<LEFT_SIDE> (symlink));
        }
    }

    void processDir(FolderPair& folder) const
    {
        if (Eval<strategy>::process(folder))
            folder.setActive(timeSizeFilter_.matchFolder()); //if date filter is active we deactivate all folders: effectively gets rid of empty folders!

        recurse(folder);
    }

    template <SelectedSide side, class T>
    bool matchTime(const T& obj) const
    {
        return timeSizeFilter_.matchTime(obj.template getLastWriteTime<side>());
    }

    template <SelectedSide side, class T>
    bool matchSize(const T& obj) const
    {
        return timeSizeFilter_.matchSize(obj.template getFileSize<side>());
    }

    const SoftFilter timeSizeFilter_;
};
}


void zen::addHardFiltering(BaseFolderPair& baseFolder, const Zstring& excludeFilter)
{
    ApplyHardFilter<STRATEGY_AND>::execute(baseFolder, NameFilter(FilterConfig().includeFilter, excludeFilter));
}


void zen::addSoftFiltering(BaseFolderPair& baseFolder, const SoftFilter& timeSizeFilter)
{
    if (!timeSizeFilter.isNull()) //since we use STRATEGY_AND, we may skip a "null" filter
        ApplySoftFilter<STRATEGY_AND>::execute(baseFolder, timeSizeFilter);
}


void zen::applyFiltering(FolderComparison& folderCmp, const MainConfiguration& mainCfg)
{
    if (folderCmp.empty())
        return;
    else if (folderCmp.size() != mainCfg.additionalPairs.size() + 1)
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    for (auto it = allPairs.begin(); it != allPairs.end(); ++it)
    {
        BaseFolderPair& baseFolder = *folderCmp[it - allPairs.begin()];

        const NormalizedFilter normFilter = normalizeFilters(mainCfg.globalFilter, it->localFilter);

        //"set" hard filter
        ApplyHardFilter<STRATEGY_SET>::execute(baseFolder, *normFilter.nameFilter);

        //"and" soft filter
        addSoftFiltering(baseFolder, normFilter.timeSizeFilter);
    }
}


class FilterByTimeSpan
{
public:
    static void execute(HierarchyObject& hierObj, std::int64_t timeFrom, std::int64_t timeTo) { FilterByTimeSpan(hierObj, timeFrom, timeTo); }

private:
    FilterByTimeSpan(HierarchyObject& hierObj,
                     std::int64_t timeFrom,
                     std::int64_t timeTo) :
        timeFrom_(timeFrom),
        timeTo_(timeTo) { recurse(hierObj); }

    void recurse(HierarchyObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (file.isEmpty<LEFT_SIDE>())
            file.setActive(matchTime<RIGHT_SIDE>(file));
        else if (file.isEmpty<RIGHT_SIDE>())
            file.setActive(matchTime<LEFT_SIDE>(file));
        else
            file.setActive(matchTime<RIGHT_SIDE>(file) ||
                           matchTime<LEFT_SIDE>(file));
    }

    void processLink(SymlinkPair& link) const
    {
        if (link.isEmpty<LEFT_SIDE>())
            link.setActive(matchTime<RIGHT_SIDE>(link));
        else if (link.isEmpty<RIGHT_SIDE>())
            link.setActive(matchTime<LEFT_SIDE>(link));
        else
            link.setActive(matchTime<RIGHT_SIDE>(link) ||
                           matchTime<LEFT_SIDE> (link));
    }

    void processDir(FolderPair& folder) const
    {
        folder.setActive(false);
        recurse(folder);
    }

    template <SelectedSide side, class T>
    bool matchTime(const T& obj) const
    {
        return timeFrom_ <= obj.template getLastWriteTime<side>() &&
               obj.template getLastWriteTime<side>() <= timeTo_;
    }

    const std::int64_t timeFrom_;
    const std::int64_t timeTo_;
};


void zen::applyTimeSpanFilter(FolderComparison& folderCmp, std::int64_t timeFrom, std::int64_t timeTo)
{
    std::for_each(begin(folderCmp), end(folderCmp), [&](BaseFolderPair& baseFolder) { FilterByTimeSpan::execute(baseFolder, timeFrom, timeTo); });
}

//############################################################################################################

std::pair<std::wstring, int> zen::getSelectedItemsAsString(const std::vector<FileSystemObject*>& selectionLeft,
                                                           const std::vector<FileSystemObject*>& selectionRight)
{
    //don't use wxString! its rather dumb linear allocation strategy brings perf down to a crawl!
    std::wstring fileList; //
    int totalDelCount = 0;

    for (const FileSystemObject* fsObj : selectionLeft)
        if (!fsObj->isEmpty<LEFT_SIDE>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<LEFT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    for (const FileSystemObject* fsObj : selectionRight)
        if (!fsObj->isEmpty<RIGHT_SIDE>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<RIGHT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    return std::make_pair(fileList, totalDelCount);
}


namespace
{
struct FSObjectLambdaVisitor : public FSObjectVisitor
{
    static void visit(FileSystemObject& fsObj,
                      const std::function<void(const FolderPair&  folder)>& onFolder,
                      const std::function<void(const FilePair&    file  )>& onFile,
                      const std::function<void(const SymlinkPair& link  )>& onSymlink)
    {
        FSObjectLambdaVisitor visitor(onFolder, onFile, onSymlink);
        fsObj.accept(visitor);
    }

private:
    FSObjectLambdaVisitor(const std::function<void(const FolderPair&  folder)>& onFolder,
                          const std::function<void(const FilePair&    file  )>& onFile,
                          const std::function<void(const SymlinkPair& link  )>& onSymlink) : onFolder_(onFolder), onFile_(onFile), onSymlink_(onSymlink) {}

    void visit(const FolderPair&  folder) override { if (onFolder_ ) onFolder_ (folder); }
    void visit(const FilePair&    file  ) override { if (onFile_   ) onFile_   (file); }
    void visit(const SymlinkPair& link  ) override { if (onSymlink_) onSymlink_(link); }

    const std::function<void(const FolderPair&  folder)> onFolder_;
    const std::function<void(const FilePair&    file  )> onFile_;
    const std::function<void(const SymlinkPair& link  )> onSymlink_;
};


template <SelectedSide side>
void copyToAlternateFolderFrom(const std::vector<FileSystemObject*>& rowsToCopy,
                               const AbstractPath& targetFolderPath,
                               bool keepRelPaths,
                               bool overwriteIfExists,
                               ProcessCallback& callback)
{
    auto notifyItemCopy = [&](const std::wstring& statusText, const std::wstring& displayPath)
    {
        callback.reportInfo(replaceCpy(statusText, L"%x", fmtPath(displayPath)));
    };

    const std::wstring txtCreatingFolder(_("Creating folder %x"       ));
    const std::wstring txtCreatingFile  (_("Creating file %x"         ));
    const std::wstring txtCreatingLink  (_("Creating symbolic link %x"));

    auto copyItem = [&](FileSystemObject& fsObj, const AbstractPath& targetPath) //throw FileError
    {
        const std::function<void()> deleteTargetItem = [&]
        {
            if (overwriteIfExists)
                try
                {
                    //file or (broken) file-symlink:
                    AFS::removeFile(targetPath); //throw FileError
                }
                catch (FileError&)
                {
                    //folder or folder-symlink:
                    if (AFS::folderExists(targetPath)) //directory or dir-symlink
                        AFS::removeFolderRecursively(targetPath, nullptr /*onBeforeFileDeletion*/, nullptr /*onBeforeFolderDeletion*/); //throw FileError
                    else
                        throw;
                }
        };

        FSObjectLambdaVisitor::visit(fsObj, [&](const FolderPair& folder)
        {
            StatisticsReporter statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingFolder, AFS::getDisplayPath(targetPath));

            try
            {
                //deleteTargetItem(); -> never delete pre-existing folders!!! => might delete child items we just copied!
                AFS::copyNewFolder(folder.getAbstractPath<side>(), targetPath, false /*copyFilePermissions*/); //throw FileError
            }
            catch (const FileError&) { if (!AFS::folderExists(targetPath)) throw; } //might already exist: see creation of intermediate directories below
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        },

        [&](const FilePair& file)
        {
            StatisticsReporter statReporter(1, file.getFileSize<side>(), callback);
            notifyItemCopy(txtCreatingFile, AFS::getDisplayPath(targetPath));

            auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };
            AFS::copyFileTransactional(file.getAbstractPath<side>(), targetPath, //throw FileError, ErrorFileLocked
                                       false /*copyFilePermissions*/, true /*transactionalCopy*/, deleteTargetItem, onNotifyCopyStatus);
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        },

        [&](const SymlinkPair& symlink)
        {
            StatisticsReporter statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingLink, AFS::getDisplayPath(targetPath));

            deleteTargetItem();
            AFS::copySymlink(symlink.getAbstractPath<side>(), targetPath, false /*copyFilePermissions*/); //throw FileError
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        });
    };

    for (FileSystemObject* fsObj : rowsToCopy)
        tryReportingError([&]
    {
        const Zstring& relPath = keepRelPaths ? fsObj->getRelativePath<side>() : fsObj->getItemName<side>();
        const AbstractPath targetItemPath = AFS::appendRelPath(targetFolderPath, relPath);

        try
        {
            copyItem(*fsObj, targetItemPath); //throw FileError
        }
        catch (FileError&)
        {
            //create intermediate directories if missing
            const AbstractPath targetParentPath = AFS::appendRelPath(targetFolderPath, beforeLast(relPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE));
            if (!AFS::somethingExists(targetParentPath)) //->(minor) file system race condition!
            {
                AFS::createFolderRecursively(targetParentPath); //throw FileError
                //retry: this should work now!
                copyItem(*fsObj, targetItemPath); //throw FileError
            }
            else
                throw;
        }
    }, callback); //throw X?
}
}


void zen::copyToAlternateFolder(const std::vector<FileSystemObject*>& rowsToCopyOnLeft,
                                const std::vector<FileSystemObject*>& rowsToCopyOnRight,
                                const Zstring& targetFolderPathPhrase,
                                bool keepRelPaths,
                                bool overwriteIfExists,
                                ProcessCallback& callback)
{
    std::vector<FileSystemObject*> itemSelectionLeft  = rowsToCopyOnLeft;
    std::vector<FileSystemObject*> itemSelectionRight = rowsToCopyOnRight;
    erase_if(itemSelectionLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty<LEFT_SIDE >(); });
    erase_if(itemSelectionRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); });

    const int itemCount = static_cast<int>(itemSelectionLeft.size() + itemSelectionRight.size());
    std::int64_t dataToProcess = 0;

    for (FileSystemObject* fsObj : itemSelectionLeft)
        FSObjectLambdaVisitor::visit(*fsObj, nullptr /*onDir*/,
        [&](const FilePair& file) {dataToProcess += static_cast<std::int64_t>(file.getFileSize<LEFT_SIDE>()); }, nullptr /*onSymlink*/);

    for (FileSystemObject* fsObj : itemSelectionRight)
        FSObjectLambdaVisitor::visit(*fsObj, nullptr /*onDir*/,
        [&](const FilePair& file) {dataToProcess += static_cast<std::int64_t>(file.getFileSize<RIGHT_SIDE>()); }, nullptr /*onSymlink*/);

    callback.initNewPhase(itemCount, dataToProcess, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    const AbstractPath targetFolderPath = createAbstractPath(targetFolderPathPhrase);

    copyToAlternateFolderFrom<LEFT_SIDE >(itemSelectionLeft,  targetFolderPath, keepRelPaths, overwriteIfExists, callback);
    copyToAlternateFolderFrom<RIGHT_SIDE>(itemSelectionRight, targetFolderPath, keepRelPaths, overwriteIfExists, callback);
}

//############################################################################################################

namespace
{
template <SelectedSide side>
void deleteFromGridAndHDOneSide(std::vector<FileSystemObject*>& rowsToDelete,
                                bool useRecycleBin,
                                ProcessCallback& callback)
{
    auto notifyItemDeletion = [&](const std::wstring& statusText, const std::wstring& displayPath)
    {
        callback.reportInfo(replaceCpy(statusText, L"%x", fmtPath(displayPath)));
    };

    std::wstring txtRemovingFile;
    std::wstring txtRemovingDirectory;
    std::wstring txtRemovingSymlink;

    if (useRecycleBin)
    {
        txtRemovingFile      = _("Moving file %x to the recycle bin");
        txtRemovingDirectory = _("Moving folder %x to the recycle bin");
        txtRemovingSymlink   = _("Moving symbolic link %x to the recycle bin");
    }
    else
    {
        txtRemovingFile      = _("Deleting file %x");
        txtRemovingDirectory = _("Deleting folder %x");
        txtRemovingSymlink   = _("Deleting symbolic link %x");
    }


    for (FileSystemObject* fsObj : rowsToDelete) //all pointers are required(!) to be bound
        tryReportingError([&]
    {
        StatisticsReporter statReporter(1, 0, callback);

        if (!fsObj->isEmpty<side>()) //element may be implicitly deleted, e.g. if parent folder was deleted first
        {
            FSObjectLambdaVisitor::visit(*fsObj,
            [&](const FolderPair& folder)
            {
                if (useRecycleBin)
                {
                    notifyItemDeletion(txtRemovingDirectory, AFS::getDisplayPath(folder.getAbstractPath<side>()));
                    AFS::recycleItemDirectly(folder.getAbstractPath<side>()); //throw FileError
                    statReporter.reportDelta(1, 0);
                }
                else
                {
                    auto onBeforeFileDeletion = [&](const std::wstring& displayPath)
                    {
                        statReporter.reportDelta(1, 0);
                        notifyItemDeletion(txtRemovingFile, displayPath);
                    };
                    auto onBeforeDirDeletion = [&](const std::wstring& displayPath)
                    {
                        statReporter.reportDelta(1, 0);
                        notifyItemDeletion(txtRemovingDirectory, displayPath);
                    };

                    AFS::removeFolderRecursively(folder.getAbstractPath<side>(), onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
                }
            },

            [&](const FilePair& file)
            {
                notifyItemDeletion(txtRemovingFile, AFS::getDisplayPath(file.getAbstractPath<side>()));

                if (useRecycleBin)
                    AFS::recycleItemDirectly(file.getAbstractPath<side>()); //throw FileError
                else
                    AFS::removeFile(file.getAbstractPath<side>()); //throw FileError
                statReporter.reportDelta(1, 0);
            },

            [&](const SymlinkPair& symlink)
            {
                notifyItemDeletion(txtRemovingSymlink, AFS::getDisplayPath(symlink.getAbstractPath<side>()));

                if (useRecycleBin)
                    AFS::recycleItemDirectly(symlink.getAbstractPath<side>()); //throw FileError
                else
                {
                    if (AFS::folderExists(symlink.getAbstractPath<side>())) //dir symlink
                        AFS::removeFolderSimple(symlink.getAbstractPath<side>()); //throw FileError
                    else //file symlink, broken symlink
                        AFS::removeFile(symlink.getAbstractPath<side>()); //throw FileError
                }
                statReporter.reportDelta(1, 0);
            });

            fsObj->removeObject<side>(); //if directory: removes recursively!
        }

        statReporter.reportFinished();

    }, callback); //throw X?
}


template <SelectedSide side>
void categorize(const std::vector<FileSystemObject*>& rows,
                std::vector<FileSystemObject*>& deletePermanent,
                std::vector<FileSystemObject*>& deleteRecyler,
                bool useRecycleBin,
                std::map<AbstractPath, bool, AFS::LessAbstractPath>& recyclerSupported,
                ProcessCallback& callback)
{
    auto hasRecycler = [&](const AbstractPath& baseFolderPath) -> bool
    {
        auto it = recyclerSupported.find(baseFolderPath); //perf: avoid duplicate checks!
        if (it != recyclerSupported.end())
            return it->second;

        const std::wstring msg = replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x", fmtPath(AFS::getDisplayPath(baseFolderPath)));

        bool recSupported = false;
        tryReportingError([&]{
            recSupported = AFS::supportsRecycleBin(baseFolderPath, [&] { callback.reportStatus(msg); /*may throw*/ }); //throw FileError
        }, callback); //throw X?

        recyclerSupported.emplace(baseFolderPath, recSupported);
        return recSupported;
    };

    for (FileSystemObject* row : rows)
        if (!row->isEmpty<side>())
        {
            if (useRecycleBin && hasRecycler(row->base().getAbstractPath<side>())) //Windows' ::SHFileOperation() will delete permanently anyway, but we have a superior deletion routine
                deleteRecyler.push_back(row);
            else
                deletePermanent.push_back(row);
        }
}
}


void zen::deleteFromGridAndHD(const std::vector<FileSystemObject*>& rowsToDeleteOnLeft,  //refresh GUI grid after deletion to remove invalid rows
                              const std::vector<FileSystemObject*>& rowsToDeleteOnRight, //all pointers need to be bound!
                              FolderComparison& folderCmp,                         //attention: rows will be physically deleted!
                              const std::vector<DirectionConfig>& directCfgs,
                              bool useRecycleBin,
                              bool& warningRecyclerMissing,
                              ProcessCallback& callback)
{
    if (folderCmp.empty())
        return;
    else if (folderCmp.size() != directCfgs.size())
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //build up mapping from base directory to corresponding direction config
    std::unordered_map<const BaseFolderPair*, DirectionConfig> baseFolderCfgs;
    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
        baseFolderCfgs[&** it] = directCfgs[it - folderCmp.begin()];

    std::vector<FileSystemObject*> deleteLeft  = rowsToDeleteOnLeft;
    std::vector<FileSystemObject*> deleteRight = rowsToDeleteOnRight;

    erase_if(deleteLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty<LEFT_SIDE >(); }); //needed?
    erase_if(deleteRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); }); //yes, for correct stats:

    const int itemCount = static_cast<int>(deleteLeft.size() + deleteRight.size());
    callback.initNewPhase(itemCount, 0, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    //ensure cleanup: redetermination of sync-directions and removal of invalid rows
    auto updateDirection = [&]
    {
        //update sync direction: we cannot do a full redetermination since the user may already have entered manual changes
        std::vector<FileSystemObject*> rowsToDelete;
        append(rowsToDelete, deleteLeft);
        append(rowsToDelete, deleteRight);
        removeDuplicates(rowsToDelete);

        for (auto it = rowsToDelete.begin(); it != rowsToDelete.end(); ++it)
        {
            FileSystemObject& fsObj = **it; //all pointers are required(!) to be bound

            if (fsObj.isEmpty<LEFT_SIDE>() != fsObj.isEmpty<RIGHT_SIDE>()) //make sure objects exists on one side only
            {
                auto cfgIter = baseFolderCfgs.find(&fsObj.base());
                assert(cfgIter != baseFolderCfgs.end());
                if (cfgIter != baseFolderCfgs.end())
                {
                    SyncDirection newDir = SyncDirection::NONE;

                    if (cfgIter->second.var == DirectionConfig::TWOWAY)
                        newDir = fsObj.isEmpty<LEFT_SIDE>() ? SyncDirection::RIGHT : SyncDirection::LEFT;
                    else
                    {
                        const DirectionSet& dirCfg = extractDirections(cfgIter->second);
                        newDir = fsObj.isEmpty<LEFT_SIDE>() ? dirCfg.exRightSideOnly : dirCfg.exLeftSideOnly;
                    }

                    setSyncDirectionRec(newDir, fsObj); //set new direction (recursively)
                }
            }
        }

        //last step: cleanup empty rows: this one invalidates all pointers!
        std::for_each(begin(folderCmp), end(folderCmp), BaseFolderPair::removeEmpty);
    };
    ZEN_ON_SCOPE_EXIT(updateDirection()); //MSVC: assert is a macro and it doesn't play nice with ZEN_ON_SCOPE_EXIT, surprise... wasn't there something about macros being "evil"?

    //categorize rows into permanent deletion and recycle bin
    std::vector<FileSystemObject*> deletePermanentLeft;
    std::vector<FileSystemObject*> deletePermanentRight;
    std::vector<FileSystemObject*> deleteRecylerLeft;
    std::vector<FileSystemObject*> deleteRecylerRight;

    std::map<AbstractPath, bool, AFS::LessAbstractPath> recyclerSupported;
    categorize<LEFT_SIDE >(deleteLeft,  deletePermanentLeft,  deleteRecylerLeft,  useRecycleBin, recyclerSupported, callback);
    categorize<RIGHT_SIDE>(deleteRight, deletePermanentRight, deleteRecylerRight, useRecycleBin, recyclerSupported, callback);

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    if (useRecycleBin &&
    std::any_of(recyclerSupported.begin(), recyclerSupported.end(), [](const auto& item) { return !item.second; }))
    {
        std::wstring msg = _("The recycle bin is not available for the following folders. Files will be deleted permanently instead:") + L"\n";

        for (const auto& item : recyclerSupported)
            if (!item.second)
                msg += L"\n" + AFS::getDisplayPath(item.first);

        callback.reportWarning(msg, warningRecyclerMissing); //throw?
    }

    deleteFromGridAndHDOneSide<LEFT_SIDE>(deleteRecylerLeft,   true,  callback);
    deleteFromGridAndHDOneSide<LEFT_SIDE>(deletePermanentLeft, false, callback);

    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deleteRecylerRight,   true,  callback);
    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deletePermanentRight, false, callback);
}
