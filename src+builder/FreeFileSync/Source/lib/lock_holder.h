#ifndef LOCK_HOLDER_H_489572039485723453425
#define LOCK_HOLDER_H_489572039485723453425

#include <set>
#include <zen/zstring.h>
#include <zen/stl_tools.h>
#include "dir_lock.h"
#include "status_handler.h"

namespace zen
{
//intermediate locks created by DirLock use this extension, too:
const Zchar LOCK_FILE_ENDING[]  = Zstr(".ffs_lock"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

//hold locks for a number of directories without blocking during lock creation
//call after having checked directory existence!
class LockHolder
{
public:
    LockHolder(const std::set<Zstring, LessFilename>& dirpathsExisting, //resolved dirpaths ending with path separator
               bool& warningDirectoryLockFailed,
               ProcessCallback& procCallback)
    {
        for (const Zstring& dirpathFmt : dirpathsExisting)
        {
            assert(endsWith(dirpathFmt, FILE_NAME_SEPARATOR)); //this is really the contract, formatting does other things as well, e.g. macro substitution

            class WaitOnLockHandler : public DirLockCallback
            {
            public:
                WaitOnLockHandler(ProcessCallback& pc) : pc_(pc) {}
                void requestUiRefresh()                     override { pc_.requestUiRefresh(); }  //allowed to throw exceptions
                void reportStatus(const std::wstring& text) override { pc_.reportStatus(text); }
            private:
                ProcessCallback& pc_;
            } callback(procCallback);

            try
            {
                //lock file creation is synchronous and may block noticeably for very slow devices (usb sticks, mapped cloud storages)
                lockHolder.emplace_back(dirpathFmt + Zstr("sync") + LOCK_FILE_ENDING, &callback); //throw FileError
            }
            catch (const FileError& e)
            {
                const std::wstring msg = replaceCpy(_("Cannot set directory lock for %x."), L"%x", fmtFileName(dirpathFmt)) + L"\n\n" + e.toString();
                procCallback.reportWarning(msg, warningDirectoryLockFailed); //may throw!
            }
        }
    }

private:
    std::vector<DirLock> lockHolder;
};
}

#endif //LOCK_HOLDER_H_489572039485723453425
