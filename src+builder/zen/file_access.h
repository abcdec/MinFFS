// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILE_ACCESS_H_8017341345614857
#define FILE_ACCESS_H_8017341345614857

#include <functional>
#include "zstring.h"
#include "file_error.h"
#include "file_id_def.h"


namespace zen
{
bool fileExists     (const Zstring& filePath); //noexcept; check whether file      or file-symlink exists
bool dirExists      (const Zstring& dirPath ); //noexcept; check whether directory or dir-symlink exists
bool symlinkExists  (const Zstring& linkPath); //noexcept; check whether a symbolic link exists
bool somethingExists(const Zstring& itemPath); //noexcept; check whether any object with this name exists

enum class ProcSymlink
{
    DIRECT,
    FOLLOW
};

void setFileTime(const Zstring& filePath, std::int64_t modificationTime, ProcSymlink procSl); //throw FileError

//symlink handling: always evaluate target
std::uint64_t getFilesize(const Zstring& filePath); //throw FileError
std::uint64_t getFreeDiskSpace(const Zstring& path); //throw FileError, returns 0 if not available

bool removeFile(const Zstring& filePath); //throw FileError; return "false" if file is not existing

void removeDirectorySimple(const Zstring& dirPath); //throw FileError

void removeDirectoryRecursively(const Zstring& dirPath); //throw FileError

//rename file or directory: no copying!!!
void renameFile(const Zstring& itemPathOld, const Zstring& itemPathNew); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting

bool supportsPermissions(const Zstring& dirpath); //throw FileError, dereferences symlinks

//- no error if already existing
//- create recursively if parent directory is not existing
void makeDirectoryRecursively(const Zstring& dirpath); //throw FileError

//fail if already existing or parent directory not existing:
//source path is optional (may be empty)
void copyNewDirectory(const Zstring& sourcePath, const Zstring& targetPath, bool copyFilePermissions); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing

void copySymlink(const Zstring& sourceLink, const Zstring& targetLink, bool copyFilePermissions); //throw FileError

struct InSyncAttributes
{
    std::uint64_t fileSize;
    std::int64_t modificationTime; //time_t UTC compatible
    FileId sourceFileId;
    FileId targetFileId;
};

InSyncAttributes copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                             //accummulated delta != file size! consider ADS, sparse, compressed files
                             const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus); //may be nullptr; throw X!
}

#endif //FILE_ACCESS_H_8017341345614857
