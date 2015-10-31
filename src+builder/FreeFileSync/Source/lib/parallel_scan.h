// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PARALLEL_SCAN_H_INCLUDED
#define PARALLEL_SCAN_H_INCLUDED

#include <map>
#include <set>
#include "hard_filter.h"
#include "../structures.h"
#include "../file_hierarchy.h"


namespace zen
{
struct DirectoryKey
{
    DirectoryKey(const ABF& baseFolder,
                 const HardFilter::FilterRef& filter,
                 SymLinkHandling handleSymlinks) :
        baseFolder_(&baseFolder),
        filter_(filter),
        handleSymlinks_(handleSymlinks) {}

    const ABF* baseFolder_; //always bound!
    HardFilter::FilterRef filter_; //filter interface: always bound by design!
    SymLinkHandling handleSymlinks_;
};

inline
bool operator<(const DirectoryKey& lhs, const DirectoryKey& rhs)
{
    if (lhs.handleSymlinks_ != rhs.handleSymlinks_)
        return lhs.handleSymlinks_ < rhs.handleSymlinks_;

    if (ABF::LessItemPath()(lhs.baseFolder_, rhs.baseFolder_))
        return true;
    if (ABF::LessItemPath()(rhs.baseFolder_, lhs.baseFolder_))
        return false;

    return *lhs.filter_ < *rhs.filter_;
}


struct DirectoryValue
{
    DirContainer dirCont;
    //relative names (or empty string for root) for directories that could not be read (completely), e.g. access denied, or temporal network drop
    std::map<Zstring, std::wstring, LessFilePath> failedDirReads; //with corresponding error message

    //relative names (never empty) for failure to read single file/dir/symlink with corresponding error message
    std::map<Zstring, std::wstring, LessFilePath> failedItemReads;
};


class FillBufferCallback
{
public:
    virtual ~FillBufferCallback() {}

    enum HandleError
    {
        ON_ERROR_RETRY,
        ON_ERROR_IGNORE
    };
    virtual HandleError reportError (const std::wstring& msg, size_t retryNumber) = 0; //may throw!
    virtual void        reportStatus(const std::wstring& msg, int    itemsTotal ) = 0; //
};

//attention: ensure directory filtering is applied later to exclude filtered directories which have been kept as parent folders

void fillBuffer(const std::set<DirectoryKey>& keysToRead, //in
                std::map<DirectoryKey, DirectoryValue>& buf, //out
                FillBufferCallback& callback,
                size_t updateIntervalMs); //unit: [ms]
}

#endif // PARALLEL_SCAN_H_INCLUDED
