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
    std::for_each(begin(folderCmp), end(folderCmp), [](BaseDirPair& baseObj) { baseObj.flip(); });
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
        for (FilePair& fileObj : hierObj.refSubFiles())
            processFile(fileObj);
        for (SymlinkPair& linkObj : hierObj.refSubLinks())
            processLink(linkObj);
        for (DirPair& dirObj : hierObj.refSubDirs())
            processDir(dirObj);
    }

    void processFile(FilePair& fileObj) const
    {
        const CompareFilesResult cat = fileObj.getCategory();

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(fileObj.getItemName<LEFT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return fileObj.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(fileObj.getItemName<RIGHT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return fileObj.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        switch (cat)
        {
            case FILE_LEFT_SIDE_ONLY:
                fileObj.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case FILE_RIGHT_SIDE_ONLY:
                fileObj.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case FILE_RIGHT_NEWER:
                fileObj.setSyncDir(dirCfg.rightNewer);
                break;
            case FILE_LEFT_NEWER:
                fileObj.setSyncDir(dirCfg.leftNewer);
                break;
            case FILE_DIFFERENT_CONTENT:
                fileObj.setSyncDir(dirCfg.different);
                break;
            case FILE_CONFLICT:
            case FILE_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    fileObj.setSyncDirConflict(fileObj.getCatExtraDescription()); //take over category conflict
                else
                    fileObj.setSyncDir(dirCfg.conflict);
                break;
            case FILE_EQUAL:
                fileObj.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processLink(SymlinkPair& linkObj) const
    {
        switch (linkObj.getLinkCategory())
        {
            case SYMLINK_LEFT_SIDE_ONLY:
                linkObj.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case SYMLINK_RIGHT_SIDE_ONLY:
                linkObj.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case SYMLINK_LEFT_NEWER:
                linkObj.setSyncDir(dirCfg.leftNewer);
                break;
            case SYMLINK_RIGHT_NEWER:
                linkObj.setSyncDir(dirCfg.rightNewer);
                break;
            case SYMLINK_CONFLICT:
            case SYMLINK_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    linkObj.setSyncDirConflict(linkObj.getCatExtraDescription()); //take over category conflict
                else
                    linkObj.setSyncDir(dirCfg.conflict);
                break;
            case SYMLINK_DIFFERENT_CONTENT:
                linkObj.setSyncDir(dirCfg.different);
                break;
            case SYMLINK_EQUAL:
                linkObj.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processDir(DirPair& dirObj) const
    {
        const CompareDirResult cat = dirObj.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(dirObj.getItemName<LEFT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, dirObj); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(dirObj.getItemName<RIGHT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, dirObj); //don't recurse below!
        //#######################################################################################

        switch (cat)
        {
            case DIR_LEFT_SIDE_ONLY:
                dirObj.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case DIR_RIGHT_SIDE_ONLY:
                dirObj.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case DIR_EQUAL:
                dirObj.setSyncDir(SyncDirection::NONE);
                break;
            case DIR_CONFLICT:
            case DIR_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    dirObj.setSyncDirConflict(dirObj.getCatExtraDescription()); //take over category conflict
                else
                    dirObj.setSyncDir(dirCfg.conflict);
                break;
        }

        recurse(dirObj);
    }

    const DirectionSet dirCfg;
};

//---------------------------------------------------------------------------------------------------------------

//test if non-equal items exist in scanned data
bool allItemsCategoryEqual(const HierarchyObject& hierObj)
{
    return std::all_of(hierObj.refSubFiles().begin(), hierObj.refSubFiles().end(),
    [](const FilePair& fileObj) { return fileObj.getCategory() == FILE_EQUAL; })&&  //files

    std::all_of(hierObj.refSubLinks().begin(), hierObj.refSubLinks().end(),
    [](const SymlinkPair& linkObj) { return linkObj.getLinkCategory() == SYMLINK_EQUAL; })&&  //symlinks

    std::all_of(hierObj.refSubDirs(). begin(), hierObj.refSubDirs().end(),
                [](const DirPair& dirObj)
    {
        return dirObj.getDirCategory() == DIR_EQUAL && allItemsCategoryEqual(dirObj); //short circuit-behavior!
    });    //directories
}
}

bool zen::allElementsEqual(const FolderComparison& folderCmp)
{
    return std::all_of(begin(folderCmp), end(folderCmp), [](const BaseDirPair& baseObj) { return allItemsCategoryEqual(baseObj); });
}

//---------------------------------------------------------------------------------------------------------------

namespace
{
template <SelectedSide side> inline
const InSyncDescrFile& getDescriptor(const InSyncFile& dbFile) { return dbFile.left; }

template <> inline
const InSyncDescrFile& getDescriptor<RIGHT_SIDE>(const InSyncFile& dbFile) { return dbFile.right; }


template <SelectedSide side> inline
bool matchesDbEntry(const FilePair& fileObj, const InSyncDir::FileList::value_type* dbFile, unsigned int optTimeShiftHours)
{
    if (fileObj.isEmpty<side>())
        return !dbFile;
    else if (!dbFile)
        return false;

    const Zstring&     shortNameDb = dbFile->first;
    const InSyncDescrFile& descrDb = getDescriptor<side>(dbFile->second);

    return fileObj.getItemName<side>() == shortNameDb && //detect changes in case (windows)
           //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes it's modification date by up to 2 seconds
           //we're not interested in "fileTimeTolerance" here!
           sameFileTime(fileObj.getLastWriteTime<side>(), descrDb.lastWriteTimeRaw, 2, optTimeShiftHours) &&
           fileObj.getFileSize<side>() == dbFile->second.fileSize;
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
bool matchesDbEntry(const SymlinkPair& linkObj, const InSyncDir::LinkList::value_type* dbLink, unsigned int optTimeShiftHours)
{
    if (linkObj.isEmpty<side>())
        return !dbLink;
    else if (!dbLink)
        return false;

    const Zstring&     shortNameDb = dbLink->first;
    const InSyncDescrLink& descrDb = getDescriptor<side>(dbLink->second);

    return linkObj.getItemName<side>() == shortNameDb &&
           //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes its modification date by up to 2 seconds
           sameFileTime(linkObj.getLastWriteTime<side>(), descrDb.lastWriteTimeRaw, 2, optTimeShiftHours);
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
bool matchesDbEntry(const DirPair& dirObj, const InSyncDir::DirList::value_type* dbDir)
{
    if (dirObj.isEmpty<side>())
        return !dbDir || dbDir->second.status == InSyncDir::DIR_STATUS_STRAW_MAN;
    else if (!dbDir || dbDir->second.status == InSyncDir::DIR_STATUS_STRAW_MAN)
        return false;

    const Zstring& shortNameDb = dbDir->first;

    return dirObj.getItemName<side>() == shortNameDb;
}


inline
bool stillInSync(const InSyncDir& dbDir)
{
    //case-sensitive short name match is a database invariant!
    //InSyncDir::DIR_STATUS_STRAW_MAN considered
    return true;
}

//----------------------------------------------------------------------------------------------

class DetectMovedFiles
{
public:
    static void execute(BaseDirPair& baseDirectory, const InSyncDir& dbFolder) { DetectMovedFiles(baseDirectory, dbFolder); }

private:
    DetectMovedFiles(BaseDirPair& baseDirectory, const InSyncDir& dbFolder) :
        cmpVar           (baseDirectory.getCompVariant()),
        fileTimeTolerance(baseDirectory.getFileTimeTolerance()),
        optTimeShiftHours(baseDirectory.getTimeShift())
    {
        recurse(baseDirectory, &dbFolder);

        if ((!exLeftOnlyById .empty() || !exLeftOnlyByPath .empty()) &&
            (!exRightOnlyById.empty() || !exRightOnlyByPath.empty()))
            detectMovePairs(dbFolder);
    }

    void recurse(HierarchyObject& hierObj, const InSyncDir* dbFolder)
    {
        for (FilePair& fileObj : hierObj.refSubFiles())
        {
            auto getDbFileEntry = [&]() -> const InSyncFile* //evaluate lazily!
            {
                if (dbFolder)
                {
                    auto it = dbFolder->files.find(fileObj.getPairShortName());
                    if (it != dbFolder->files.end())
                        return &it->second;
                }
                return nullptr;
            };

            const CompareFilesResult cat = fileObj.getCategory();

            if (cat == FILE_LEFT_SIDE_ONLY)
            {
                if (const InSyncFile* dbFile = getDbFileEntry())
                    exLeftOnlyByPath.emplace(dbFile, &fileObj);
                else if (!fileObj.getFileId<LEFT_SIDE>().empty())
                {
                    auto rv = exLeftOnlyById.emplace(fileObj.getFileId<LEFT_SIDE>(), &fileObj);
                    if (!rv.second) //duplicate file ID! NTFS hard link/symlink?
                        rv.first->second = nullptr;
                }
            }
            else if (cat == FILE_RIGHT_SIDE_ONLY)
            {
                if (const InSyncFile* dbFile = getDbFileEntry())
                    exRightOnlyByPath.emplace(dbFile, &fileObj);
                else if (!fileObj.getFileId<RIGHT_SIDE>().empty())
                {
                    auto rv = exRightOnlyById.emplace(fileObj.getFileId<RIGHT_SIDE>(), &fileObj);
                    if (!rv.second) //duplicate file ID! NTFS hard link/symlink?
                        rv.first->second = nullptr;
                }
            }
        }

        for (DirPair& dirObj : hierObj.refSubDirs())
        {
            const InSyncDir* dbSubFolder = nullptr; //try to find corresponding database entry
            if (dbFolder)
            {
                auto it = dbFolder->dirs.find(dirObj.getPairShortName());
                if (it != dbFolder->dirs.end())
                    dbSubFolder = &it->second;
            }

            recurse(dirObj, dbSubFolder);
        }
    }

    void detectMovePairs(const InSyncDir& container) const
    {
        for (auto& dbFile : container.files)
            findAndSetMovePair(dbFile.second);

        for (auto& dbDir : container.dirs)
            detectMovePairs(dbDir.second);
    }

    template <SelectedSide side>
    static bool sameSizeAndDate(const FilePair& fileObj, const InSyncFile& dbFile)
    {
        return fileObj.getFileSize<side>() == dbFile.fileSize &&
               sameFileTime(fileObj.getLastWriteTime<side>(), getDescriptor<side>(dbFile).lastWriteTimeRaw, 2, 0);
        //- respect 2 second FAT/FAT32 precision!
        //- a "optTimeShiftHours" != 0 may lead to false positive move detections => let's be conservative and not allow it
        //  (time shift is only ever required during FAT DST switches)

        //PS: *never* allow 2 sec tolerance as container predicate!!
        // => no strict weak ordering relation! reason: no transitivity of equivalence!
    }

    template <SelectedSide side>
    static FilePair* getAssocFilePair(const InSyncFile& dbFile,
                                      const std::unordered_map<ABF::FileId, FilePair*, StringHash>& exOneSideById,
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

        const ABF::FileId fileId = getDescriptor<side>(dbFile).fileId;
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

    std::unordered_map<ABF::FileId, FilePair*, StringHash> exLeftOnlyById;  //FilePair* == nullptr for duplicate ids! => consider aliasing through symlinks!
    std::unordered_map<ABF::FileId, FilePair*, StringHash> exRightOnlyById; //=> avoid ambiguity for mixtures of files/symlinks on one side and allow 1-1 mapping only!
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
    static void execute(BaseDirPair& baseDirectory, const InSyncDir& dbFolder) { RedetermineTwoWay(baseDirectory, dbFolder); }

private:
    RedetermineTwoWay(BaseDirPair& baseDirectory, const InSyncDir& dbFolder) :
        txtBothSidesChanged(_("Both sides have changed since last synchronization.")),
        txtNoSideChanged(_("Cannot determine sync-direction:") + L" \n" + _("No change since last synchronization.")),
        txtDbNotInSync(_("Cannot determine sync-direction:") + L" \n" + _("The database entry is not in sync considering current settings.")),
        cmpVar           (baseDirectory.getCompVariant()),
        fileTimeTolerance(baseDirectory.getFileTimeTolerance()),
        optTimeShiftHours(baseDirectory.getTimeShift())
    {
        //-> considering filter not relevant:
        //if narrowing filter: all ok; if widening filter (if file ex on both sides -> conflict, fine; if file ex. on one side: copy to other side: fine)

        recurse(baseDirectory, &dbFolder);
    }

    void recurse(HierarchyObject& hierObj, const InSyncDir* dbFolder) const
    {
        for (FilePair& fileObj : hierObj.refSubFiles())
            processFile(fileObj, dbFolder);
        for (SymlinkPair& linkObj : hierObj.refSubLinks())
            processSymlink(linkObj, dbFolder);
        for (DirPair& dirObj : hierObj.refSubDirs())
            processDir(dirObj, dbFolder);
    }

    void processFile(FilePair& fileObj, const InSyncDir* dbFolder) const
    {
        const CompareFilesResult cat = fileObj.getCategory();
        if (cat == FILE_EQUAL)
            return;

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(fileObj.getItemName<LEFT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return fileObj.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(fileObj.getItemName<RIGHT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return fileObj.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        //try to find corresponding database entry
        const InSyncDir::FileList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->files.find(fileObj.getPairShortName());
            if (it != dbFolder->files.end())
                dbEntry = &*it;
        }

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(fileObj, dbEntry, optTimeShiftHours);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(fileObj, dbEntry, optTimeShiftHours);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> do not set direction based on async status!
            if (dbEntry && !stillInSync(dbEntry->second, cmpVar, fileTimeTolerance, optTimeShiftHours))
                fileObj.setSyncDirConflict(txtDbNotInSync);
            else
                fileObj.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                fileObj.setSyncDirConflict(txtBothSidesChanged);
            else
                fileObj.setSyncDirConflict(txtNoSideChanged);
        }
    }

    void processSymlink(SymlinkPair& linkObj, const InSyncDir* dbFolder) const
    {
        const CompareSymlinkResult cat = linkObj.getLinkCategory();
        if (cat == SYMLINK_EQUAL)
            return;

        //try to find corresponding database entry
        const InSyncDir::LinkList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->symlinks.find(linkObj.getPairShortName());
            if (it != dbFolder->symlinks.end())
                dbEntry = &*it;
        }

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(linkObj, dbEntry, optTimeShiftHours);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(linkObj, dbEntry, optTimeShiftHours);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> do not set direction based on async status!
            if (dbEntry && !stillInSync(dbEntry->second, cmpVar, fileTimeTolerance, optTimeShiftHours))
                linkObj.setSyncDirConflict(txtDbNotInSync);
            else
                linkObj.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                linkObj.setSyncDirConflict(txtBothSidesChanged);
            else
                linkObj.setSyncDirConflict(txtNoSideChanged);
        }
    }

    void processDir(DirPair& dirObj, const InSyncDir* dbFolder) const
    {
        const CompareDirResult cat = dirObj.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(dirObj.getItemName<LEFT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, dirObj); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(dirObj.getItemName<RIGHT_SIDE>(), ABF::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, dirObj); //don't recurse below!
        //#######################################################################################

        //try to find corresponding database entry
        const InSyncDir::DirList::value_type* dbEntry = nullptr;
        if (dbFolder)
        {
            auto it = dbFolder->dirs.find(dirObj.getPairShortName());
            if (it != dbFolder->dirs.end())
                dbEntry = &*it;
        }

        if (cat != DIR_EQUAL)
        {
            //evaluation
            const bool changeOnLeft  = !matchesDbEntry<LEFT_SIDE >(dirObj, dbEntry);
            const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(dirObj, dbEntry);

            if (changeOnLeft != changeOnRight)
            {
                //if database entry not in sync according to current settings! -> do not set direction based on async status!
                if (dbEntry && !stillInSync(dbEntry->second))
                    dirObj.setSyncDirConflict(txtDbNotInSync);
                else
                    dirObj.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
            }
            else
            {
                if (changeOnLeft)
                    dirObj.setSyncDirConflict(txtBothSidesChanged);
                else
                    dirObj.setSyncDirConflict(txtNoSideChanged);
            }
        }

        recurse(dirObj, dbEntry ? &dbEntry->second : nullptr);
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
                                   BaseDirPair& baseDirectory,
                                   const std::function<void(const std::wstring& msg)>& reportWarning,
                                   const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)
{
    //try to load sync-database files
    std::shared_ptr<InSyncDir> lastSyncState;
    if (dirCfg.var == DirectionConfig::TWOWAY || detectMovedFilesEnabled(dirCfg))
        try
        {
            if (allItemsCategoryEqual(baseDirectory))
                return; //nothing to do: abort and don't even try to open db files

            lastSyncState = loadLastSynchronousState(baseDirectory, onUpdateStatus); //throw FileError, FileErrorDatabaseNotExisting
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
            RedetermineTwoWay::execute(baseDirectory, *lastSyncState);
        else //default fallback
            Redetermine::execute(getTwoWayUpdateSet(), baseDirectory);
    }
    else
        Redetermine::execute(extractDirections(dirCfg), baseDirectory);

    //detect renamed files
    if (lastSyncState)
        DetectMovedFiles::execute(baseDirectory, *lastSyncState);
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
    static void execute(FilePair& fileObj, SyncDirection newDirection)
    {
        if (fileObj.getCategory() != FILE_EQUAL)
            fileObj.setSyncDir(newDirection);
    }

    static void execute(SymlinkPair& linkObj, SyncDirection newDirection)
    {
        if (linkObj.getLinkCategory() != SYMLINK_EQUAL)
            linkObj.setSyncDir(newDirection);
    }

    static void execute(DirPair& dirObj, SyncDirection newDirection)
    {
        if (dirObj.getDirCategory() != DIR_EQUAL)
            dirObj.setSyncDir(newDirection);

        //recurse:
        for (FilePair& fileObj : dirObj.refSubFiles())
            execute(fileObj, newDirection);
        for (SymlinkPair& linkObj : dirObj.refSubLinks())
            execute(linkObj, newDirection);
        for (DirPair& dirObj2 : dirObj.refSubDirs())
            execute(dirObj2, newDirection);
    }
};


void zen::setSyncDirectionRec(SyncDirection newDirection, FileSystemObject& fsObj)
{
    //process subdirectories also!
    struct Recurse: public FSObjectVisitor
    {
        Recurse(SyncDirection newDir) : newDir_(newDir) {}
        void visit(const FilePair& fileObj) override
        {
            SetNewDirection::execute(const_cast<FilePair&>(fileObj), newDir_); //phyiscal object is not const in this method anyway
        }
        void visit(const SymlinkPair& linkObj) override
        {
            SetNewDirection::execute(const_cast<SymlinkPair&>(linkObj), newDir_); //
        }
        void visit(const DirPair& dirObj) override
        {
            SetNewDirection::execute(const_cast<DirPair&>(dirObj), newDir_); //
        }
    private:
        SyncDirection newDir_;
    } setDirVisitor(newDirection);
    fsObj.accept(setDirVisitor);
}

//--------------- functions related to filtering ------------------------------------------------------------------------------------

namespace
{
template <bool include>
void inOrExcludeAllRows(zen::HierarchyObject& hierObj)
{
    for (FilePair& fileObj : hierObj.refSubFiles())
        fileObj.setActive(include);
    for (SymlinkPair& linkObj : hierObj.refSubLinks())
        linkObj.setActive(include);
    for (DirPair& dirObj : hierObj.refSubDirs())
    {
        dirObj.setActive(include);
        inOrExcludeAllRows<include>(dirObj); //recurse
    }
}
}


void zen::setActiveStatus(bool newStatus, zen::FolderComparison& folderCmp)
{
    if (newStatus)
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseDirPair& baseDirObj) { inOrExcludeAllRows<true>(baseDirObj); }); //include all rows
    else
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseDirPair& baseDirObj) { inOrExcludeAllRows<false>(baseDirObj); }); //exclude all rows
}


void zen::setActiveStatus(bool newStatus, zen::FileSystemObject& fsObj)
{
    fsObj.setActive(newStatus);

    //process subdirectories also!
    struct Recurse: public FSObjectVisitor
    {
        Recurse(bool newStat) : newStatus_(newStat) {}
        void visit(const FilePair&    fileObj) override {}
        void visit(const SymlinkPair& linkObj) override {}
        void visit(const DirPair&      dirObj) override
        {
            if (newStatus_)
                inOrExcludeAllRows<true>(const_cast<DirPair&>(dirObj)); //object is not physically const here anyway
            else
                inOrExcludeAllRows<false>(const_cast<DirPair&>(dirObj)); //
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
        for (FilePair& fileObj : hierObj.refSubFiles())
            processFile(fileObj);
        for (SymlinkPair& linkObj : hierObj.refSubLinks())
            processLink(linkObj);
        for (DirPair& dirObj : hierObj.refSubDirs())
            processDir(dirObj);
    };

    void processFile(FilePair& fileObj) const
    {
        if (Eval<strategy>::process(fileObj))
            fileObj.setActive(filterProc.passFileFilter(fileObj.getPairRelativePath()));
    }

    void processLink(SymlinkPair& linkObj) const
    {
        if (Eval<strategy>::process(linkObj))
            linkObj.setActive(filterProc.passFileFilter(linkObj.getPairRelativePath()));
    }

    void processDir(DirPair& dirObj) const
    {
        bool childItemMightMatch = true;
        const bool filterPassed = filterProc.passDirFilter(dirObj.getPairRelativePath(), &childItemMightMatch);

        if (Eval<strategy>::process(dirObj))
            dirObj.setActive(filterPassed);

        if (!childItemMightMatch) //use same logic like directory traversing here: evaluate filter in subdirs only if objects could match
        {
            inOrExcludeAllRows<false>(dirObj); //exclude all files dirs in subfolders => incompatible with STRATEGY_OR!
            return;
        }

        recurse(dirObj);
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
        for (FilePair& fileObj : hierObj.refSubFiles())
            processFile(fileObj);
        for (SymlinkPair& linkObj : hierObj.refSubLinks())
            processLink(linkObj);
        for (DirPair& dirObj : hierObj.refSubDirs())
            processDir(dirObj);
    };

    void processFile(FilePair& fileObj) const
    {
        if (Eval<strategy>::process(fileObj))
        {
            if (fileObj.isEmpty<LEFT_SIDE>())
                fileObj.setActive(matchSize<RIGHT_SIDE>(fileObj) &&
                                  matchTime<RIGHT_SIDE>(fileObj));
            else if (fileObj.isEmpty<RIGHT_SIDE>())
                fileObj.setActive(matchSize<LEFT_SIDE>(fileObj) &&
                                  matchTime<LEFT_SIDE>(fileObj));
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
                fileObj.setActive((matchSize<RIGHT_SIDE>(fileObj) &&
                                   matchTime<RIGHT_SIDE>(fileObj)) ||
                                  (matchSize<LEFT_SIDE>(fileObj) &&
                                   matchTime<LEFT_SIDE>(fileObj)));
            }
        }
    }

    void processLink(SymlinkPair& linkObj) const
    {
        if (Eval<strategy>::process(linkObj))
        {
            if (linkObj.isEmpty<LEFT_SIDE>())
                linkObj.setActive(matchTime<RIGHT_SIDE>(linkObj));
            else if (linkObj.isEmpty<RIGHT_SIDE>())
                linkObj.setActive(matchTime<LEFT_SIDE>(linkObj));
            else
                linkObj.setActive(matchTime<RIGHT_SIDE>(linkObj) ||
                                  matchTime<LEFT_SIDE> (linkObj));
        }
    }

    void processDir(DirPair& dirObj) const
    {
        if (Eval<strategy>::process(dirObj))
            dirObj.setActive(timeSizeFilter_.matchFolder()); //if date filter is active we deactivate all folders: effectively gets rid of empty folders!

        recurse(dirObj);
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


void zen::addHardFiltering(BaseDirPair& baseDirObj, const Zstring& excludeFilter)
{
    ApplyHardFilter<STRATEGY_AND>::execute(baseDirObj, NameFilter(FilterConfig().includeFilter, excludeFilter));
}


void zen::addSoftFiltering(BaseDirPair& baseDirObj, const SoftFilter& timeSizeFilter)
{
    if (!timeSizeFilter.isNull()) //since we use STRATEGY_AND, we may skip a "null" filter
        ApplySoftFilter<STRATEGY_AND>::execute(baseDirObj, timeSizeFilter);
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
        BaseDirPair& baseDirectory = *folderCmp[it - allPairs.begin()];

        const NormalizedFilter normFilter = normalizeFilters(mainCfg.globalFilter, it->localFilter);

        //"set" hard filter
        ApplyHardFilter<STRATEGY_SET>::execute(baseDirectory, *normFilter.nameFilter);

        //"and" soft filter
        addSoftFiltering(baseDirectory, normFilter.timeSizeFilter);
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
        for (FilePair& fileObj : hierObj.refSubFiles())
            processFile(fileObj);
        for (SymlinkPair& linkObj : hierObj.refSubLinks())
            processLink(linkObj);
        for (DirPair& dirObj : hierObj.refSubDirs())
            processDir(dirObj);
    };

    void processFile(FilePair& fileObj) const
    {
        if (fileObj.isEmpty<LEFT_SIDE>())
            fileObj.setActive(matchTime<RIGHT_SIDE>(fileObj));
        else if (fileObj.isEmpty<RIGHT_SIDE>())
            fileObj.setActive(matchTime<LEFT_SIDE>(fileObj));
        else
            fileObj.setActive(matchTime<RIGHT_SIDE>(fileObj) ||
                              matchTime<LEFT_SIDE >(fileObj));
    }

    void processLink(SymlinkPair& linkObj) const
    {
        if (linkObj.isEmpty<LEFT_SIDE>())
            linkObj.setActive(matchTime<RIGHT_SIDE>(linkObj));
        else if (linkObj.isEmpty<RIGHT_SIDE>())
            linkObj.setActive(matchTime<LEFT_SIDE>(linkObj));
        else
            linkObj.setActive(matchTime<RIGHT_SIDE>(linkObj) ||
                              matchTime<LEFT_SIDE> (linkObj));
    }

    void processDir(DirPair& dirObj) const
    {
        dirObj.setActive(false);
        recurse(dirObj);
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
    std::for_each(begin(folderCmp), end(folderCmp), [&](BaseDirPair& baseDirObj) { FilterByTimeSpan::execute(baseDirObj, timeFrom, timeTo); });
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
            fileList += ABF::getDisplayPath(fsObj->getAbstractPath<LEFT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    for (const FileSystemObject* fsObj : selectionRight)
        if (!fsObj->isEmpty<RIGHT_SIDE>())
        {
            fileList += ABF::getDisplayPath(fsObj->getAbstractPath<RIGHT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    return std::make_pair(fileList, totalDelCount);
}


namespace
{
struct FSObjectLambdaVisitor : public FSObjectVisitor
{
    static void visit(FileSystemObject& fsObj,
                      const std::function<void(const DirPair&     dirObj )>& onDir,
                      const std::function<void(const FilePair&    fileObj)>& onFile,
                      const std::function<void(const SymlinkPair& linkObj)>& onSymlink)
    {
        FSObjectLambdaVisitor visitor(onDir, onFile, onSymlink);
        fsObj.accept(visitor);
    }

private:
    FSObjectLambdaVisitor(const std::function<void(const DirPair&     dirObj )>& onDir,
                          const std::function<void(const FilePair&    fileObj)>& onFile,
                          const std::function<void(const SymlinkPair& linkObj)>& onSymlink) : onDir_(onDir), onFile_(onFile), onSymlink_(onSymlink) {}

    void visit(const DirPair&     dirObj ) override { if (onDir_)     onDir_    (dirObj ); }
    void visit(const FilePair&    fileObj) override { if (onFile_)    onFile_   (fileObj); }
    void visit(const SymlinkPair& linkObj) override { if (onSymlink_) onSymlink_(linkObj); }

    const std::function<void(const DirPair&     dirObj )> onDir_;
    const std::function<void(const FilePair&    fileObj)> onFile_;
    const std::function<void(const SymlinkPair& linkObj)> onSymlink_;
};


template <SelectedSide side>
void copyToAlternateFolderFrom(const std::vector<FileSystemObject*>& rowsToCopy,
                               ABF& abfTarget,
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

    auto copyItem = [&](FileSystemObject& fsObj, const Zstring& relPath) //throw FileError
    {
        const AbstractPathRef targetPath = abfTarget.getAbstractPath(relPath);

        const std::function<void()> deleteTargetItem = [&]
        {
            if (overwriteIfExists)
                try
                {
                    //file or (broken) file-symlink:
                    ABF::removeFile(targetPath); //throw FileError
                }
                catch (FileError&)
                {
                    //folder or folder-symlink:
                    if (ABF::folderExists(targetPath)) //directory or dir-symlink
                        ABF::removeFolderRecursively(targetPath, nullptr /*onBeforeFileDeletion*/, nullptr /*onBeforeFolderDeletion*/); //throw FileError
                    else
                        throw;
                }
        };

        FSObjectLambdaVisitor::visit(fsObj,
                                     [&](const DirPair& dirObj)
        {
            StatisticsReporter statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingFolder, ABF::getDisplayPath(targetPath));

            try
            {
                //deleteTargetItem(); -> never delete pre-existing folders!!! => might delete child items we just copied!
                ABF::copyNewFolder(dirObj.getAbstractPath<side>(), targetPath, false /*copyFilePermissions*/); //throw FileError
            }
            catch (const FileError&) { if (!ABF::folderExists(targetPath)) throw; } //might already exist: see creation of intermediate directories below
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        },

        [&](const FilePair& fileObj)
        {
            StatisticsReporter statReporter(1, fileObj.getFileSize<side>(), callback);
            notifyItemCopy(txtCreatingFile, ABF::getDisplayPath(targetPath));

            auto onNotifyCopyStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };
            ABF::copyFileTransactional(fileObj.getAbstractPath<side>(), targetPath, //throw FileError, ErrorFileLocked
                                       false /*copyFilePermissions*/, true /*transactionalCopy*/, deleteTargetItem, onNotifyCopyStatus);
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        },

        [&](const SymlinkPair& linkObj)
        {
            StatisticsReporter statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingLink, ABF::getDisplayPath(targetPath));

            deleteTargetItem();
            ABF::copySymlink(linkObj.getAbstractPath<side>(), targetPath, false /*copyFilePermissions*/); //throw FileError
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        });
    };

    for (FileSystemObject* fsObj : rowsToCopy)
        tryReportingError([&]
    {
        const Zstring& relPath = keepRelPaths ? fsObj->getRelativePath<side>() : fsObj->getItemName<side>();
        try
        {
            copyItem(*fsObj, relPath); //throw FileError
        }
        catch (FileError&)
        {
            //create intermediate directories if missing
            const AbstractPathRef targetParentPath = abfTarget.getAbstractPath(beforeLast(relPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE));
            if (!ABF::somethingExists(targetParentPath)) //->(minor) file system race condition!
            {
                ABF::createFolderRecursively(targetParentPath); //throw FileError
                //retry: this should work now!
                copyItem(*fsObj, relPath); //throw FileError
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
        [&](const FilePair& fileObj) {dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<LEFT_SIDE>()); }, nullptr /*onSymlink*/);

    for (FileSystemObject* fsObj : itemSelectionRight)
        FSObjectLambdaVisitor::visit(*fsObj, nullptr /*onDir*/,
        [&](const FilePair& fileObj) {dataToProcess += static_cast<std::int64_t>(fileObj.getFileSize<RIGHT_SIDE>()); }, nullptr /*onSymlink*/);

    callback.initNewPhase(itemCount, dataToProcess, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    std::unique_ptr<ABF> abfTarget = createAbstractBaseFolder(targetFolderPathPhrase);

    copyToAlternateFolderFrom<LEFT_SIDE >(itemSelectionLeft,  *abfTarget, keepRelPaths, overwriteIfExists, callback);
    copyToAlternateFolderFrom<RIGHT_SIDE>(itemSelectionRight, *abfTarget, keepRelPaths, overwriteIfExists, callback);
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
            [&](const DirPair& dirObj)
            {
                if (useRecycleBin)
                {
                    notifyItemDeletion(txtRemovingDirectory, ABF::getDisplayPath(dirObj.getAbstractPath<side>()));
                    ABF::recycleItemDirectly(dirObj.getAbstractPath<side>()); //throw FileError
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

                    ABF::removeFolderRecursively(dirObj.getAbstractPath<side>(), onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
                }
            },

            [&](const FilePair& fileObj)
            {
                notifyItemDeletion(txtRemovingFile, ABF::getDisplayPath(fileObj.getAbstractPath<side>()));

                if (useRecycleBin)
                    ABF::recycleItemDirectly(fileObj.getAbstractPath<side>()); //throw FileError
                else
                    ABF::removeFile(fileObj.getAbstractPath<side>()); //throw FileError
                statReporter.reportDelta(1, 0);
            },

            [&](const SymlinkPair& linkObj)
            {
                notifyItemDeletion(txtRemovingSymlink, ABF::getDisplayPath(linkObj.getAbstractPath<side>()));

                if (useRecycleBin)
                    ABF::recycleItemDirectly(linkObj.getAbstractPath<side>()); //throw FileError
                else
                {
                    if (ABF::folderExists(linkObj.getAbstractPath<side>())) //dir symlink
                        ABF::removeFolderSimple(linkObj.getAbstractPath<side>()); //throw FileError
                    else //file symlink, broken symlink
                        ABF::removeFile(linkObj.getAbstractPath<side>()); //throw FileError
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
                std::map<const ABF*, bool, ABF::LessItemPath>& recyclerSupported,
                ProcessCallback& callback)
{
    auto hasRecycler = [&](const ABF& baseFolder) -> bool
    {
        auto it = recyclerSupported.find(&baseFolder); //perf: avoid duplicate checks!
        if (it != recyclerSupported.end())
            return it->second;

        const std::wstring msg = replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x",
        fmtPath(ABF::getDisplayPath(baseFolder.getAbstractPath())));

        bool recSupported = false;
        tryReportingError([&]{
            recSupported = baseFolder.supportsRecycleBin([&] { callback.reportStatus(msg); /*may throw*/ }); //throw FileError
        }, callback); //throw X?

        recyclerSupported.emplace(&baseFolder, recSupported);
        return recSupported;
    };

    for (FileSystemObject* row : rows)
        if (!row->isEmpty<side>())
        {
            if (useRecycleBin && hasRecycler(row->root().getABF<side>())) //Windows' ::SHFileOperation() will delete permanently anyway, but we have a superior deletion routine
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
    std::unordered_map<const BaseDirPair*, DirectionConfig> baseDirCfgs;
    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
        baseDirCfgs[&** it] = directCfgs[it - folderCmp.begin()];

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
                auto cfgIter = baseDirCfgs.find(&fsObj.root());
                assert(cfgIter != baseDirCfgs.end());
                if (cfgIter != baseDirCfgs.end())
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
        std::for_each(begin(folderCmp), end(folderCmp), BaseDirPair::removeEmpty);
    };
    ZEN_ON_SCOPE_EXIT(updateDirection()); //MSVC: assert is a macro and it doesn't play nice with ZEN_ON_SCOPE_EXIT, surprise... wasn't there something about macros being "evil"?

    //categorize rows into permanent deletion and recycle bin
    std::vector<FileSystemObject*> deletePermanentLeft;
    std::vector<FileSystemObject*> deletePermanentRight;
    std::vector<FileSystemObject*> deleteRecylerLeft;
    std::vector<FileSystemObject*> deleteRecylerRight;

    std::map<const ABF*, bool, ABF::LessItemPath> recyclerSupported;
    categorize<LEFT_SIDE >(deleteLeft,  deletePermanentLeft,  deleteRecylerLeft,  useRecycleBin, recyclerSupported, callback);
    categorize<RIGHT_SIDE>(deleteRight, deletePermanentRight, deleteRecylerRight, useRecycleBin, recyclerSupported, callback);

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    if (useRecycleBin &&
    std::any_of(recyclerSupported.begin(), recyclerSupported.end(), [](const decltype(recyclerSupported)::value_type& item) { return !item.second; }))
    {
        std::wstring msg = _("The recycle bin is not available for the following folders. Files will be deleted permanently instead:") + L"\n";

        for (const auto& item : recyclerSupported)
            if (!item.second)
                msg += L"\n" + ABF::getDisplayPath(item.first->getAbstractPath());

        callback.reportWarning(msg, warningRecyclerMissing); //throw?
    }

    deleteFromGridAndHDOneSide<LEFT_SIDE>(deleteRecylerLeft,   true,  callback);
    deleteFromGridAndHDOneSide<LEFT_SIDE>(deletePermanentLeft, false, callback);

    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deleteRecylerRight,   true,  callback);
    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deletePermanentRight, false, callback);
}
