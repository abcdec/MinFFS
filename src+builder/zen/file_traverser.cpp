// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "file_traverser.h"
#include "sys_error.h"
#include "symlink_target.h"
#include "int64.h"

#ifdef ZEN_WIN
    #include "win_ver.h"
    #include "long_path_prefix.h"
    #include "file_access.h"
    #include "dll.h"
    #include "FindFilePlus/find_file_plus.h"

#elif defined ZEN_MAC
    #include "osx_string.h"
#endif

#if defined ZEN_LINUX || defined ZEN_MAC
    #include <cstddef> //required by GCC 4.8.1 to find ptrdiff_t
    #include <sys/stat.h>
    #include <dirent.h>
#endif

using namespace zen;


namespace
{
//implement "retry" in a generic way:

template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingDirError(Command cmd, zen::TraverseCallback& callback) //return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const FileError& e)
        {
            switch (callback.reportDirError(e.toString(), retryNumber))
            {
                case TraverseCallback::ON_ERROR_RETRY:
                    break;
                case TraverseCallback::ON_ERROR_IGNORE:
                    return false;
            }
        }
}

template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingItemError(Command cmd, zen::TraverseCallback& callback, const Zchar* shortName) //return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const FileError& e)
        {
            switch (callback.reportItemError(e.toString(), retryNumber, shortName))
            {
                case TraverseCallback::ON_ERROR_RETRY:
                    break;
                case TraverseCallback::ON_ERROR_IGNORE:
                    return false;
            }
        }
}


#ifdef ZEN_WIN
TraverseCallback::FileInfo getInfoFromFileSymlink(const Zstring& linkName) //throw FileError
{
    //open handle to target of symbolic link
    HANDLE hFile = ::CreateFile(zen::applyLongPathPrefix(linkName).c_str(),              //_In_      LPCTSTR lpFileName,
                                0,                                                       //_In_      DWORD dwDesiredAccess,
                                FILE_SHARE_READ  | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                nullptr,                    //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                OPEN_EXISTING,              //_In_      DWORD dwCreationDisposition,
                                FILE_FLAG_BACKUP_SEMANTICS, //_In_      DWORD dwFlagsAndAttributes,
                                //needed to open a directory -> keep it even if we expect to open a file! See comment below
                                nullptr);                   //_In_opt_  HANDLE hTemplateFile
    if (hFile == INVALID_HANDLE_VALUE)
        throwFileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtFileName(linkName)), L"CreateFile", getLastError());
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFile));

    BY_HANDLE_FILE_INFORMATION fileInfo = {};
    if (!::GetFileInformationByHandle(hFile, &fileInfo))
        throwFileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtFileName(linkName)), L"GetFileInformationByHandle", getLastError());

    //a file symlink may incorrectly point to a directory, but both CreateFile() and GetFileInformationByHandle() will succeed and return garbage!
    //- if we did not use FILE_FLAG_BACKUP_SEMANTICS above, CreateFile() would error out with an even less helpful ERROR_ACCESS_DENIED!
    //- reinterpreting the link as a directory symlink would still fail during traversal, so just show an error here
    //- OTOH a directory symlink that points to a file fails immediately in ::FindFirstFile() with ERROR_DIRECTORY! -> nothing to do in this case
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtFileName(linkName)), formatSystemError(L"GetFileInformationByHandle", static_cast<DWORD>(ERROR_FILE_INVALID)));

    TraverseCallback::FileInfo output;
    output.fileSize      = get64BitUInt(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);
    output.lastWriteTime = filetimeToTimeT(fileInfo.ftLastWriteTime);
    output.id            = extractFileId(fileInfo); //consider detection of moved files: allow for duplicate file ids, renaming affects symlink, not target, ...
    //output.symlinkInfo -> not filled here
    return output;
}


