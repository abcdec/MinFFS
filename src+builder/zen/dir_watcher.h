// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DIR_WATCHER_348577025748023458
#define DIR_WATCHER_348577025748023458

#include <vector>
#include <memory>
#include <functional>
#include "file_error.h"

namespace zen
{
//Windows: ReadDirectoryChangesW http://msdn.microsoft.com/en-us/library/aa365465(v=vs.85).aspx
//Linux:   inotify               http://linux.die.net/man/7/inotify
//OS X:    kqueue                http://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/kqueue.2.html

//watch directory including subdirectories
/*
!Note handling of directories!:
	Windows: removal of top watched directory is NOT notified (e.g. brute force usb stick removal)
	         however manual unmount IS notified (e.g. usb stick removal, then re-insert), but watching is stopped!
			 Renaming of top watched directory handled incorrectly: Not notified(!) + additional changes in subfolders
			 now do report FILE_ACTION_MODIFIED for directory (check that should prevent this fails!)

    Linux: newly added subdirectories are reported but not automatically added for watching! -> reset Dirwatcher!
	       removal of top watched directory is NOT notified!

	OS X: everything works as expected; renaming of top level folder is also detected

	Overcome all issues portably: check existence of top watched directory externally + reinstall watch after changes in directory structure (added directories) are detected
*/
class DirWatcher
{
public:
    DirWatcher(const Zstring& directory); //throw FileError
    ~DirWatcher();

    enum ActionType
    {
        ACTION_CREATE, //informal only!
        ACTION_UPDATE, //use for debugging/logging only!
        ACTION_DELETE, //
    };

    struct Entry
    {
        Entry() : action_(ACTION_CREATE) {}
        Entry(ActionType action, const Zstring& filepath) : action_(action), filepath_(filepath) {}

        ActionType action_;
        Zstring filepath_;
    };

    //extract accumulated changes since last call
    std::vector<Entry> getChanges(const std::function<void()>& processGuiMessages); //throw FileError

private:
    DirWatcher           (const DirWatcher&) = delete;
    DirWatcher& operator=(const DirWatcher&) = delete;

    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl_;
};

}

#endif
