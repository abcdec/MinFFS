// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FOLDER_TRAVERSER_H_INCLUDED_3127463214871234
#define FOLDER_TRAVERSER_H_INCLUDED_3127463214871234

#include <cstdint>
#include <functional>
#include "zstring.h"

namespace zen
{
struct FileInfo
{
    const Zchar* shortName;
    const Zstring& fullPath;
    std::uint64_t fileSize;     //[bytes]
    std::int64_t lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
};

struct DirInfo
{
    const Zchar* shortName;
    const Zstring& fullPath;
};

struct SymlinkInfo
{
    const Zchar* shortName;
    const Zstring& fullPath;
    std::int64_t lastWriteTime; //number of seconds since Jan. 1st 1970 UTC
};

//- non-recursive
//- directory path may end with PATH_SEPARATOR
void traverseFolder(const Zstring& dirPath, //noexcept
                    const std::function<void (const FileInfo&    fi)>& onFile,          //
                    const std::function<void (const DirInfo&     di)>& onDir,           //optional
                    const std::function<void (const SymlinkInfo& si)>& onLink,          //
                    const std::function<void (const std::wstring& errorMsg)>& onError); //
}

#endif //FOLDER_TRAVERSER_H_INCLUDED_3127463214871234
