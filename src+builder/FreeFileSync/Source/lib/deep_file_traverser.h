// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DEEP_FILE_TRAVERSER_H_INCLUDED_342765897342151
#define DEEP_FILE_TRAVERSER_H_INCLUDED_342765897342151

#include <cstdint>
#include <zen/zstring.h>
#include <zen/file_id_def.h>

//advanced file traverser returning metadata and hierarchical information on files and directories

namespace zen
{
struct TraverseCallback
{
    virtual ~TraverseCallback() {}

    struct SymlinkInfo
    {
        const Zchar*   shortName;
        const Zstring& fullPath;
        std::int64_t   lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
    };

    struct FileInfo
    {
        const Zchar*   shortName;
        const Zstring& fullPath;
        std::uint64_t  fileSize;      //unit: bytes!
        std::int64_t   lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
        const FileId&  id;            //optional: initial if not supported!
        const SymlinkInfo* symlinkInfo; //only filled if file is a followed symlink
    };

    struct DirInfo
    {
        const Zchar*   shortName;
        const Zstring& fullPath;
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

    virtual void              onFile   (const FileInfo&    fi) = 0;
    virtual TraverseCallback* onDir    (const DirInfo&     di) = 0;
    virtual HandleLink        onSymlink(const SymlinkInfo& li) = 0;
    //nullptr: ignore directory, non-nullptr: traverse into using the (new) callback => implement releaseDirTraverser() if necessary!
    virtual void releaseDirTraverser(TraverseCallback* trav) {}

    virtual HandleError reportDirError (const std::wstring& msg, size_t retryNumber) = 0; //failed directory traversal -> consider directory data at current level as incomplete!
    virtual HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zchar* shortName) = 0; //failed to get data for single file/dir/symlink only!
};

//custom traverser with detail information about files
//- client needs to handle duplicate file reports! (FilePlusTraverser fallback, retrying to read directory contents, ...)
//- directory may end with PATH_SEPARATOR
void deepTraverseFolder(const Zstring& dirpath, TraverseCallback& sink); //noexcept
}

#endif //DEEP_FILE_TRAVERSER_H_INCLUDED_342765897342151