DWORD retrieveVolumeSerial(const Zstring& pathName) //returns 0 on error or if serial is not supported!
{
    //this works for:
    //- root paths "C:\", "D:\"
    //- network shares: \\share\dirname
    //- indirection: subst S: %USERPROFILE%
    //   -> GetVolumePathName() + GetVolumeInformation() OTOH incorrectly resolves "S:\Desktop\somedir" to "S:\Desktop\" - nice try...
    const HANDLE hDir = ::CreateFile(zen::applyLongPathPrefix(pathName).c_str(),             //_In_      LPCTSTR lpFileName,
                                     0,                                                      //_In_      DWORD dwDesiredAccess,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                     nullptr,                    //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                     OPEN_EXISTING,              //_In_      DWORD dwCreationDisposition,
                                     // FILE_FLAG_OPEN_REPARSE_POINT -> no, we follow symlinks!
                                     FILE_FLAG_BACKUP_SEMANTICS, //_In_      DWORD dwFlagsAndAttributes,
                                     /*needed to open a directory*/
                                     nullptr);                   //_In_opt_  HANDLE hTemplateFile
    if (hDir == INVALID_HANDLE_VALUE)
        return 0;
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hDir));

    BY_HANDLE_FILE_INFORMATION fileInfo = {};
    if (!::GetFileInformationByHandle(hDir, &fileInfo))
        return 0;

    return fileInfo.dwVolumeSerialNumber;
}


const bool isXpOrLater = winXpOrLater(); //VS2010 compiled DLLs are not supported on Win 2000: Popup dialog "DecodePointer not found"

#define DEF_DLL_FUN(name) const auto name = isXpOrLater ? DllFun<findplus::FunType_##name>(findplus::getDllName(), findplus::funName_##name) : DllFun<findplus::FunType_##name>();
DEF_DLL_FUN(openDir);   //
DEF_DLL_FUN(readDir);   //load at startup: avoid pre C++11 static initialization MT issues
DEF_DLL_FUN(closeDir);  //
    
/*
Common C-style interface for Win32 FindFirstFile(), FindNextFile() and FileFilePlus openDir(), closeDir():
struct TraverserPolicy //see "policy based design"
{
typedef ... DirHandle;
typedef ... FindData;

static DirHandle create(const Zstring& directory); //throw FileError - don't follow FindFirstFile() design: open handle only, *no* return of data!
static void destroy(DirHandle hnd); //throw()

static bool getEntry(DirHandle hnd, const Zstring& directory, FindData& fileInfo) //throw FileError, NeedFallbackToWin32Traverser -> fallback to FindFirstFile()/FindNextFile()

//FindData "member" functions
static TraverseCallback::FileInfo extractFileInfo(const FindData& fileInfo, DWORD volumeSerial); //volumeSerial may be 0 if not available!
static std::int64_t getModTime         (const FindData& fileInfo);
static const FILETIME& getModTimeRaw   (const FindData& fileInfo); //yet another concession to DST hack
static const FILETIME& getCreateTimeRaw(const FindData& fileInfo); //
static const wchar_t* getItemName      (const FindData& fileInfo);
static bool isDirectory                (const FindData& fileInfo);
static bool isSymlink                  (const FindData& fileInfo);
}

Note: Win32 FindFirstFile(), FindNextFile() is a weaker abstraction than FileFilePlus openDir(), readDir(), closeDir() and Unix opendir(), closedir(), stat()
*/


struct Win32Traverser
{
    struct DirHandle
    {
        DirHandle(HANDLE hnd, const WIN32_FIND_DATA& d) : searchHandle(hnd), haveData(true), data(d) {}
        explicit DirHandle(HANDLE hnd)                  : searchHandle(hnd), haveData(false) {}

        HANDLE searchHandle;
        bool haveData;
        WIN32_FIND_DATA data;
    };

    typedef WIN32_FIND_DATA FindData;

