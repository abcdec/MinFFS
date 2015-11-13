// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SYNCHRONIZATION_H_8913470815943295
#define SYNCHRONIZATION_H_8913470815943295

#include <zen/time.h>
#include "file_hierarchy.h"
#include "lib/process_xml.h"
#include "process_callback.h"


namespace zen
{
class SyncStatistics //this class counts *logical* operations, (create, update, delete + bytes), *not* disk accesses!
{
    //-> note the fundamental difference compared to counting disk accesses!
public:
    SyncStatistics(const FolderComparison& folderCmp);
    SyncStatistics(const HierarchyObject& hierObj);
    SyncStatistics(const FilePair& file);

    template <SelectedSide side>
    int createCount() const { return SelectParam<side>::ref(createLeft, createRight); }
    int createCount() const { return createLeft + createRight; }

    template <SelectedSide side>
    int updateCount() const { return SelectParam<side>::ref(updateLeft, updateRight); }
    int updateCount() const { return updateLeft + updateRight; }

    template <SelectedSide side>
    int deleteCount() const { return SelectParam<side>::ref(deleteLeft, deleteRight); }
    int deleteCount() const { return deleteLeft + deleteRight; }

    int conflictCount() const { return static_cast<int>(conflictMsgs.size()); }

    std::int64_t getDataToProcess() const { return dataToProcess; }
    size_t       rowCount        () const { return rowsTotal; }

    using ConflictInfo = std::pair<Zstring, std::wstring>; //pair(filePath/conflict message)
    const std::vector<ConflictInfo>& getConflicts() const { return conflictMsgs; }

private:
    void recurse(const HierarchyObject& hierObj);

    void processFile(const FilePair& file);
    void processLink(const SymlinkPair& link);
    void processFolder(const FolderPair& folder);

    int createLeft  = 0;
    int createRight = 0;
    int updateLeft  = 0;
    int updateRight = 0;
    int deleteLeft  = 0;
    int deleteRight = 0;
    std::vector<ConflictInfo> conflictMsgs; //conflict texts to display as a warning message
    std::int64_t dataToProcess = 0;
    size_t rowsTotal = 0;
};


struct FolderPairSyncCfg
{
    FolderPairSyncCfg(bool saveSyncDB,
                      const DeletionPolicy handleDel,
                      VersioningStyle versioningStyle,
                      const Zstring& versioningPhrase,
                      DirectionConfig::Variant syncVariant) :
        saveSyncDB_(saveSyncDB),
        handleDeletion(handleDel),
        versioningStyle_(versioningStyle),
        versioningFolderPhrase(versioningPhrase),
        syncVariant_(syncVariant) {}

    bool saveSyncDB_; //save database if in automatic mode or dection of moved files is active
    DeletionPolicy handleDeletion;
    VersioningStyle versioningStyle_;
    Zstring versioningFolderPhrase; //unresolved directory names as entered by user!
    DirectionConfig::Variant syncVariant_;
};
std::vector<FolderPairSyncCfg> extractSyncCfg(const MainConfiguration& mainCfg);


//FFS core routine:
void synchronize(const TimeComp& timeStamp,
                 xmlAccess::OptionalDialogs& warnings,
                 bool verifyCopiedFiles,
                 bool copyLockedFiles,
                 bool copyFilePermissions,
                 bool transactionalFileCopy,
                 bool runWithBackgroundPriority,
                 const std::vector<FolderPairSyncCfg>& syncConfig, //CONTRACT: syncConfig and folderCmp correspond row-wise!
                 FolderComparison& folderCmp,                      //
                 ProcessCallback& callback);
}

#endif //SYNCHRONIZATION_H_8913470815943295
