// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DB_FILE_H_834275398588021574
#define DB_FILE_H_834275398588021574

#include <zen/file_error.h>
#include "../file_hierarchy.h"


namespace zen
{
const Zchar SYNC_DB_FILE_ENDING[] = Zstr(".ffs_db"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

struct InSyncDescrFile //subset of FileDescriptor
{
    InSyncDescrFile(std::int64_t lastWriteTimeRawIn, const AFS::FileId& idIn) :
        lastWriteTimeRaw(lastWriteTimeRawIn),
        fileId(idIn) {}

    std::int64_t lastWriteTimeRaw;
    AFS::FileId fileId; // == file id: optional! (however, always set on Linux, and *generally* available on Windows)
};

struct InSyncDescrLink
{
    explicit InSyncDescrLink(std::int64_t lastWriteTimeRawIn) : lastWriteTimeRaw(lastWriteTimeRawIn) {}

    std::int64_t lastWriteTimeRaw;
};


//artificial hierarchy of last synchronous state:
struct InSyncFile
{
    InSyncFile(const InSyncDescrFile& l, const InSyncDescrFile& r, CompareVariant cv, std::uint64_t fileSizeIn) : left(l), right(r), cmpVar(cv), fileSize(fileSizeIn) {}
    InSyncDescrFile left;
    InSyncDescrFile right;
    CompareVariant cmpVar; //the one active while finding "file in sync"
    std::uint64_t fileSize; //file size must be identical on both sides!
};

struct InSyncSymlink
{
    InSyncSymlink(const InSyncDescrLink& l, const InSyncDescrLink& r, CompareVariant cv) : left(l), right(r), cmpVar(cv) {}
    InSyncDescrLink left;
    InSyncDescrLink right;
    CompareVariant cmpVar;
};

struct InSyncFolder
{
    //for directories we have a logical problem: we cannot have "not existent" as an indicator for
    //"no last synchronous state" since this precludes child elements that may be in sync!
    enum InSyncStatus
    {
        DIR_STATUS_IN_SYNC,
        DIR_STATUS_STRAW_MAN //there is no last synchronous state, but used as container only
    };
    InSyncFolder(InSyncStatus statusIn) : status(statusIn) {}

    InSyncStatus status;

    //------------------------------------------------------------------
    typedef std::map<Zstring, InSyncFolder,  LessFilePath> FolderList;  //
    typedef std::map<Zstring, InSyncFile,    LessFilePath> FileList;    // key: file name
    typedef std::map<Zstring, InSyncSymlink, LessFilePath> SymlinkList; //
    //------------------------------------------------------------------

    FolderList  folders;
    FileList    files;
    SymlinkList symlinks; //non-followed symlinks

    //convenience
    InSyncFolder& addFolder(const Zstring& shortName, InSyncStatus st)
    {
        return folders.emplace(shortName, InSyncFolder(st)).first->second;
    }

    void addFile(const Zstring& shortName, const InSyncDescrFile& dataL, const InSyncDescrFile& dataR, CompareVariant cmpVar, std::uint64_t fileSize)
    {
        files.emplace(shortName, InSyncFile(dataL, dataR, cmpVar, fileSize));
    }

    void addSymlink(const Zstring& shortName, const InSyncDescrLink& dataL, const InSyncDescrLink& dataR, CompareVariant cmpVar)
    {
        symlinks.emplace(shortName, InSyncSymlink(dataL, dataR, cmpVar));
    }
};


DEFINE_NEW_FILE_ERROR(FileErrorDatabaseNotExisting);

std::shared_ptr<InSyncFolder> loadLastSynchronousState(const BaseFolderPair& baseDirObj, //throw FileError, FileErrorDatabaseNotExisting -> return value always bound!
                                                       const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus);

void saveLastSynchronousState(const BaseFolderPair& baseDirObj, //throw FileError
                              const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus);
}

#endif //DB_FILE_H_834275398588021574