    static DirHandle create(const Zstring& dirpath) //throw FileError
    {
        const Zstring& dirpathPf = appendSeparator(dirpath);

        WIN32_FIND_DATA fileData = {};
        HANDLE hnd = ::FindFirstFile(applyLongPathPrefix(dirpathPf + L'*').c_str(), &fileData);
        //no noticable performance difference compared to FindFirstFileEx with FindExInfoBasic, FIND_FIRST_EX_CASE_SENSITIVE and/or FIND_FIRST_EX_LARGE_FETCH
        if (hnd == INVALID_HANDLE_VALUE)
        {
            const DWORD lastError = ::GetLastError(); //copy before making other system calls!
            if (lastError == ERROR_FILE_NOT_FOUND)
            {
                //1. directory may not exist *or* 2. it is completely empty: not all directories contain "., .." entries, e.g. a drive's root directory; NetDrive
                // -> FindFirstFile() is a nice example of violation of API design principle of single responsibility
                if (dirExists(dirpath)) //yes, a race-condition, still the best we can do
                    return DirHandle(hnd);
            }
            throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirpath)), L"FindFirstFile", lastError);
        }
        return DirHandle(hnd, fileData);
    }

    static void destroy(const DirHandle& hnd) { ::FindClose(hnd.searchHandle); } //throw()

    static bool getEntry(DirHandle& hnd, const Zstring& dirpath, FindData& fileInfo) //throw FileError
    {
        if (hnd.searchHandle == INVALID_HANDLE_VALUE) //handle special case of "truly empty directories"
            return false;

        if (hnd.haveData)
        {
            hnd.haveData = false;
            ::memcpy(&fileInfo, &hnd.data, sizeof(fileInfo));
            return true;
        }

        if (!::FindNextFile(hnd.searchHandle, &fileInfo))
        {
            const DWORD lastError = ::GetLastError(); //copy before making other system calls!
            if (lastError == ERROR_NO_MORE_FILES) //not an error situation
                return false;
            //else we have a problem... report it:
            throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirpath)), L"FindNextFile", lastError);
        }
        return true;
    }

    static TraverseCallback::FileInfo extractFileInfo(const FindData& fileInfo, DWORD volumeSerial)
    {
        TraverseCallback::FileInfo output;
        output.fileSize      = get64BitUInt(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);
        output.lastWriteTime = getModTime(fileInfo);
        //output.id = FileId();
        //output.symlinkInfo = nullptr;
        return output;
    }

    static std::int64_t    getModTime      (const FindData& fileInfo) { return filetimeToTimeT(fileInfo.ftLastWriteTime); }
    static const FILETIME& getModTimeRaw   (const FindData& fileInfo) { return fileInfo.ftLastWriteTime; }
    static const FILETIME& getCreateTimeRaw(const FindData& fileInfo) { return fileInfo.ftCreationTime; }
    static const wchar_t*  getItemName     (const FindData& fileInfo) { return fileInfo.cFileName; }
    static bool            isDirectory     (const FindData& fileInfo) { return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    static bool            isSymlink       (const FindData& fileInfo) { return zen::isSymlink(fileInfo); } //[!] keep namespace
};


class NeedFallbackToWin32Traverser {}; //special exception class


struct FilePlusTraverser
{
    struct DirHandle
    {
        explicit DirHandle(findplus::FindHandle hnd) : searchHandle(hnd) {}

        findplus::FindHandle searchHandle;
    };

    typedef findplus::FileInformation FindData;

