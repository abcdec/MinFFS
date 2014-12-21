// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "monitor.h"
#include <ctime>
#include <set>
#include <zen/file_access.h>
#include <zen/dir_watcher.h>
#include <zen/thread.h>
#include <zen/tick_count.h>
#include <wx/utils.h>
#include "../lib/resolve_path.h"
//#include "../library/db_file.h"     //SYNC_DB_FILE_ENDING -> complete file too much of a dependency; file ending too little to decouple into single header
//#include "../library/lock_holder.h" //LOCK_FILE_ENDING

using namespace zen;


namespace
{
const int CHECK_DIR_INTERVAL = 1; //unit: [s]


std::vector<Zstring> getFormattedDirs(const std::vector<Zstring>& dirpathPhrases) //throw FileError
{
    std::set<Zstring, LessFilename> dirpaths; //make unique
    for (const Zstring& phrase : std::set<Zstring, LessFilename>(dirpathPhrases.begin(), dirpathPhrases.end()))
        //make unique: no need to resolve duplicate phrases more than once! (consider "[volume name]" syntax) -> shouldn't this be already buffered by OS?
        dirpaths.insert(getFormattedDirectoryPath(phrase));

    return std::vector<Zstring>(dirpaths.begin(), dirpaths.end());
}


//wait until changes are detected or if a directory is not available (anymore)
struct WaitResult
{
    enum ChangeType
    {
        CHANGE_DETECTED,
        CHANGE_DIR_MISSING
    };

    WaitResult(const zen::DirWatcher::Entry& changedItem) : type(CHANGE_DETECTED), changedItem_(changedItem) {}
    WaitResult(const Zstring& dirpath) : type(CHANGE_DIR_MISSING), dirpath_(dirpath) {}

    ChangeType type;
    zen::DirWatcher::Entry changedItem_; //for type == CHANGE_DETECTED: file or directory
    Zstring dirpath_;                    //for type == CHANGE_DIR_MISSING
};


WaitResult waitForChanges(const std::vector<Zstring>& dirpathPhrases, //throw FileError
                          const std::function<void(bool readyForSync)>& onRefreshGui)
{
    const std::vector<Zstring> dirpathsFmt = getFormattedDirs(dirpathPhrases); //throw FileError
    if (dirpathsFmt.empty()) //pathological case, but we have to check else this function will wait endlessly
        throw zen::FileError(_("A folder input field is empty.")); //should have been checked by caller!

    //detect when volumes are removed/are not available anymore
    std::vector<std::pair<Zstring, std::shared_ptr<DirWatcher>>> watches;

    for (const Zstring& dirpathFmt : dirpathsFmt)
    {
        try
        {
            //a non-existent network path may block, so check existence asynchronously!
            auto ftDirExists = async([=] { return zen::dirExists(dirpathFmt); });
            //we need to check dirExists(), not somethingExists(): it's not clear if DirWatcher detects a type clash (file instead of directory!)
            while (!ftDirExists.timed_wait(boost::posix_time::milliseconds(rts::UI_UPDATE_INTERVAL / 2)))
                onRefreshGui(false); //may throw!
            if (!ftDirExists.get())
                return WaitResult(dirpathFmt);

            watches.emplace_back(dirpathFmt, std::make_shared<DirWatcher>(dirpathFmt)); //throw FileError
        }
        catch (FileError&)
        {
            if (!somethingExists(dirpathFmt)) //a benign(?) race condition with FileError
                return WaitResult(dirpathFmt);
            throw;
        }
    }

    const std::int64_t TICKS_DIR_CHECK_INTERVAL = CHECK_DIR_INTERVAL * ticksPerSec(); //0 on error
    TickVal lastCheck = getTicks(); //0 on error
    while (true)
    {
        const bool checkDirExistNow = [&]() -> bool //checking once per sec should suffice
        {
            const TickVal now = getTicks(); //0 on error
            if (dist(lastCheck, now) >= TICKS_DIR_CHECK_INTERVAL)
            {
                lastCheck = now;
                return true;
            }
            return false;
        }();


        for (auto it = watches.begin(); it != watches.end(); ++it)
        {
            const Zstring& dirpath = it->first;
            DirWatcher& watcher = *(it->second);

            //IMPORTANT CHECK: dirwatcher has problems detecting removal of top watched directories!
            if (checkDirExistNow)
                if (!dirExists(dirpath)) //catch errors related to directory removal, e.g. ERROR_NETNAME_DELETED -> somethingExists() is NOT sufficient here!
                    return WaitResult(dirpath);
            try
            {
                std::vector<DirWatcher::Entry> changedItems = watcher.getChanges([&] { onRefreshGui(false); /*may throw!*/ }); //throw FileError

                //remove to be ignored changes
                vector_remove_if(changedItems, [](const DirWatcher::Entry& e)
                {
                    return
#ifdef ZEN_MAC
                        endsWith(e.filepath_, Zstr("/.DS_Store")) ||
#endif
                        endsWith(e.filepath_, TEMP_FILE_ENDING)  ||
                        endsWith(e.filepath_, Zstr(".ffs_lock")) || //sync.ffs_lock, sync.Del.ffs_lock
                        endsWith(e.filepath_, Zstr(".ffs_db"));     //sync.ffs_db, .sync.tmp.ffs_db
                    //no need to ignore temporal recycle bin directory: this must be caused by a file deletion anyway
                });

                if (!changedItems.empty())
                    return WaitResult(changedItems[0]); //directory change detected
            }
            catch (FileError&)
            {
                if (!somethingExists(dirpath)) //a benign(?) race condition with FileError
                    return WaitResult(dirpath);
                throw;
            }
        }

        boost::this_thread::sleep(boost::posix_time::milliseconds(rts::UI_UPDATE_INTERVAL / 2));
        onRefreshGui(true); //throw ?: may start sync at this presumably idle time
    }
}


//wait until all directories become available (again) + logs in network share
void waitForMissingDirs(const std::vector<Zstring>& dirpathPhrases, //throw FileError
                        const std::function<void(const Zstring& dirpath)>& onRefreshGui)
{
    while (true)
    {
        bool allExisting = true;
        //support specifying volume by name => call getFormattedDirectoryPath() repeatedly
        for (const Zstring& dirpathFmt : getFormattedDirs(dirpathPhrases)) //throw FileError
        {
            auto ftDirExisting = async([=]() -> bool
            {
#ifdef ZEN_WIN
                //1. login to network share, if necessary -> we probably do NOT want multiple concurrent runs: GUI!?
                loginNetworkShare(dirpathFmt, false); //login networks shares, no PW prompt -> is this really RTS's job?
#endif
                //2. check dir existence
                return zen::dirExists(dirpathFmt);
            });
            while (!ftDirExisting.timed_wait(boost::posix_time::milliseconds(rts::UI_UPDATE_INTERVAL / 2)))
                onRefreshGui(dirpathFmt); //may throw!

            if (!ftDirExisting.get())
            {
                allExisting = false;
                //wait some time...
                const int refreshInterval = rts::UI_UPDATE_INTERVAL / 2;
                static_assert(CHECK_DIR_INTERVAL * 1000 % refreshInterval == 0, "");
                for (int i = 0; i < CHECK_DIR_INTERVAL * 1000 / refreshInterval; ++i)
                {
                    onRefreshGui(dirpathFmt); //may throw!
                    boost::this_thread::sleep(boost::posix_time::milliseconds(refreshInterval));
                }
                break;
            }
        }
        if (allExisting)
            return;
    }
}


inline
wxString toString(DirWatcher::ActionType type)
{
    switch (type)
    {
        case DirWatcher::ACTION_CREATE:
            return L"CREATE";
        case DirWatcher::ACTION_UPDATE:
            return L"UPDATE";
        case DirWatcher::ACTION_DELETE:
            return L"DELETE";
    }
    return L"ERROR";
}

struct ExecCommandNowException {};
}


