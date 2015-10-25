// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DIR_EXIST_HEADER_08173281673432158067342132467183267
#define DIR_EXIST_HEADER_08173281673432158067342132467183267

#include <zen/thread.h>
#include <zen/file_access.h>
#include <zen/file_error.h>
#include "../process_callback.h"
#include "resolve_path.h"

namespace zen
{
namespace
{
//directory existence checking may hang for non-existent network drives => run asynchronously and update UI!
//- check existence of all directories in parallel! (avoid adding up search times if multiple network drives are not reachable)
//- add reasonable time-out time!
//- avoid checking duplicate entries by design: set<Zstring, LessFilename>
struct DirectoryStatus
{
    std::set<Zstring, LessFilename> existing;
    std::set<Zstring, LessFilename> missing;

};

DirectoryStatus getExistingDirsUpdating(const std::set<Zstring, LessFilename>& dirpaths,
                                        bool allowUserInteraction,
                                        ProcessCallback& procCallback)
{
    using namespace zen;

    DirectoryStatus output;

    std::list<std::pair<Zstring, boost::unique_future<bool>>> futureInfo;
    for (const Zstring& dirpath : dirpaths)
        if (!dirpath.empty()) //skip empty dirs
            futureInfo.emplace_back(dirpath, async([=]() -> bool
        {
#ifdef ZEN_WIN
            //1. login to network share, if necessary
            loginNetworkShare(dirpath, allowUserInteraction);
#endif
            //2. check dir existence
            return dirExists(dirpath);
        }));

    //don't wait (almost) endlessly like win32 would on not existing network shares:
    const boost::system_time endTime = boost::get_system_time() + boost::posix_time::seconds(20); //consider CD-rom insert or hard disk spin up time from sleep

    for (auto& fi : futureInfo)
    {
        procCallback.reportStatus(replaceCpy(_("Searching for folder %x..."), L"%x", fmtFileName(fi.first), false)); //may throw!

        while (boost::get_system_time() < endTime &&
               !fi.second.timed_wait(boost::posix_time::milliseconds(UI_UPDATE_INTERVAL / 2)))
            procCallback.requestUiRefresh(); //may throw!

        if (fi.second.is_ready() && fi.second.get())
            output.existing.insert(fi.first);
        else
            output.missing.insert(fi.first);
    }
    return output;
}
}

inline //also silences Clang "unused function" for compilation units depending from getExistingDirsUpdating() only
bool dirExistsUpdating(const Zstring& dirpath, bool allowUserInteraction, ProcessCallback& procCallback)
{
    if (dirpath.empty()) return false;
    const DirectoryStatus dirStatus = getExistingDirsUpdating({ dirpath }, allowUserInteraction, procCallback);
    assert(dirStatus.existing.empty() != dirStatus.missing.empty());
    return dirStatus.existing.find(dirpath) != dirStatus.existing.end();
}
}

#endif //DIR_EXIST_HEADER_08173281673432158067342132467183267