    static DirHandle create(const Zstring& dirpath) //throw FileError
    {
#ifdef TODO_MinFFS_openDir_DLL_PROTO
        const findplus::FindHandle hnd = ::openDir(applyLongPathPrefix(dirpath).c_str());
#else//TODO_MinFFS_openDir_DLL_PROTO
        const findplus::FindHandle hnd = ::openDir(applyLongPathPrefix(dirpath));
#endif//TODO_MinFFS_openDir_DLL_PROTO
        if (!hnd)
            throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirpath)), L"openDir", getLastError());

        return DirHandle(hnd);
    }

    static void destroy(DirHandle hnd) { ::closeDir(hnd.searchHandle); } //throw()

    static bool getEntry(DirHandle hnd, const Zstring& dirpath, FindData& fileInfo) //throw FileError, NeedFallbackToWin32Traverser
    {
	if (!::readDir(hnd.searchHandle, fileInfo))
        {
            const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
            if (lastError == ERROR_NO_MORE_FILES) //not an error situation
                return false;

            /*
            fallback to default directory query method, if FileIdBothDirectoryInformation is not properly implemented
            this is required for NetDrive mounted Webdav, e.g. www.box.net and NT4, 2000 remote drives, et al.
            */
            if (lastError == ERROR_NOT_SUPPORTED)
                throw NeedFallbackToWin32Traverser();
            //fallback should apply to whole directory sub-tree! => client needs to handle duplicate file notifications!

            //else we have a problem... report it:
            throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirpath)), L"readDir", lastError);
        }
        return true;
    }

    static TraverseCallback::FileInfo extractFileInfo(const FindData& fileInfo, DWORD volumeSerial)
    {
        TraverseCallback::FileInfo output;
        output.fileSize      = fileInfo.fileSize;
        output.lastWriteTime = getModTime(fileInfo);
        output.id            = extractFileId(volumeSerial, fileInfo.fileId);
        //output.symlinkInfo = nullptr;
        return output;
    }

    static std::int64_t    getModTime      (const FindData& fileInfo) { return filetimeToTimeT(fileInfo.lastWriteTime); }
    static const FILETIME& getModTimeRaw   (const FindData& fileInfo) { return fileInfo.lastWriteTime; }
    static const FILETIME& getCreateTimeRaw(const FindData& fileInfo) { return fileInfo.creationTime; }
    static const wchar_t*  getItemName     (const FindData& fileInfo) { return fileInfo.shortName; }
    static bool            isDirectory     (const FindData& fileInfo) { return (fileInfo.fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    static bool            isSymlink       (const FindData& fileInfo) { return zen::isSymlink(fileInfo.fileAttributes, fileInfo.reparseTag); } //[!] keep namespace
};


class DirTraverser
{
public:
    static void execute(const Zstring& baseDirectory, TraverseCallback& sink)
    {
        DirTraverser(baseDirectory, sink);
    }

private:
    DirTraverser(const Zstring& baseDirectory, TraverseCallback& sink);
    DirTraverser           (const DirTraverser&) = delete;
    DirTraverser& operator=(const DirTraverser&) = delete;

    template <class Trav>
    void traverse(const Zstring& dirpath, TraverseCallback& sink, DWORD volumeSerial);

    template <class Trav>
    void traverseWithException(const Zstring& dirpath, TraverseCallback& sink, DWORD volumeSerial /*may be 0!*/); //throw FileError, NeedFallbackToWin32Traverser
};


template <> inline
void DirTraverser::traverse<Win32Traverser>(const Zstring& dirpath, TraverseCallback& sink, DWORD volumeSerial)
{
    tryReportingDirError([&]
    {
        traverseWithException<Win32Traverser>(dirpath, sink, 0); //throw FileError
    }, sink);
}


template <> inline
void DirTraverser::traverse<FilePlusTraverser>(const Zstring& dirpath, TraverseCallback& sink, DWORD volumeSerial)
{
    try
    {
        tryReportingDirError([&]
        {
            traverseWithException<FilePlusTraverser>(dirpath, sink, volumeSerial); //throw FileError, NeedFallbackToWin32Traverser
        }, sink);
    }
    catch (NeedFallbackToWin32Traverser&) { traverse<Win32Traverser>(dirpath, sink, 0); }
}


inline
DirTraverser::DirTraverser(const Zstring& baseDirectory, TraverseCallback& sink)
{
    try //traversing certain folders with restricted permissions requires this privilege! (but copying these files may still fail)
    {
#ifdef TODO_MinFFS_activatePrivate
    activatePrivilege(SE_BACKUP_NAME); //throw FileError
#endif//TODO_MinFFS_activatePrivate
    }
    catch (FileError&) {} //don't cause issues in user mode

    if (::openDir && ::readDir && ::closeDir)
        traverse<FilePlusTraverser>(baseDirectory, sink, retrieveVolumeSerial(baseDirectory)); //retrieveVolumeSerial returns 0 on error
    else //fallback
        traverse<Win32Traverser>(baseDirectory, sink, 0);
}


template <class Trav>
void DirTraverser::traverseWithException(const Zstring& dirpath, TraverseCallback& sink, DWORD volumeSerial /*may be 0!*/) //throw FileError, NeedFallbackToWin32Traverser
{
    //no need to check for endless recursion: Windows seems to have an internal path limit of about 700 chars

    typename Trav::DirHandle searchHandle = Trav::create(dirpath); //throw FileError
    ZEN_ON_SCOPE_EXIT(Trav::destroy(searchHandle));

    typename Trav::FindData findData = {};

    while (Trav::getEntry(searchHandle, dirpath, findData)) //throw FileError, NeedFallbackToWin32Traverser
        //don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx
    {
        //skip "." and ".."
        const Zchar* const shortName = Trav::getItemName(findData);
        if (shortName[0] == L'.' &&
            (shortName[1] == 0 || (shortName[1] == L'.' && shortName[2] == 0)))
            continue;

        const Zstring& itempath = appendSeparator(dirpath) + shortName;

        if (Trav::isSymlink(findData)) //check first!
        {
            TraverseCallback::SymlinkInfo linkInfo;
            linkInfo.lastWriteTime = Trav::getModTime (findData);

            switch (sink.onSymlink(shortName, itempath, linkInfo))
            {
                case TraverseCallback::LINK_FOLLOW:
                    if (Trav::isDirectory(findData))
                    {
                        if (TraverseCallback* trav = sink.onDir(shortName, itempath))
                        {
                            ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
                            traverse<Trav>(itempath, *trav, retrieveVolumeSerial(itempath)); //symlink may link to different volume => redetermine volume serial!
                        }
                    }
                    else //a file
                    {
                        TraverseCallback::FileInfo targetInfo;
                        const bool validLink = tryReportingItemError([&] //try to resolve symlink (and report error on failure!!!)
                        {
                            targetInfo = getInfoFromFileSymlink(itempath); //throw FileError
                            targetInfo.symlinkInfo = &linkInfo;
                        }, sink, shortName);

                        if (validLink)
                            sink.onFile(shortName, itempath, targetInfo);
                        // else //broken symlink -> ignore: it's client's responsibility to handle error!
                    }
                    break;

                case TraverseCallback::LINK_SKIP:
                    break;
            }
        }
        else if (Trav::isDirectory(findData))
        {
            if (TraverseCallback* trav = sink.onDir(shortName, itempath))
            {
                ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
                traverse<Trav>(itempath, *trav, volumeSerial);
            }
        }
        else //a file
        {
            const TraverseCallback::FileInfo fileInfo = Trav::extractFileInfo(findData, volumeSerial);
            sink.onFile(shortName, itempath, fileInfo);
        }
    }
}


#elif defined ZEN_LINUX || defined ZEN_MAC
class DirTraverser
{
public:
    static void execute(const Zstring& baseDirectory, TraverseCallback& sink)
    {
        DirTraverser(baseDirectory, sink);
    }

private:
    DirTraverser(const Zstring& baseDirectory, zen::TraverseCallback& sink)
    {
        const Zstring directoryFormatted = //remove trailing slash
            baseDirectory.size() > 1 && endsWith(baseDirectory, FILE_NAME_SEPARATOR) ?  //exception: allow '/'
            beforeLast(baseDirectory, FILE_NAME_SEPARATOR) :
            baseDirectory;

        /* quote: "Since POSIX.1 does not specify the size of the d_name field, and other nonstandard fields may precede
                   that field within the dirent structure, portable applications that use readdir_r() should allocate
                   the buffer whose address is passed in entry as follows:
                       len = offsetof(struct dirent, d_name) + pathconf(dirpath, _PC_NAME_MAX) + 1
                       entryp = malloc(len); */
        const size_t nameMax = std::max<long>(::pathconf(directoryFormatted.c_str(), _PC_NAME_MAX), 10000); //::pathconf may return long(-1)
        buffer.resize(offsetof(struct ::dirent, d_name) + nameMax + 1);

        traverse(directoryFormatted, sink);
    }

    DirTraverser           (const DirTraverser&) = delete;
    DirTraverser& operator=(const DirTraverser&) = delete;

    void traverse(const Zstring& dirpath, TraverseCallback& sink)
    {
        tryReportingDirError([&]
        {
            traverseWithException(dirpath, sink); //throw FileError
        }, sink);
    }

    void traverseWithException(const Zstring& dirpath, TraverseCallback& sink) //throw FileError
    {
        //no need to check for endless recursion: Linux has a fixed limit on the number of symbolic links in a path

        DIR* dirObj = ::opendir(dirpath.c_str()); //directory must NOT end with path separator, except "/"
        if (!dirObj)
            throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirpath)), L"opendir", getLastError());
        ZEN_ON_SCOPE_EXIT(::closedir(dirObj)); //never close nullptr handles! -> crash

        for (;;)
        {
            struct ::dirent* dirEntry = nullptr;
            if (::readdir_r(dirObj, reinterpret_cast< ::dirent*>(&buffer[0]), &dirEntry) != 0)
                throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirpath)), L"readdir_r", getLastError());
            //don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx

            if (!dirEntry) //no more items
                return;

            //don't return "." and ".."
            const char* shortName = dirEntry->d_name; //evaluate dirEntry *before* going into recursion => we use a single "buffer"!
            if (shortName[0] == '.' &&
                (shortName[1] == 0 || (shortName[1] == '.' && shortName[2] == 0)))
                continue;