void rts::monitorDirectories(const std::vector<Zstring>& dirpathPhrases, unsigned int delay, rts::MonitorCallback& callback)
{
    if (dirpathPhrases.empty())
    {
        assert(false);
        return;
    }

    auto execMonitoring = [&] //throw FileError
    {
        callback.setPhase(MonitorCallback::MONITOR_PHASE_WAITING);
        waitForMissingDirs(dirpathPhrases, [&](const Zstring& dirpath) { callback.requestUiRefresh(); }); //throw FileError
        callback.setPhase(MonitorCallback::MONITOR_PHASE_ACTIVE);

        //schedule initial execution (*after* all directories have arrived, which could take some time which we don't want to include)
        time_t nextExecDate = std::time(nullptr) + delay;

        while (true) //loop over command invocations
        {
            DirWatcher::Entry lastChangeDetected;
            try
            {
                while (true) //loop over detected changes
                {
                    //wait for changes (and for all directories to become available)
                    WaitResult res = waitForChanges(dirpathPhrases, [&](bool readyForSync) //throw FileError, ExecCommandNowException
                    {
                        if (readyForSync)
                            if (nextExecDate <= std::time(nullptr))
                                throw ExecCommandNowException(); //abort wait and start sync
                        callback.requestUiRefresh();
                    });
                    switch (res.type)
                    {
                        case WaitResult::CHANGE_DIR_MISSING: //don't execute the command before all directories are available!
                            callback.setPhase(MonitorCallback::MONITOR_PHASE_WAITING);
                            waitForMissingDirs(dirpathPhrases, [&](const Zstring& dirpath) { callback.requestUiRefresh(); }); //throw FileError
                            callback.setPhase(MonitorCallback::MONITOR_PHASE_ACTIVE);
                            break;

                        case WaitResult::CHANGE_DETECTED:
                            lastChangeDetected = res.changedItem_;
                            break;
                    }
                    nextExecDate = std::time(nullptr) + delay;
                }
            }
            catch (ExecCommandNowException&) {}

            ::wxSetEnv(L"change_path", utfCvrtTo<wxString>(lastChangeDetected.filepath_)); //some way to output what file changed to the user
            ::wxSetEnv(L"change_action", toString(lastChangeDetected.action_)); //

            //execute command
            callback.executeExternalCommand();
            nextExecDate = std::numeric_limits<time_t>::max();
        }
    };

    while (true)
        try
        {
            execMonitoring(); //throw FileError
        }
        catch (const zen::FileError& e)
        {
            callback.reportError(e.toString());
        }
}
