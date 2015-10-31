// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DIR_EXIST_HEADER_08173281673432158067342132467183267
#define DIR_EXIST_HEADER_08173281673432158067342132467183267

#include <list>
#include <zen/thread.h>
#include <zen/file_error.h>
#include "../fs/abstract.h"
#include "../process_callback.h"


namespace zen
{
namespace
{
//directory existence checking may hang for non-existent network drives => run asynchronously and update UI!
//- check existence of all directories in parallel! (avoid adding up search times if multiple network drives are not reachable)
//- add reasonable time-out time!
//- avoid checking duplicate entries by design: std::set
struct DirectoryStatus
{
    std::set<const ABF*, ABF::LessItemPath> existingBaseFolder;
    std::set<const ABF*, ABF::LessItemPath> missingBaseFolder;
    std::map<const ABF*, FileError, ABF::LessItemPath> failedChecks;
};


DirectoryStatus checkFolderExistenceUpdating(const std::set<const ABF*, ABF::LessItemPath>& baseFolders, bool allowUserInteraction, ProcessCallback& procCallback)
{
    using namespace zen;

    DirectoryStatus output;

    std::list<std::pair<const ABF*, std::future<bool>>> futureInfo;

    for (const ABF* baseFolder : baseFolders)
        if (!baseFolder->emptyBaseFolderPath()) //skip empty dirs
        {
            AbstractPathRef folderPath = baseFolder->getAbstractPath();

            std::function<void()> connectFolder /*throw FileError*/ = baseFolder->getAsyncConnectFolder(allowUserInteraction); //noexcept
            std::function<bool()> dirExists     /*throw FileError*/ = ABF::getAsyncCheckFolderExists(folderPath); //noexcept

            futureInfo.emplace_back(baseFolder, runAsync([connectFolder, dirExists]
            {
                //1. login to network share, open FTP connection, ect.
                if (connectFolder)
                    connectFolder(); //throw FileError

                //2. check dir existence
                return dirExists(); //throw FileError
            }));
        }

    //don't wait (almost) endlessly like win32 would on non-existing network shares:
    std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now() + std::chrono::seconds(20); //consider CD-rom insert or hard disk spin up time from sleep

    for (auto& fi : futureInfo)
    {
        const std::wstring& displayPathFmt = fmtPath(ABF::getDisplayPath(fi.first->getAbstractPath()));

        procCallback.reportStatus(replaceCpy(_("Searching for folder %x..."), L"%x", displayPathFmt)); //may throw!

        while (std::chrono::steady_clock::now() < endTime &&
               fi.second.wait_for(std::chrono::milliseconds(UI_UPDATE_INTERVAL / 2)) != std::future_status::ready)
            procCallback.requestUiRefresh(); //may throw!

        if (isReady(fi.second))
        {
            try
            {
                //call future::get() only *once*! otherwise: undefined behavior!
                if (fi.second.get()) //throw FileError
                    output.existingBaseFolder.insert(fi.first);
                else
                    output.missingBaseFolder.insert(fi.first);
            }
            catch (const FileError& e) { output.failedChecks.emplace(fi.first, e); }
        }
        else
            output.failedChecks.emplace(fi.first, FileError(replaceCpy(_("Time out while searching for folder %x."), L"%x", displayPathFmt)));
    }

    return output;
}
}

inline //also silences Clang "unused function" for compilation units depending from getExistingDirsUpdating() only
bool folderExistsUpdating(const ABF& baseFolder, bool allowUserInteraction, ProcessCallback& procCallback)
{
    std::set<const ABF*, ABF::LessItemPath> baseFolders{ &baseFolder };
    const DirectoryStatus status = checkFolderExistenceUpdating(baseFolders, allowUserInteraction, procCallback);
    return status.existingBaseFolder.find(&baseFolder) != status.existingBaseFolder.end();
}
}

#endif //DIR_EXIST_HEADER_08173281673432158067342132467183267