#ifdef ZEN_MAC
            //some file system abstraction layers fail to properly return decomposed UTF8: http://developer.apple.com/library/mac/#qa/qa1173/_index.html
            //so we need to do it ourselves; perf: ~600 ns per conversion
            //note: it's not sufficient to apply this in z_impl::compareFilenamesNoCase: if UTF8 forms differ, FFS assumes a rename in case sensitivity and
            //   will try to propagate the rename => this won't work if target drive reports a particular UTF8 form only!
            if (CFStringRef cfStr = osx::createCFString(shortName))
            {
                ZEN_ON_SCOPE_EXIT(::CFRelease(cfStr));

                CFIndex lenMax = ::CFStringGetMaximumSizeOfFileSystemRepresentation(cfStr); //"could be much larger than the actual space required" => don't store in Zstring
                if (lenMax > 0)
                {
                    bufferUtfDecomposed.resize(lenMax);
                    if (::CFStringGetFileSystemRepresentation(cfStr, &bufferUtfDecomposed[0], lenMax)) //get decomposed UTF form (verified!) despite ambiguous documentation
                        shortName = &bufferUtfDecomposed[0]; //attention: => don't access "shortName" after recursion in "traverse"!
                }
            }
            //const char* sampleDecomposed  = "\x6f\xcc\x81.txt";
            //const char* samplePrecomposed = "\xc3\xb3.txt";
