// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
#ifndef DIR_LOCK_H_INCLUDED
#define DIR_LOCK_H_INCLUDED

#include <memory>
#include <zen/file_error.h>

namespace zen
{
const size_t GUI_CALLBACK_INTERVAL = 100;

struct DirLockCallback //while waiting for the lock
{
    virtual ~DirLockCallback() {}
    virtual void requestUiRefresh() = 0;  //allowed to throw exceptions
    virtual void reportStatus(const std::wstring& text) = 0;
};

/*
RAII structure to place a directory lock against other FFS processes:
        - recursive locking supported, even with alternate lockfile names, e.g. via symlinks, network mounts etc.
        - ownership shared between all object instances refering to a specific lock location(= GUID)
        - can be copied safely and efficiently! (ref-counting)
        - detects and resolves abandoned locks (instantly if lock is associated with local pc, else after 30 seconds)
        - temporary locks created during abandoned lock resolution keep "lockfilepath"'s extension
        - race-free (Windows, almost on Linux(NFS))
        - NOT thread-safe! (1. static LockAdmin 2. directory name aliases must be resolved sequentially!)
*/
class DirLock
{
public:
    DirLock(const Zstring& lockfilepath, DirLockCallback* callback = nullptr); //throw FileError, callback only used during construction

private:
    class LockAdmin;
    class SharedDirLock;
    std::shared_ptr<SharedDirLock> sharedLock;
};
}

#endif // DIR_LOCK_H_INCLUDED