#endif
            const Zstring& itempath = appendSeparator(dirpath) + shortName;

            struct ::stat statData = {};
            if (!tryReportingItemError([&]
        {
            if (::lstat(itempath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
                    throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(itempath)), L"lstat", getLastError());
            }, sink, shortName))
            continue; //ignore error: skip file

            if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
            {
                TraverseCallback::SymlinkInfo linkInfo;
                linkInfo.lastWriteTime = statData.st_mtime; //UTC time (ANSI C format); unit: 1 second

                switch (sink.onSymlink(shortName, itempath, linkInfo))
                {
                    case TraverseCallback::LINK_FOLLOW:
                    {
                        //try to resolve symlink (and report error on failure!!!)
                        struct ::stat statDataTrg = {};
                        bool validLink = tryReportingItemError([&]
                        {
                            if (::stat(itempath.c_str(), &statDataTrg) != 0)
                                throwFileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtFileName(itempath)), L"stat", getLastError());
                        }, sink, shortName);

                        if (validLink)
                        {
                            if (S_ISDIR(statDataTrg.st_mode)) //a directory
                            {
                                if (TraverseCallback* trav = sink.onDir(shortName, itempath))
                                {
                                    ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
                                    traverse(itempath, *trav);
                                }
                            }
                            else //a file or named pipe, ect.
                            {
                                TraverseCallback::FileInfo fileInfo;
                                fileInfo.fileSize      = statDataTrg.st_size;
                                fileInfo.lastWriteTime = statDataTrg.st_mtime; //UTC time (time_t format); unit: 1 second
                                fileInfo.id            = extractFileId(statDataTrg);
                                fileInfo.symlinkInfo   = &linkInfo;
                                sink.onFile(shortName, itempath, fileInfo);
                            }
                        }
                        // else //broken symlink -> ignore: it's client's responsibility to handle error!
                    }
                    break;

                    case TraverseCallback::LINK_SKIP:
                        break;
                }
            }
            else if (S_ISDIR(statData.st_mode)) //a directory
            {
                if (TraverseCallback* trav = sink.onDir(shortName, itempath))
                {
                    ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
                    traverse(itempath, *trav);
                }
            }
            else //a file or named pipe, ect.
            {
                TraverseCallback::FileInfo fileInfo;
                fileInfo.fileSize      = statData.st_size;
                fileInfo.lastWriteTime = statData.st_mtime; //UTC time (time_t format); unit: 1 second
                fileInfo.id            = extractFileId(statData);

                sink.onFile(shortName, itempath, fileInfo);
            }
            /*
            It may be a good idea to not check "S_ISREG(statData.st_mode)" explicitly and to not issue an error message on other types to support these scenarios:
            - RTS setup watch (essentially wants to read directories only)
            - removeDirectory (wants to delete everything; pipes can be deleted just like files via "unlink")

            However an "open" on a pipe will block (https://sourceforge.net/p/freefilesync/bugs/221/), so the copy routines need to be smarter!!
            */
        }
    }

    std::vector<char> buffer;
#ifdef ZEN_MAC
    std::vector<char> bufferUtfDecomposed;
#endif
};
#endif
}


void zen::traverseFolder(const Zstring& dirpath, TraverseCallback& sink) { DirTraverser::execute(dirpath, sink); }
