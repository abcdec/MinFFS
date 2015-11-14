// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 7.5         *
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

#include "file_access.h"
#include <map>
#include <algorithm>
#include <stdexcept>
#include "file_traverser.h"
#include "scope_guard.h"
#include "symlink_target.h"
#include "file_id_def.h"
#include "serialize.h"

#ifdef ZEN_WIN
    #include <Aclapi.h>
    #include "int64.h"
    #include "privilege.h"
    #include "long_path_prefix.h"
    #include "win_ver.h"
    #ifdef ZEN_WIN_VISTA_AND_LATER
        #include <zen/vista_file_op.h> //requires COM initialization!
    #endif

#ifdef MinFFS_PATCH
// Missing def for FILE_BASIC_INFO https://msdn.microsoft.com/en-us/library/windows/desktop/aa364217(v=vs.85).aspx
typedef struct _FILE_BASIC_INFO {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD         FileAttributes;
} FILE_BASIC_INFO, *PFILE_BASIC_INFO;
// Missing def for SYMBOLIC_LINK_FLAG_DIRECTORY https://msdn.microsoft.com/en-us/library/windows/desktop/aa363866(v=vs.85).aspx
#define SYMBOLIC_LINK_FLAG_DIRECTORY 1
#endif//MinFFS_PATCH

#elif defined ZEN_LINUX
    #include <sys/vfs.h> //statfs
    #include <sys/time.h> //lutimes
    #ifdef HAVE_SELINUX
        #include <selinux/selinux.h>
    #endif

#elif defined ZEN_MAC
    #include <sys/mount.h> //statfs
    #include <copyfile.h>
#endif

#if defined ZEN_LINUX || defined ZEN_MAC
    #include <fcntl.h> //open, close, AT_SYMLINK_NOFOLLOW, UTIME_OMIT
    #include <sys/stat.h>
#endif

using namespace zen;


bool zen::fileExists(const Zstring& filePath)
{
    //symbolic links (broken or not) are also treated as existing files!
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(filePath).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0; //returns true for (file-)symlinks also

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0) //follow symlinks!
        return S_ISREG(fileInfo.st_mode);
#endif
    return false;
}


bool zen::dirExists(const Zstring& dirPath)
{
    //symbolic links (broken or not) are also treated as existing directories!
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(dirPath).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0; //returns true for (dir-)symlinks also

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat dirInfo = {};
    if (::stat(dirPath.c_str(), &dirInfo) == 0) //follow symlinks!
        return S_ISDIR(dirInfo.st_mode);
#endif
    return false;
}


bool zen::symlinkExists(const Zstring& linkPath)
{
#ifdef ZEN_WIN
    WIN32_FIND_DATA linkInfo = {};
    const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(linkPath).c_str(), &linkInfo);
    if (searchHandle != INVALID_HANDLE_VALUE)
    {
        ::FindClose(searchHandle);
        return isSymlink(linkInfo);
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat linkInfo = {};
    if (::lstat(linkPath.c_str(), &linkInfo) == 0)
        return S_ISLNK(linkInfo.st_mode);
#endif
    return false;
}


bool zen::somethingExists(const Zstring& itemPath)
{
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(itemPath).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return true;
    const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

    //handle obscure file permission problem where ::GetFileAttributes() fails with ERROR_ACCESS_DENIED or ERROR_SHARING_VIOLATION
    //while parent directory traversal is successful: e.g. "C:\pagefile.sys"
    if (ec != ERROR_PATH_NOT_FOUND && //perf: short circuit for common "not existing" error codes
        ec != ERROR_FILE_NOT_FOUND && //
        ec != ERROR_BAD_NETPATH    && //
        ec != ERROR_BAD_NET_NAME)     //
    {
        WIN32_FIND_DATA fileInfo = {};
        const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(itemPath).c_str(), &fileInfo);
        if (searchHandle != INVALID_HANDLE_VALUE)
        {
            ::FindClose(searchHandle);
            return true;
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::lstat(itemPath.c_str(), &fileInfo) == 0)
        return true;
#endif
    return false;
}


namespace
{
#ifdef ZEN_WIN
bool isFatDrive(const Zstring& filePath) //noexcept
{
    const DWORD bufferSize = MAX_PATH + 1;
    std::vector<wchar_t> buffer(bufferSize);

    //this call is expensive: ~1.5 ms!
    if (!::GetVolumePathName(filePath.c_str(), //__in   LPCTSTR lpszFileName,
                             &buffer[0],       //__out  LPTSTR lpszVolumePathName,
                             bufferSize))      //__in   DWORD cchBufferLength
    {
        assert(false);
        return false;
    }

    const Zstring volumePath = appendSeparator(&buffer[0]);

    //suprisingly fast: ca. 0.03 ms per call!
    if (!::GetVolumeInformation(volumePath.c_str(), //__in_opt   LPCTSTR lpRootPathName,
                                nullptr,     //__out      LPTSTR lpVolumeNameBuffer,
                                0,           //__in       DWORD nVolumeNameSize,
                                nullptr,     //__out_opt  LPDWORD lpVolumeSerialNumber,
                                nullptr,     //__out_opt  LPDWORD lpMaximumComponentLength,
                                nullptr,     //__out_opt  LPDWORD lpFileSystemFlags,
                                &buffer[0],  //__out      LPTSTR lpFileSystemNameBuffer,
                                bufferSize)) //__in       DWORD nFileSystemNameSize
    {
        assert(false);
        return false;
    }

    return &buffer[0] == Zstring(L"FAT") ||
           &buffer[0] == Zstring(L"FAT32");
}
#endif
}


std::uint64_t zen::getFilesize(const Zstring& filePath) //throw FileError
{
#ifdef ZEN_WIN
    {
        WIN32_FIND_DATA fileInfo = {};
        const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(filePath).c_str(), &fileInfo);
        if (searchHandle == INVALID_HANDLE_VALUE)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"FindFirstFile");
        ::FindClose(searchHandle);

        if (!isSymlink(fileInfo))
            return get64BitUInt(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);
    }
    //        WIN32_FILE_ATTRIBUTE_DATA sourceAttr = {};
    //        if (!::GetFileAttributesEx(applyLongPathPrefix(filePath).c_str(), //__in   LPCTSTR lpFileName,
    //                                   GetFileExInfoStandard,                 //__in   GET_FILEEX_INFO_LEVELS fInfoLevelId,
    //                                   &sourceAttr))                          //__out  LPVOID lpFileInformation

    //open handle to target of symbolic link
    const HANDLE hFile = ::CreateFile(applyLongPathPrefix(filePath).c_str(),                  //_In_      LPCTSTR lpFileName,
                                      0,                                                      //_In_      DWORD dwDesiredAccess,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                      nullptr,                                                //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                      OPEN_EXISTING,                                          //_In_      DWORD dwCreationDisposition,
                                      FILE_FLAG_BACKUP_SEMANTICS, /*needed to open a directory*/ //_In_      DWORD dwFlagsAndAttributes,
                                      nullptr);                                               //_In_opt_  HANDLE hTemplateFile
    if (hFile == INVALID_HANDLE_VALUE)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"CreateFile");
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFile));

    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(hFile,      //_In_  HANDLE         hFile,
                         &fileSize)) //_Out_ PLARGE_INTEGER lpFileSize
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"GetFileSizeEx");
    return fileSize.QuadPart;

    //alternative:
    //BY_HANDLE_FILE_INFORMATION fileInfoHnd = {};
    //if (!::GetFileInformationByHandle(hFile, &fileInfoHnd))
    //    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"GetFileInformationByHandle");
    //return get64BitUInt(fileInfoHnd.nFileSizeLow, fileInfoHnd.nFileSizeHigh);

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"stat");

    return fileInfo.st_size;
#endif
}


std::uint64_t zen::getFreeDiskSpace(const Zstring& path) //throw FileError, returns 0 if not available
{
#ifdef ZEN_WIN
    ULARGE_INTEGER bytesFree = {};
    if (!::GetDiskFreeSpaceEx(appendSeparator(path).c_str(), //__in_opt   LPCTSTR lpDirectoryName, -> "UNC name [...] must include a trailing backslash, for example, "\\MyServer\MyShare\"
                              &bytesFree,                    //__out_opt  PULARGE_INTEGER lpFreeBytesAvailable,
                              nullptr,                       //__out_opt  PULARGE_INTEGER lpTotalNumberOfBytes,
                              nullptr))                      //__out_opt  PULARGE_INTEGER lpTotalNumberOfFreeBytes
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(path)), L"GetDiskFreeSpaceEx");

    //return 0 if info is not available: "The GetDiskFreeSpaceEx function returns zero for lpFreeBytesAvailable for all CD requests"
    return get64BitUInt(bytesFree.LowPart, bytesFree.HighPart);

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::statfs info = {};
    if (::statfs(path.c_str(), &info) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(path)), L"statfs");

    return static_cast<std::uint64_t>(info.f_bsize) * info.f_bavail;
#endif
}


bool zen::removeFile(const Zstring& filePath) //throw FileError
{
#ifdef ZEN_WIN
    const wchar_t functionName[] = L"DeleteFile";

    if (!::DeleteFile(applyLongPathPrefix(filePath).c_str()))
#elif defined ZEN_LINUX || defined ZEN_MAC
    const wchar_t functionName[] = L"unlink";
    if (::unlink(filePath.c_str()) != 0)
#endif
    {
        ErrorCode ec = getLastError(); //copy before directly/indirectly making other system calls!
#ifdef ZEN_WIN
        if (ec == ERROR_ACCESS_DENIED) //function fails if file is read-only
        {
            ::SetFileAttributes(applyLongPathPrefix(filePath).c_str(), FILE_ATTRIBUTE_NORMAL); //(try to) normalize file attributes

            if (::DeleteFile(applyLongPathPrefix(filePath).c_str())) //now try again...
                return true;
            ec = ::GetLastError();
        }
#endif
        if (!somethingExists(filePath)) //warning: changes global error code!!
            return false; //neither file nor any other object (e.g. broken symlink) with that name existing - caveat: what if "access is denied"!?!??!?!?

        //begin of "regular" error reporting
        const std::wstring errorMsg = replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(filePath));
        std::wstring errorDescr = formatSystemError(functionName, ec);

#ifdef ZEN_WIN_VISTA_AND_LATER
        if (ec == ERROR_SHARING_VIOLATION || //-> enhance error message!
            ec == ERROR_LOCK_VIOLATION)
        {
            const std::wstring procList = vista::getLockingProcesses(filePath); //noexcept
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
        }
#endif
        throw FileError(errorMsg, errorDescr);
    }
    return true;
}


void zen::removeDirectorySimple(const Zstring& dirPath) //throw FileError
{
#ifdef ZEN_WIN
    //(try to) normalize file attributes: actually NEEDED for symbolic links also!
    ::SetFileAttributes(applyLongPathPrefix(dirPath).c_str(), FILE_ATTRIBUTE_NORMAL);

    const wchar_t functionName[] = L"RemoveDirectory";
    if (!::RemoveDirectory(applyLongPathPrefix(dirPath).c_str()))
#elif defined ZEN_LINUX || defined ZEN_MAC
    const wchar_t functionName[] = L"rmdir";
    if (::rmdir(dirPath.c_str()) != 0)
#endif
    {
        const ErrorCode ec = getLastError(); //copy before making other system calls!

        if (!somethingExists(dirPath)) //warning: changes global error code!!
            return;

#if defined ZEN_LINUX || defined ZEN_MAC
        if (symlinkExists(dirPath))
        {
            if (::unlink(dirPath.c_str()) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), L"unlink");
            return;
        }
#endif
        throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), formatSystemError(functionName, ec));
    }
    /*
    Windows: may spuriously fail with ERROR_DIR_NOT_EMPTY(145) even though all child items have
    successfully been *marked* for deletion, but some application still has a handle open!
    e.g. Open "C:\Test\Dir1\Dir2" (filled with lots of files) in Explorer, then delete "C:\Test\Dir1" via ::RemoveDirectory() => Error 145
    Sample code: http://us.generation-nt.com/answer/createfile-directory-handles-removing-parent-help-29126332.html
    Alternatives: 1. move file/empty folder to some other location, then DeleteFile()/RemoveDirectory()
                  2. use CreateFile/FILE_FLAG_DELETE_ON_CLOSE *without* FILE_SHARE_DELETE instead of DeleteFile() => early failure
    */
}


namespace
{
void removeDirectoryImpl(const Zstring& folderPath) //throw FileError
{
    assert(dirExists(folderPath)); //[!] no symlinks in this context!!!
    //attention: check if folderPath is a symlink! Do NOT traverse into it deleting contained files!!!

    std::vector<Zstring> filePaths;
    std::vector<Zstring> folderSymlinkPaths;
    std::vector<Zstring> folderPaths;

    //get all files and directories from current directory (WITHOUT subdirectories!)
    traverseFolder(folderPath,
    [&](const FileInfo&    fi) { filePaths.push_back(fi.fullPath); },
    [&](const DirInfo&     di) { folderPaths .push_back(di.fullPath); }, //defer recursion => save stack space and allow deletion of extremely deep hierarchies!
    [&](const SymlinkInfo& si)
    {
#ifdef ZEN_WIN
        if (dirExists(si.fullPath)) //dir symlink
            folderSymlinkPaths.push_back(si.fullPath);
        else //file symlink, broken symlink
#endif
            filePaths.push_back(si.fullPath);
    },
    [](const std::wstring& errorMsg) { throw FileError(errorMsg); });

    for (const Zstring& filePath : filePaths)
        removeFile(filePath); //throw FileError

    for (const Zstring& symlinkPath : folderSymlinkPaths)
        removeDirectorySimple(symlinkPath); //throw FileError

    //delete directories recursively
    for (const Zstring& subFolderPath : folderPaths)
        removeDirectoryImpl(subFolderPath); //throw FileError; call recursively to correctly handle symbolic links

    removeDirectorySimple(folderPath); //throw FileError
}
}


void zen::removeDirectoryRecursively(const Zstring& dirPath) //throw FileError
{
    if (symlinkExists(dirPath))
        removeDirectorySimple(dirPath); //throw FileError
    else if (somethingExists(dirPath))
        removeDirectoryImpl(dirPath); //throw FileError
}


namespace
{
/* Usage overview: (avoid circular pattern!)

  renameFile()  -->  renameFile_sub()
      |               /|\
     \|/               |
      Fix8Dot3NameClash()
*/
//wrapper for file system rename function:
void renameFile_sub(const Zstring& pathSource, const Zstring& pathTarget) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
#ifdef ZEN_WIN
    const Zstring pathSourceFmt = applyLongPathPrefix(pathSource);
    const Zstring pathTargetFmt = applyLongPathPrefix(pathTarget);

    if (!::MoveFileEx(pathSourceFmt.c_str(), //__in      LPCTSTR lpExistingFileName,
                      pathTargetFmt.c_str(), //__in_opt  LPCTSTR lpNewFileName,
                      0))                 //__in      DWORD dwFlags
    {
        DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

        if (ec == ERROR_ACCESS_DENIED) //MoveFileEx may fail to rename a read-only file on a SAMBA-share -> (try to) handle this
        {
            const DWORD oldAttr = ::GetFileAttributes(pathSourceFmt.c_str());
            if (oldAttr != INVALID_FILE_ATTRIBUTES && (oldAttr & FILE_ATTRIBUTE_READONLY))
            {
                if (::SetFileAttributes(pathSourceFmt.c_str(), FILE_ATTRIBUTE_NORMAL)) //remove readonly-attribute
                {
                    //try again...
                    if (::MoveFileEx(pathSourceFmt.c_str(), //__in      LPCTSTR lpExistingFileName,
                                     pathTargetFmt.c_str(), //__in_opt  LPCTSTR lpNewFileName,
                                     0))                 //__in      DWORD dwFlags
                    {
                        //(try to) restore file attributes
                        ::SetFileAttributes(pathTargetFmt.c_str(), oldAttr); //don't handle error
                        return;
                    }
                    else
                    {
                        ec = ::GetLastError(); //use error code from second call to ::MoveFileEx()
                        //cleanup: (try to) restore file attributes: assume pathSource is still existing
                        ::SetFileAttributes(pathSourceFmt.c_str(), oldAttr);
                    }
                }
            }
        }
        //begin of "regular" error reporting
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x", L"\n" + fmtPath(pathSource)), L"%y", L"\n" + fmtPath(pathTarget));
        std::wstring errorDescr = formatSystemError(L"MoveFileEx", ec);

#ifdef ZEN_WIN_VISTA_AND_LATER //(try to) enhance error message
        if (ec == ERROR_SHARING_VIOLATION ||
            ec == ERROR_LOCK_VIOLATION)
        {
            const std::wstring procList = vista::getLockingProcesses(pathSource); //noexcept
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
        }
#endif

        if (ec == ERROR_NOT_SAME_DEVICE)
            throw ErrorDifferentVolume(errorMsg, errorDescr);
        if (ec == ERROR_ALREADY_EXISTS || //-> used on Win7 x64
            ec == ERROR_FILE_EXISTS)      //-> used by XP???
            throw ErrorTargetExisting(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    //rename() will never fail with EEXIST, but always (atomically) overwrite! => equivalent to SetFileInformationByHandle() + FILE_RENAME_INFO::ReplaceIfExists
    //=> Linux: renameat2() with RENAME_NOREPLACE -> still new, probably buggy
    //=> OS X: no solution

    auto throwException = [&](int ec)
    {
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x", L"\n" + fmtPath(pathSource)), L"%y", L"\n" + fmtPath(pathTarget));
        const std::wstring errorDescr = formatSystemError(L"rename", ec);

        if (ec == EXDEV)
            throw ErrorDifferentVolume(errorMsg, errorDescr);
        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    };

    if (!equalFilePath(pathSource, pathTarget)) //OS X: changing file name case is not an "already exists" error!
        if (somethingExists(pathTarget))
            throwException(EEXIST);

    if (::rename(pathSource.c_str(), pathTarget.c_str()) != 0)
        throwException(errno);
#endif
}


#ifdef ZEN_WIN
/*small wrapper around
::GetShortPathName()
::GetLongPathName() */
template <typename Function>
Zstring getFilenameFmt(const Zstring& filePath, Function fun) //throw(); returns empty string on error
{
    const Zstring filePathFmt = applyLongPathPrefix(filePath);

    const DWORD bufferSize = fun(filePathFmt.c_str(), nullptr, 0);
    if (bufferSize == 0)
        return Zstring();

    std::vector<wchar_t> buffer(bufferSize);

    const DWORD charsWritten = fun(filePathFmt.c_str(), //__in   LPCTSTR lpszShortPath,
                                   &buffer[0],          //__out  LPTSTR  lpszLongPath,
                                   bufferSize);         //__in   DWORD   cchBuffer
    if (charsWritten == 0 || charsWritten >= bufferSize)
        return Zstring();

    return &buffer[0];
}


Zstring findUnused8Dot3Name(const Zstring& filePath) //find a unique 8.3 short name
{
    const Zstring pathPrefix = contains(filePath, FILE_NAME_SEPARATOR) ?
                               (beforeLast(filePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE) + FILE_NAME_SEPARATOR) : Zstring();

    //extension needn't contain reasonable data
    Zstring extension = getFileExtension(filePath);
    if (extension.empty())
        extension = Zstr("FFS");
    else if (extension.length() > 3)
        extension.resize(3);

    for (int index = 0; index < 100000000; ++index) //filePath must be representable by <= 8 characters
    {
        const Zstring output = pathPrefix + numberTo<Zstring>(index) + Zchar('.') + extension;
        if (!somethingExists(output)) //ensure uniqueness
            return output;
    }
    throw std::runtime_error(std::string("100,000,000 files, one for each number, exist in this directory? You're kidding...") + utfCvrtTo<std::string>(pathPrefix) +
                             "\n" + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}


bool have8dot3NameClash(const Zstring& filePath)
{
    if (!contains(filePath, FILE_NAME_SEPARATOR))
        return false;

    if (somethingExists(filePath)) //name OR directory!
    {
        const Zstring origName  = afterLast(filePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
        const Zstring shortName = afterLast(getFilenameFmt(filePath, ::GetShortPathName), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL); //throw() returns empty string on error
        const Zstring longName  = afterLast(getFilenameFmt(filePath, ::GetLongPathName ), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL); //

        if (!shortName.empty() &&
            !longName .empty() &&
            equalFilePath(origName,  shortName) &&
            !equalFilePath(shortName, longName))
        {
            //for filePath short and long file name are equal and another unrelated file happens to have the same short name
            //e.g. filePath == "TESTWE~1", but another file is existing named "TestWeb" with short name ""TESTWE~1"
            return true;
        }
    }
    return false;
}

class Fix8Dot3NameClash //throw FileError
{
public:
    Fix8Dot3NameClash(const Zstring& filePath)
    {
        const Zstring longName = afterLast(getFilenameFmt(filePath, ::GetLongPathName), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL); //throw() returns empty string on error

        unrelatedFile = beforeLast(filePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (!unrelatedFile.empty())
            unrelatedFile += FILE_NAME_SEPARATOR;
        unrelatedFile += longName;

        //find another name in short format: this ensures the actual short name WILL be renamed as well!
        unrelatedFileParked = findUnused8Dot3Name(filePath);

        //move already existing short name out of the way for now
        renameFile_sub(unrelatedFile, unrelatedFileParked); //throw FileError, ErrorDifferentVolume
        //DON'T call renameFile() to avoid reentrance!
    }

    ~Fix8Dot3NameClash()
    {
        //the file system should assign this unrelated file a new (unique) short name
        try
        {
            renameFile_sub(unrelatedFileParked, unrelatedFile); //throw FileError, ErrorDifferentVolume
        }
        catch (FileError&) {}
    }
private:
    Zstring unrelatedFile;
    Zstring unrelatedFileParked;
};
#endif
}


//rename file: no copying!!!
void zen::renameFile(const Zstring& pathSource, const Zstring& pathTarget) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
    try
    {
        renameFile_sub(pathSource, pathTarget); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
    }
    catch (const ErrorTargetExisting&)
    {
#ifdef ZEN_WIN
        //try to handle issues with already existing short 8.3 file names on Windows
        if (have8dot3NameClash(pathTarget))
        {
            Fix8Dot3NameClash dummy(pathTarget); //throw FileError; move clashing file path to the side
            //now try again...
            renameFile_sub(pathSource, pathTarget); //throw FileError
            return;
        }
#endif
        throw;
    }
}


namespace
{

#ifdef ZEN_WIN
void setFileTimeRaw(const Zstring& filePath,
                    const FILETIME* creationTime, //optional
                    const FILETIME& lastWriteTime,
                    ProcSymlink procSl) //throw FileError
{
    {
        //extra scope for debug check below

        //privilege SE_BACKUP_NAME doesn't seem to be required here for symbolic links
        //note: setting privileges requires admin rights!

        //opening newly created target file may fail due to some AV-software scanning it: no error, we will wait!
        //http://support.microsoft.com/?scid=kb%3Ben-us%3B316609&x=17&y=20
        //-> enable as soon it turns out it is required!

        /*const int retryInterval = 50;
        const int maxRetries = 2000 / retryInterval;
        for (int i = 0; i < maxRetries; ++i)
        {
        */

        /*
        if (hTarget == INVALID_HANDLE_VALUE && ::GetLastError() == ERROR_SHARING_VIOLATION)
            ::Sleep(retryInterval); //wait then retry
        else //success or unknown failure
            break;
        }
        */
        //temporarily reset read-only flag if required
        DWORD attribs = INVALID_FILE_ATTRIBUTES;
        ZEN_ON_SCOPE_EXIT(
            if (attribs != INVALID_FILE_ATTRIBUTES)
            ::SetFileAttributes(applyLongPathPrefix(filePath).c_str(), attribs);
        );

        auto removeReadonly = [&]() -> bool //throw FileError; may need to remove the readonly-attribute (e.g. on FAT usb drives)
        {
            if (attribs == INVALID_FILE_ATTRIBUTES)
            {
                const DWORD tmpAttr = ::GetFileAttributes(applyLongPathPrefix(filePath).c_str());
                if (tmpAttr == INVALID_FILE_ATTRIBUTES)
                    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"GetFileAttributes");

                if (tmpAttr & FILE_ATTRIBUTE_READONLY)
                {
                    if (!::SetFileAttributes(applyLongPathPrefix(filePath).c_str(), FILE_ATTRIBUTE_NORMAL))
                        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtPath(filePath)), L"SetFileAttributes");

                    attribs = tmpAttr; //reapplied on scope exit
                    return true;
                }
            }
            return false;
        };

        auto openFile = [&](bool conservativeApproach)
        {
            return ::CreateFile(applyLongPathPrefix(filePath).c_str(), //_In_      LPCTSTR lpFileName,
                                (conservativeApproach ?
                                 //some NAS seem to have issues with FILE_WRITE_ATTRIBUTES, even worse, they may fail silently!
                                 //http://sourceforge.net/tracker/?func=detail&atid=1093081&aid=3536680&group_id=234430
                                 //Citrix shares seem to have this issue, too, but at least fail with "access denied" => try generic access first:
                                 GENERIC_READ | GENERIC_WRITE :
                                 //avoids mysterious "access denied" when using "GENERIC_READ | GENERIC_WRITE" on a read-only file, even *after* read-only was removed directly before the call!
                                 //http://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3514569&group_id=234430
                                 //since former gives an error notification we may very well try FILE_WRITE_ATTRIBUTES second.
                                 FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES),         //_In_      DWORD dwDesiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                nullptr,                                                //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                OPEN_EXISTING,                                          //_In_      DWORD dwCreationDisposition,
                                (procSl == ProcSymlink::DIRECT ? FILE_FLAG_OPEN_REPARSE_POINT : 0) |
                                FILE_FLAG_BACKUP_SEMANTICS, /*needed to open a directory*/ //_In_      DWORD dwFlagsAndAttributes,
                                nullptr);                                               //_In_opt_  HANDLE hTemplateFile
        };

        HANDLE hFile = INVALID_HANDLE_VALUE;
        for (int i = 0; i < 2; ++i) //we will get this handle, no matter what! :)
        {
            //1. be conservative
            hFile = openFile(true);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                if (::GetLastError() == ERROR_ACCESS_DENIED) //fails if file is read-only (or for "other" reasons)
                    if (removeReadonly()) //throw FileError
                        continue;

                //2. be a *little* fancy
                hFile = openFile(false);
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!
                    if (ec == ERROR_ACCESS_DENIED)
                        if (removeReadonly()) //throw FileError
                            continue;

                    //3. after these herculean stunts we give up...
                    throw FileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), formatSystemError(L"CreateFile", ec));
                }
            }
            break;
        }
        ZEN_ON_SCOPE_EXIT(::CloseHandle(hFile));


        if (!::SetFileTime(hFile,           //__in      HANDLE hFile,
                           creationTime,    //__in_opt  const FILETIME *lpCreationTime,
                           nullptr,         //__in_opt  const FILETIME *lpLastAccessTime,
                           &lastWriteTime)) //__in_opt  const FILETIME *lpLastWriteTime
        {
            DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

            //function may fail if file is read-only: https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3514569&group_id=234430
            if (ec == ERROR_ACCESS_DENIED)
            {
                //dynamically load windows API function: available with Windows Vista and later
                typedef BOOL (WINAPI* SetFileInformationByHandleFunc)(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize);
                const SysDllFun<SetFileInformationByHandleFunc> setFileInformationByHandle(L"kernel32.dll", "SetFileInformationByHandle");

                if (setFileInformationByHandle) //if not: let the original error propagate!
                {
                    auto setFileInfo = [&](FILE_BASIC_INFO basicInfo) //throw FileError; no const& since SetFileInformationByHandle() requires non-const parameter!
                    {
                        if (!setFileInformationByHandle(hFile,              //__in  HANDLE hFile,
                                                        FileBasicInfo,      //__in  FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
                                                        &basicInfo,         //__in  LPVOID lpFileInformation,
                                                        sizeof(basicInfo))) //__in  DWORD dwBufferSize
                            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtPath(filePath)), L"SetFileInformationByHandle");
                    };

                    auto toLargeInteger = [](const FILETIME& ft) -> LARGE_INTEGER
                    {
                        LARGE_INTEGER tmp = {};
                        tmp.LowPart  = ft.dwLowDateTime;
                        tmp.HighPart = ft.dwHighDateTime;
                        return tmp;
                    };
                    //---------------------------------------------------------------------------

                    BY_HANDLE_FILE_INFORMATION fileInfo = {};
                    if (::GetFileInformationByHandle(hFile, &fileInfo))
                        if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                        {
                            FILE_BASIC_INFO basicInfo = {}; //undocumented: file times of "0" stand for "don't change"
                            basicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;        //[!] the bug in the ticket above requires we set this together with file times!!!
                            basicInfo.LastWriteTime = toLargeInteger(lastWriteTime); //
                            if (creationTime)
                                basicInfo.CreationTime = toLargeInteger(*creationTime);

                            //set file time + attributes
                            setFileInfo(basicInfo); //throw FileError

                            try //... to restore original file attributes
                            {
                                FILE_BASIC_INFO basicInfo2 = {};
                                basicInfo2.FileAttributes = fileInfo.dwFileAttributes;
                                setFileInfo(basicInfo2); //throw FileError
                            }
                            catch (FileError&) {}

                            ec = ERROR_SUCCESS;
                        }
                }
            }

            std::wstring errorMsg = replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath));

            //add more meaningful message: FAT accepts only a subset of the NTFS date range
            if (ec == ERROR_INVALID_PARAMETER &&
                isFatDrive(filePath))
            {
                //we need a low-level reliable routine to format a potentially invalid date => don't use strftime!!!
                auto fmtDate = [](const FILETIME& ft)
                {
                    SYSTEMTIME st = {};
                    if (!::FileTimeToSystemTime(&ft,  //__in   const FILETIME *lpFileTime,
                                                &st)) //__out  LPSYSTEMTIME lpSystemTime
                        return std::wstring();

                    std::wstring dateTime;
                    {
                        const int bufferSize = ::GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, nullptr, 0);
                        if (bufferSize > 0)
                        {
                            std::vector<wchar_t> buffer(bufferSize);
                            if (::GetDateFormat(LOCALE_USER_DEFAULT, //_In_       LCID Locale,
                                                0,                   //_In_       DWORD dwFlags,
                                                &st,                  //_In_opt_   const SYSTEMTIME *lpDate,
                                                nullptr,              //_In_opt_   LPCTSTR lpFormat,
                                                &buffer[0],       //_Out_opt_  LPTSTR lpDateStr,
                                                bufferSize) > 0)          //_In_       int cchDate
                                dateTime = &buffer[0]; //GetDateFormat() returns char count *including* 0-termination!
                        }
                    }

                    const int bufferSize = ::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, nullptr, 0);
                    if (bufferSize > 0)
                    {
                        std::vector<wchar_t> buffer(bufferSize);
                        if (::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, &buffer[0], bufferSize) > 0)
                        {
                            dateTime += L" ";
                            dateTime += &buffer[0]; //GetDateFormat() returns char count *including* 0-termination!
                        }
                    }
                    return dateTime;
                };

                errorMsg += std::wstring(L"\nA FAT volume can only store dates between 1980 and 2107:\n") +
                            L"\twrite (UTC): \t" + fmtDate(lastWriteTime) +
                            (creationTime ? L"\n\tcreate (UTC): \t" + fmtDate(*creationTime) : L"");
            }

            if (ec != ERROR_SUCCESS)
                throw FileError(errorMsg, formatSystemError(L"SetFileTime", ec));
        }
    }
#ifndef NDEBUG //verify written data: mainly required to check consistency of DST hack
    FILETIME creationTimeDbg  = {};
    FILETIME lastWriteTimeDbg = {};

    HANDLE hFile = ::CreateFile(applyLongPathPrefix(filePath).c_str(),                  //_In_      LPCTSTR lpFileName,
                                FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,           //_In_      DWORD dwDesiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                nullptr,                                                //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                OPEN_EXISTING,                                          //_In_      DWORD dwCreationDisposition,
                                (procSl == ProcSymlink::DIRECT ? FILE_FLAG_OPEN_REPARSE_POINT : 0) |
                                FILE_FLAG_BACKUP_SEMANTICS, /*needed to open a directory*/ //_In_      DWORD dwFlagsAndAttributes,
                                nullptr);                                               //_In_opt_  HANDLE hTemplateFile
    assert(hFile != INVALID_HANDLE_VALUE);
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFile));

    assert(::GetFileTime(hFile, //probably more up to date than GetFileAttributesEx()!?
                         &creationTimeDbg,
                         nullptr,
                         &lastWriteTimeDbg));

    assert(std::abs(filetimeToTimeT(lastWriteTimeDbg) - filetimeToTimeT(lastWriteTime)) <= 2); //respect 2 second FAT/FAT32 precision
    //assert(std::abs(filetimeToTimeT(creationTimeDbg ) - filetimeToTimeT(creationTime )) <= 2); -> creation time not available for Linux-hosted Samba shares!
#endif
}


#elif defined ZEN_LINUX
DEFINE_NEW_FILE_ERROR(ErrorLinuxFallbackToUtimes);

void setFileTimeRaw(const Zstring& filePath, const struct ::timespec& modTime, ProcSymlink procSl) //throw FileError, ErrorLinuxFallbackToUtimes
{
    /*
    [2013-05-01] sigh, we can't use utimensat() on NTFS volumes on Ubuntu: silent failure!!! what morons are programming this shit???
    => fallback to "retarded-idiot version"! -- DarkByte

    [2015-03-09]
     - cannot reproduce issues with NTFS and utimensat() on Ubuntu
     - utimensat() is supposed to obsolete utime/utimes and is also used by "cp" and "touch"
     - solves utimes() EINVAL bug for certain CIFS/NTFS drives: https://sourceforge.net/p/freefilesync/discussion/help/thread/1ace042d/
        => don't use utimensat() directly, but open file descriptor manually, else EINVAL, again!

    => let's give utimensat another chance:
    */
    struct ::timespec newTimes[2] = {};
    newTimes[0].tv_sec = ::time(nullptr); //access time; using UTIME_OMIT for tv_nsec would trigger even more bugs!!
    //https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/218564cf/
    newTimes[1] = modTime; //modification time

    //=> using open()/futimens() for regular files and utimensat(AT_SYMLINK_NOFOLLOW) for symlinks is consistent with "cp" and "touch"!
    if (procSl == ProcSymlink::FOLLOW)
    {
        const int fdFile = ::open(filePath.c_str(), O_WRONLY, 0); //"if O_CREAT is not specified, then mode is ignored"
        if (fdFile == -1)
        {
            if (errno == EACCES) //bullshit, access denied even with 0777 permissions! => utimes should work!
                throw ErrorLinuxFallbackToUtimes(L"");

            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"open");
        }
        ZEN_ON_SCOPE_EXIT(::close(fdFile));

        if (::futimens(fdFile, newTimes) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"futimens");
    }
    else
    {
        if (::utimensat(AT_FDCWD, filePath.c_str(), newTimes, AT_SYMLINK_NOFOLLOW) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"utimensat");
    }
}


#elif defined ZEN_MAC
struct AttrBufFileTimes
{
    std::uint32_t length = 0;
    struct ::timespec createTime = {}; //keep order; see docs!
    struct ::timespec writeTime  = {}; //
}  __attribute__((aligned(4), packed));


void setFileTimeRaw(const Zstring& filePath,
                    const struct ::timespec* createTime, //optional
                    const struct ::timespec& writeTime,
                    ProcSymlink procSl) //throw FileError
{
    //OS X: utime() is obsoleted by utimes()! utimensat() not yet implemented
    //use ::setattrlist() instead of ::utimes() => 1. set file creation times 2. nanosecond precision
    //https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/setattrlist.2.html

    struct ::attrlist attribs = {};
    attribs.bitmapcount = ATTR_BIT_MAP_COUNT;
    attribs.commonattr = (createTime ? ATTR_CMN_CRTIME : 0) | ATTR_CMN_MODTIME;

    AttrBufFileTimes newTimes;
    if (createTime)
    {
        newTimes.createTime.tv_sec  = createTime->tv_sec;
        newTimes.createTime.tv_nsec = createTime->tv_nsec;
    }
    newTimes.writeTime.tv_sec  = writeTime.tv_sec;
    newTimes.writeTime.tv_nsec = writeTime.tv_nsec;

    const int rv = ::setattrlist(filePath.c_str(), //const char* path,
                                 &attribs,         //struct ::attrlist* attrList,
                                 createTime ? &newTimes.createTime : &newTimes.writeTime,                     //void* attrBuf,
                                 (createTime ? sizeof(newTimes.createTime) : 0) + sizeof(newTimes.writeTime), //size_t attrBufSize,
                                 procSl == ProcSymlink::DIRECT ? FSOPT_NOFOLLOW : 0);                         //unsigned long options
    if (rv != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"setattrlist");
}

/*
void getFileTimeRaw(int fd, //throw FileError
                    const Zstring& filePath, //for error reporting only
                    struct ::timespec& createTime, //out
                    struct ::timespec& writeTime)  //
{
    //https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/getattrlist.2.html
    struct ::attrlist attribs = {};
    attribs.bitmapcount = ATTR_BIT_MAP_COUNT;
    attribs.commonattr = ATTR_CMN_CRTIME | ATTR_CMN_MODTIME;

    AttrBufFileTimes fileTimes;

    const int rv = ::fgetattrlist(fd,                //int fd,
                                  &attribs,          //struct ::attrlist* attrList,
                                  &fileTimes,        //void* attrBuf,
                                  sizeof(fileTimes), //size_t attrBufSize,
                                  0);                //unsigned long options
    if (rv != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"getattrlist");

    createTime.tv_sec  = fileTimes.createTime.tv_sec;
    createTime.tv_nsec = fileTimes.createTime.tv_nsec;
    writeTime.tv_sec   = fileTimes.writeTime.tv_sec;
    writeTime.tv_nsec  = fileTimes.writeTime.tv_nsec;
}
*/
#endif
}


void zen::setFileTime(const Zstring& filePath, std::int64_t modTime, ProcSymlink procSl) //throw FileError
{
#ifdef ZEN_WIN
    setFileTimeRaw(filePath, nullptr, timetToFileTime(modTime), procSl); //throw FileError

#elif defined ZEN_LINUX
    try
    {
        struct ::timespec writeTime = {};
        writeTime.tv_sec = modTime;
        setFileTimeRaw(filePath, writeTime, procSl); //throw FileError, ErrorLinuxFallbackToUtimes
    }
    catch (ErrorLinuxFallbackToUtimes&)
    {
        struct ::timeval writeTime[2] = {};
        writeTime[0].tv_sec = ::time(nullptr); //access time (seconds)
        writeTime[1].tv_sec = modTime;         //modification time (seconds)

        if (procSl == ProcSymlink::FOLLOW)
        {
            if (::utimes(filePath.c_str(), writeTime) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"utimes");
        }
        else
        {
            if (::lutimes(filePath.c_str(), writeTime) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(filePath)), L"lutimes");
        }
    }

#elif defined ZEN_MAC
    struct ::timespec writeTime = {};
    writeTime.tv_sec = modTime;
    setFileTimeRaw(filePath, nullptr, writeTime, procSl); //throw FileError
#endif
}


bool zen::supportsPermissions(const Zstring& dirpath) //throw FileError
{
#ifdef ZEN_WIN
    const DWORD bufferSize = MAX_PATH + 1;
    std::vector<wchar_t> buffer(bufferSize);

    if (!::GetVolumePathName(dirpath.c_str(), //__in   LPCTSTR lpszFileName,
                             &buffer[0],      //__out  LPTSTR lpszVolumePathName,
                             bufferSize))     //__in   DWORD cchBufferLength
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(dirpath)), L"GetVolumePathName");

    const Zstring volumePath = appendSeparator(&buffer[0]);

    DWORD fsFlags = 0;
    if (!::GetVolumeInformation(volumePath.c_str(), //__in_opt   LPCTSTR lpRootPathName,
                                nullptr,    //__out      LPTSTR  lpVolumeNameBuffer,
                                0,          //__in       DWORD   nVolumeNameSize,
                                nullptr,    //__out_opt  LPDWORD lpVolumeSerialNumber,
                                nullptr,    //__out_opt  LPDWORD lpMaximumComponentLength,
                                &fsFlags,   //__out_opt  LPDWORD lpFileSystemFlags,
                                nullptr,    //__out      LPTSTR  lpFileSystemNameBuffer,
                                0))         //__in       DWORD   nFileSystemNameSize
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(dirpath)), L"GetVolumeInformation");

    return (fsFlags & FILE_PERSISTENT_ACLS) != 0;

#elif defined ZEN_LINUX || defined ZEN_MAC
    return true;
#endif
}


namespace
{
#ifdef HAVE_SELINUX
//copy SELinux security context
void copySecurityContext(const Zstring& source, const Zstring& target, ProcSymlink procSl) //throw FileError
{
    security_context_t contextSource = nullptr;
    const int rv = procSl == ProcSymlink::FOLLOW ?
                   ::getfilecon(source.c_str(), &contextSource) :
                   ::lgetfilecon(source.c_str(), &contextSource);
    if (rv < 0)
    {
        if (errno == ENODATA ||  //no security context (allegedly) is not an error condition on SELinux
            errno == EOPNOTSUPP) //extended attributes are not supported by the filesystem
            return;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read security context of %x."), L"%x", fmtPath(source)), L"getfilecon");
    }
    ZEN_ON_SCOPE_EXIT(::freecon(contextSource));

    {
        security_context_t contextTarget = nullptr;
        const int rv2 = procSl == ProcSymlink::FOLLOW ?
                        ::getfilecon(target.c_str(), &contextTarget) :
                        ::lgetfilecon(target.c_str(), &contextTarget);
        if (rv2 < 0)
        {
            if (errno == EOPNOTSUPP)
                return;
            //else: still try to set security context
        }
        else
        {
            ZEN_ON_SCOPE_EXIT(::freecon(contextTarget));

            if (::strcmp(contextSource, contextTarget) == 0) //nothing to do
                return;
        }
    }

    const int rv3 = procSl == ProcSymlink::FOLLOW ?
                    ::setfilecon(target.c_str(), contextSource) :
                    ::lsetfilecon(target.c_str(), contextSource);
    if (rv3 < 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write security context of %x."), L"%x", fmtPath(target)), L"setfilecon");
}
#endif


//copy permissions for files, directories or symbolic links: requires admin rights
void copyItemPermissions(const Zstring& sourcePath, const Zstring& targetPath, ProcSymlink procSl) //throw FileError
{
#ifdef ZEN_WIN
    //in contrast to ::SetSecurityInfo(), ::SetFileSecurity() seems to honor the "inherit DACL/SACL" flags
    //CAVEAT: if a file system does not support ACLs, GetFileSecurity() will return successfully with a *valid* security descriptor containing *no* ACL entries!

    //NOTE: ::GetFileSecurity()/::SetFileSecurity() do NOT follow Symlinks! getResolvedSymlinkPath() requires Vista or later!
    const Zstring sourceResolved = procSl == ProcSymlink::FOLLOW && symlinkExists(sourcePath) ? getResolvedSymlinkPath(sourcePath) : sourcePath; //throw FileError
    const Zstring targetResolved = procSl == ProcSymlink::FOLLOW && symlinkExists(targetPath) ? getResolvedSymlinkPath(targetPath) : targetPath; //

    //setting privileges requires admin rights!
    try
    {
#ifdef TODO_MinFFS_activatePrivilege
        //enable privilege: required to read/write SACL information (only)
        activatePrivilege(SE_SECURITY_NAME); //throw FileError
        //Note: trying to copy SACL (SACL_SECURITY_INFORMATION) may return ERROR_PRIVILEGE_NOT_HELD (1314) on Samba shares. This is not due to missing privileges!
        //However, this is okay, since copying NTFS permissions doesn't make sense in this case anyway

        //the following privilege may be required according to http://msdn.microsoft.com/en-us/library/aa364399(VS.85).aspx (although not needed nor active in my tests)
        activatePrivilege(SE_BACKUP_NAME); //throw FileError

        //enable privilege: required to copy owner information
        activatePrivilege(SE_RESTORE_NAME); //throw FileError
#endif//TODO_MinFFS_activatePrivilege
    }
    catch (const FileError& e)//add some more context description (e.g. user is not an admin)
    {
        throw FileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourceResolved)), e.toString());
    }


    std::vector<char> buffer(10000); //example of actually required buffer size: 192 bytes
    for (;;)
    {
        DWORD bytesNeeded = 0;
        if (::GetFileSecurity(applyLongPathPrefix(sourceResolved).c_str(), //__in LPCTSTR lpFileName, -> long path prefix IS needed, although it is NOT mentioned on MSDN!!!
                              DACL_SECURITY_INFORMATION  | SACL_SECURITY_INFORMATION | //__in       SECURITY_INFORMATION RequestedInformation,
                              OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
                              reinterpret_cast<PSECURITY_DESCRIPTOR>(&buffer[0]),     //__out_opt  PSECURITY_DESCRIPTOR pSecurityDescriptor,
                              static_cast<DWORD>(buffer.size()), //__in       DWORD nLength,
                              &bytesNeeded))                     //__out      LPDWORD lpnLengthNeeded
            break;
        //failure: ...
        if (bytesNeeded > buffer.size())
            buffer.resize(bytesNeeded);
        else
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourceResolved)), L"GetFileSecurity");
    }
    SECURITY_DESCRIPTOR& secDescr = reinterpret_cast<SECURITY_DESCRIPTOR&>(buffer[0]);

    /*
        SECURITY_DESCRIPTOR_CONTROL secCtrl = 0;
        {
            DWORD ctrlRev = 0;
            if (!::GetSecurityDescriptorControl(&secDescr, // __in   PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                                &secCtrl,  // __out  PSECURITY_DESCRIPTOR_CONTROL pControl,
                                                &ctrlRev)) //__out  LPDWORD lpdwRevision
                throw FileErro
       }
    //interesting flags:
    //#define SE_DACL_PRESENT                  (0x0004)
    //#define SE_SACL_PRESENT                  (0x0010)
    //#define SE_DACL_PROTECTED                (0x1000)
    //#define SE_SACL_PROTECTED                (0x2000)
    */

    if (!::SetFileSecurity(applyLongPathPrefix(targetResolved).c_str(), //__in  LPCTSTR lpFileName, -> long path prefix IS needed, although it is NOT mentioned on MSDN!!!
                           OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                           DACL_SECURITY_INFORMATION  | SACL_SECURITY_INFORMATION, //__in  SECURITY_INFORMATION SecurityInformation,
                           &secDescr)) //__in  PSECURITY_DESCRIPTOR pSecurityDescriptor
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetResolved)), L"SetFileSecurity");

    /*
    PSECURITY_DESCRIPTOR buffer = nullptr;
    PSID owner                  = nullptr;
    PSID group                  = nullptr;
    PACL dacl                   = nullptr;
    PACL sacl                   = nullptr;

    //File Security and Access Rights:    http://msdn.microsoft.com/en-us/library/aa364399(v=VS.85).aspx
    //SECURITY_INFORMATION Access Rights: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379573(v=vs.85).aspx
    const HANDLE hSource = ::CreateFile(applyLongPathPrefix(source).c_str(),
                                        READ_CONTROL | ACCESS_SYSTEM_SECURITY, //ACCESS_SYSTEM_SECURITY required for SACL access
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_FLAG_BACKUP_SEMANTICS | (procSl == SYMLINK_DIRECT ? FILE_FLAG_OPEN_REPARSE_POINT : 0), //FILE_FLAG_BACKUP_SEMANTICS needed to open a directory
                                        nullptr);
    if (hSource == INVALID_HANDLE_VALUE)
        throw FileError
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hSource));

    //  DWORD rc = ::GetNamedSecurityInfo(const_cast<WCHAR*>(applyLongPathPrefix(source).c_str()), -> does NOT dereference symlinks!
    DWORD rc = ::GetSecurityInfo(hSource,        //__in       LPTSTR pObjectName,
                                 SE_FILE_OBJECT, //__in       SE_OBJECT_TYPE ObjectType,
                                 OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                 DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,  //__in       SECURITY_INFORMATION SecurityInfo,
                                 &owner,   //__out_opt  PSID *ppsidOwner,
                                 &group,   //__out_opt  PSID *ppsidGroup,
                                 &dacl,    //__out_opt  PACL *ppDacl,
                                 &sacl,    //__out_opt  PACL *ppSacl,
                                 &buffer); //__out_opt  PSECURITY_DESCRIPTOR *ppSecurityDescriptor
    if (rc != ERROR_SUCCESS)
        throw FileError
    ZEN_ON_SCOPE_EXIT(::LocalFree(buffer));

    SECURITY_DESCRIPTOR_CONTROL secCtrl = 0;
    {
    DWORD ctrlRev = 0;
    if (!::GetSecurityDescriptorControl(buffer, // __in   PSECURITY_DESCRIPTOR pSecurityDescriptor,
    &secCtrl, // __out  PSECURITY_DESCRIPTOR_CONTROL pControl,
    &ctrlRev))//__out  LPDWORD lpdwRevision
        throw FileError
    }

    //may need to remove the readonly-attribute
    FileUpdateHandle targetHandle(target, [=]
    {
        return ::CreateFile(applyLongPathPrefix(target).c_str(),                              // lpFileName
                            GENERIC_WRITE | WRITE_OWNER | WRITE_DAC | ACCESS_SYSTEM_SECURITY, // dwDesiredAccess: all four seem to be required!!!
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,           // dwShareMode
                            nullptr,                       // lpSecurityAttributes
                            OPEN_EXISTING,              // dwCreationDisposition
                            FILE_FLAG_BACKUP_SEMANTICS | (procSl == SYMLINK_DIRECT ? FILE_FLAG_OPEN_REPARSE_POINT : 0), // dwFlagsAndAttributes
                            nullptr);                        // hTemplateFile
    });

    if (targetHandle.get() == INVALID_HANDLE_VALUE)
        throw FileError

        SECURITY_INFORMATION secFlags = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;

        //SACL/DACL inheritence flag is NOT copied by default: we have to tell ::SetSecurityInfo(() to enable/disable it manually!
        //if (secCtrl & SE_DACL_PRESENT)
        secFlags |= (secCtrl & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION;
        //if (secCtrl & SE_SACL_PRESENT)
        secFlags |= (secCtrl & SE_SACL_PROTECTED) ? PROTECTED_SACL_SECURITY_INFORMATION : UNPROTECTED_SACL_SECURITY_INFORMATION;


    //  rc = ::SetNamedSecurityInfo(const_cast<WCHAR*>(applyLongPathPrefix(target).c_str()), //__in      LPTSTR pObjectName, -> does NOT dereference symlinks!
    rc = ::SetSecurityInfo(targetHandle.get(), //__in      LPTSTR pObjectName,
                           SE_FILE_OBJECT,     //__in      SE_OBJECT_TYPE ObjectType,
                           secFlags, //__in      SECURITY_INFORMATION SecurityInfo,
                           owner, //__in_opt  PSID psidOwner,
                           group, //__in_opt  PSID psidGroup,
                           dacl,  //__in_opt  PACL pDacl,
                           sacl); //__in_opt  PACL pSacl

    if (rc != ERROR_SUCCESS)
        throw FileError
            */

#elif defined ZEN_LINUX

#ifdef HAVE_SELINUX  //copy SELinux security context
    copySecurityContext(sourcePath, targetPath, procSl); //throw FileError
#endif

    struct ::stat fileInfo = {};
    if (procSl == ProcSymlink::FOLLOW)
    {
        if (::stat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"stat");

        if (::chown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chown");

        if (::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chmod");
    }
    else
    {
        if (::lstat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"lstat");

        if (::lchown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"lchown");

        if (!symlinkExists(targetPath) && //setting access permissions doesn't make sense for symlinks on Linux: there is no lchmod()
            ::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chmod");
    }

#elif defined ZEN_MAC
    copyfile_flags_t flags = COPYFILE_ACL | COPYFILE_STAT; //unfortunately COPYFILE_STAT copies modtime, too!
    if (procSl == ProcSymlink::DIRECT)
        flags |= COPYFILE_NOFOLLOW;

    if (::copyfile(sourcePath.c_str(), targetPath.c_str(), 0, flags) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(replaceCpy(_("Cannot copy permissions from %x to %y."), L"%x", L"\n" + fmtPath(sourcePath)), L"%y", L"\n" + fmtPath(targetPath)), L"copyfile");

    //owner is *not* copied with ::copyfile():

    struct ::stat fileInfo = {};
    if (procSl == ProcSymlink::FOLLOW)
    {
        if (::stat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"stat");

        if (::chown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chown");
    }
    else
    {
        if (::lstat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"lstat");

        if (::lchown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"lchown");
    }
#endif
}


void makeDirectoryRecursivelyImpl(const Zstring& directory) //FileError
{
    assert(!endsWith(directory, FILE_NAME_SEPARATOR)); //even "C:\" should be "C:" as input!

    try
    {
        copyNewDirectory(Zstring(), directory, false /*copyFilePermissions*/); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
    }
    catch (const ErrorTargetExisting&) {} //*something* existing: folder or FILE!
    catch (const ErrorTargetPathMissing&)
    {
        //we need to create parent directories first
        const Zstring dirParent = beforeLast(directory, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (!dirParent.empty())
        {
            //recurse...
            makeDirectoryRecursivelyImpl(dirParent); //throw FileError

            //now try again...
            copyNewDirectory(Zstring(), directory, false /*copyFilePermissions*/); //throw FileError, (ErrorTargetExisting), (ErrorTargetPathMissing)
            return;
        }
        throw;
    }
}
}


void zen::makeDirectoryRecursively(const Zstring& dirpath) //throw FileError
{
    //remove trailing separator (even for C:\ root directories)
    const Zstring dirFormatted = endsWith(dirpath, FILE_NAME_SEPARATOR) ?
                                 beforeLast(dirpath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE) :
                                 dirpath;
    makeDirectoryRecursivelyImpl(dirFormatted); //FileError
}


//source path is optional (may be empty)
void zen::copyNewDirectory(const Zstring& sourcePath, const Zstring& targetPath, //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
                           bool copyFilePermissions)
{
#ifdef ZEN_WIN
auto getErrorMsg = [](const Zstring& path){ return replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(path)); };

//special handling for volume root: trying to create existing root directory results in ERROR_ACCESS_DENIED rather than ERROR_ALREADY_EXISTS!
    Zstring dirTmp = removeLongPathPrefix(endsWith(targetPath, FILE_NAME_SEPARATOR) ?
                                          beforeLast(targetPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE) :
                                          targetPath);
    if (dirTmp.size() == 2 &&
        isAlpha(dirTmp[0]) && dirTmp[1] == L':')
    {
        dirTmp += FILE_NAME_SEPARATOR; //we do not support "C:" to represent a relative path!

        const DWORD ec = somethingExists(dirTmp) ? ERROR_ALREADY_EXISTS : ERROR_PATH_NOT_FOUND; //don't use dirExists() => harmonize with ErrorTargetExisting!
            const std::wstring errorDescr = formatSystemError(L"CreateDirectory", ec);

        if (ec == ERROR_ALREADY_EXISTS)
            throw ErrorTargetExisting(getErrorMsg(dirTmp), errorDescr);
        throw FileError(getErrorMsg(dirTmp), errorDescr); //[!] this is NOT a ErrorTargetPathMissing case!
    }

    //deliberately don't support creating irregular folders like "...." https://social.technet.microsoft.com/Forums/windows/en-US/ffee2322-bb6b-4fdf-86f9-8f93cf1fa6cb/
    if (endsWith(targetPath, L' ') ||
        endsWith(targetPath, L'.'))
        throw FileError(getErrorMsg(targetPath), replaceCpy(_("%x is not a regular directory name."), L"%x", fmtPath(afterLast(targetPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL))));

    //don't use ::CreateDirectoryEx:
    //- it may fail with "wrong parameter (error code 87)" when source is on mapped online storage
    //- automatically copies symbolic links if encountered: unfortunately it doesn't copy symlinks over network shares but silently creates empty folders instead (on XP)!
    //- it isn't able to copy most junctions because of missing permissions (although target path can be retrieved alternatively!)
    if (!::CreateDirectory(applyLongPathPrefixCreateDir(targetPath).c_str(), //__in      LPCTSTR lpPathName,
                           nullptr))                                        //__in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes
    {
        DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

        //handle issues with already existing short 8.3 file names on Windows
        if (ec == ERROR_ALREADY_EXISTS)
            if (have8dot3NameClash(targetPath))
            {
                Fix8Dot3NameClash dummy(targetPath); //throw FileError; move clashing object to the side

                //now try again...
                if (::CreateDirectory(applyLongPathPrefixCreateDir(targetPath).c_str(), nullptr))
                    ec = ERROR_SUCCESS;
                else
                    ec = ::GetLastError();
            }

        if (ec != ERROR_SUCCESS)
        {
            const std::wstring errorDescr = formatSystemError(L"CreateDirectory", ec);

            if (ec == ERROR_ALREADY_EXISTS)
                throw ErrorTargetExisting(getErrorMsg(targetPath), errorDescr);
            else if (ec == ERROR_PATH_NOT_FOUND)
                throw ErrorTargetPathMissing(getErrorMsg(targetPath), errorDescr);
            throw FileError(getErrorMsg(targetPath), errorDescr);
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //0777, default for newly created directories

    struct ::stat dirInfo = {};
    if (!sourcePath.empty())
        if (::stat(sourcePath.c_str(), &dirInfo) == 0)
        {
            mode = dirInfo.st_mode; //analog to "cp" which copies "mode" (considering umask) by default
            mode |= S_IRWXU; //FFS only: we need full access to copy child items! "cp" seems to apply permissions *after* copying child items
        }
    //=> need copyItemPermissions() only for "chown" and umask-agnostic permissions

    if (::mkdir(targetPath.c_str(), mode) != 0)
    {
        const int lastError = errno; //copy before directly or indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(targetPath));
        const std::wstring errorDescr = formatSystemError(L"mkdir", lastError);

        if (lastError == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        else if (lastError == ENOENT)
            throw ErrorTargetPathMissing(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    }
#endif

    if (!sourcePath.empty())
    {
#ifdef ZEN_WIN
        //optional: try to copy file attributes (dereference symlinks and junctions)
        const HANDLE hDirSrc = ::CreateFile(zen::applyLongPathPrefix(sourcePath).c_str(),          //_In_      LPCTSTR lpFileName,
                                            0,                                                      //_In_      DWORD dwDesiredAccess,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                            nullptr,                                                //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                            OPEN_EXISTING,                                          //_In_      DWORD dwCreationDisposition,
                                            // FILE_FLAG_OPEN_REPARSE_POINT -> no, we follow symlinks!
                                            FILE_FLAG_BACKUP_SEMANTICS, /*needed to open a directory*/ //_In_      DWORD dwFlagsAndAttributes,
                                            nullptr);                                               //_In_opt_  HANDLE hTemplateFile
        if (hDirSrc != INVALID_HANDLE_VALUE) //dereferencing a symbolic link usually fails if it is located on network drive or client is XP: NOT really an error...
        {
            ZEN_ON_SCOPE_EXIT(::CloseHandle(hDirSrc));

            BY_HANDLE_FILE_INFORMATION dirInfo = {};
            if (::GetFileInformationByHandle(hDirSrc, &dirInfo))
            {
                ::SetFileAttributes(applyLongPathPrefix(targetPath).c_str(), dirInfo.dwFileAttributes);
                //copy "read-only and system attributes": http://blogs.msdn.com/b/oldnewthing/archive/2003/09/30/55100.aspx

                const bool isEncrypted  = (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED)  != 0;
                const bool isCompressed = (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;

                if (isEncrypted)
                    ::EncryptFile(targetPath.c_str()); //seems no long path is required (check passed!)

                HANDLE hDirTrg = ::CreateFile(applyLongPathPrefix(targetPath).c_str(), //_In_      LPCTSTR lpFileName,
                                              GENERIC_READ | GENERIC_WRITE,           //_In_      DWORD dwDesiredAccess,
                                              /*read access required for FSCTL_SET_COMPRESSION*/
                                              FILE_SHARE_READ  |
                                              FILE_SHARE_WRITE |
                                              FILE_SHARE_DELETE,          //_In_      DWORD dwShareMode,
                                              nullptr,                    //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                              OPEN_EXISTING,              //_In_      DWORD dwCreationDisposition,
                                              FILE_FLAG_BACKUP_SEMANTICS, //_In_      DWORD dwFlagsAndAttributes,
                                              nullptr);                   //_In_opt_  HANDLE hTemplateFile
                if (hDirTrg != INVALID_HANDLE_VALUE)
                {
                    ZEN_ON_SCOPE_EXIT(::CloseHandle(hDirTrg));

                    if (isCompressed)
                    {
                        USHORT cmpState = COMPRESSION_FORMAT_DEFAULT;
                        DWORD bytesReturned = 0;
                        /*bool rv = */::DeviceIoControl(hDirTrg,               //_In_         HANDLE hDevice,
                                                        FSCTL_SET_COMPRESSION, //_In_         DWORD dwIoControlCode,
                                                        &cmpState,             //_In_opt_     LPVOID lpInBuffer,
                                                        sizeof(cmpState),      //_In_         DWORD nInBufferSize,
                                                        nullptr,               //_Out_opt_    LPVOID lpOutBuffer,
                                                        0,                     //_In_         DWORD nOutBufferSize,
                                                        &bytesReturned,        //_Out_opt_    LPDWORD lpBytesReturned,
                                                        nullptr);              //_Inout_opt_  LPOVERLAPPED lpOverlapped
                    }

                    //(try to) set creation and modification time
                    /*bool rv = */::SetFileTime(hDirTrg,                   //_In_       HANDLE hFile,
                                                &dirInfo.ftCreationTime,   //_Out_opt_  LPFILETIME lpCreationTime,
                                                nullptr,                   //_Out_opt_  LPFILETIME lpLastAccessTime,
                                                &dirInfo.ftLastWriteTime); //_Out_opt_  LPFILETIME lpLastWriteTime
                }
            }
        }

#elif defined ZEN_MAC
        /*int rv =*/ ::copyfile(sourcePath.c_str(), targetPath.c_str(), 0, COPYFILE_XATTR);
#endif

        ZEN_ON_SCOPE_FAIL(try { removeDirectorySimple(targetPath); }
        catch (FileError&) {});   //ensure cleanup:

        //enforce copying file permissions: it's advertized on GUI...
        if (copyFilePermissions)
            copyItemPermissions(sourcePath, targetPath, ProcSymlink::FOLLOW); //throw FileError
    }
}


void zen::copySymlink(const Zstring& sourceLink, const Zstring& targetLink, bool copyFilePermissions) //throw FileError
{
    const Zstring linkPath = getSymlinkTargetRaw(sourceLink); //throw FileError; accept broken symlinks

#ifdef ZEN_WIN
    const bool isDirLink = [&]() -> bool
    {
        const DWORD ret = ::GetFileAttributes(applyLongPathPrefix(sourceLink).c_str());
        return ret != INVALID_FILE_ATTRIBUTES && (ret & FILE_ATTRIBUTE_DIRECTORY);
    }();

    typedef BOOLEAN (WINAPI* CreateSymbolicLinkFunc)(LPCTSTR lpSymlinkFileName, LPCTSTR lpTargetFileName, DWORD dwFlags);
    const SysDllFun<CreateSymbolicLinkFunc> createSymbolicLink(L"kernel32.dll", "CreateSymbolicLinkW");

    if (!createSymbolicLink)
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L"\n" + fmtPath(sourceLink)), L"%y", L"\n" + fmtPath(targetLink)),
                        replaceCpy(_("Cannot find system function %x."), L"%x", L"\"CreateSymbolicLinkW\""));

    const wchar_t functionName[] = L"CreateSymbolicLinkW";
    if (!createSymbolicLink(targetLink.c_str(), //__in  LPTSTR lpSymlinkFileName, - seems no long path prefix is required...
                            linkPath.c_str(),   //__in  LPTSTR lpTargetFileName,
                            (isDirLink ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))) //__in  DWORD dwFlags
#elif defined ZEN_LINUX || defined ZEN_MAC
    const wchar_t functionName[] = L"symlink";
    if (::symlink(linkPath.c_str(), targetLink.c_str()) != 0)
#endif
        THROW_LAST_FILE_ERROR(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L"\n" + fmtPath(sourceLink)), L"%y", L"\n" + fmtPath(targetLink)), functionName);

    //allow only consistent objects to be created -> don't place before ::symlink, targetLink may already exist!

    auto cleanUp = [&]
    {
        try
        {
#ifdef ZEN_WIN
            if (isDirLink)
                removeDirectorySimple(targetLink); //throw FileError
            else
#endif
                removeFile(targetLink); //throw FileError
        }
        catch (FileError&) {}
    };
    ZEN_ON_SCOPE_FAIL(cleanUp());

    //file times: essential for sync'ing a symlink: enforce this! (don't just try!)
#ifdef ZEN_WIN
    WIN32_FILE_ATTRIBUTE_DATA sourceAttr = {};
    if (!::GetFileAttributesEx(applyLongPathPrefix(sourceLink).c_str(), //__in   LPCTSTR lpFileName,
                               GetFileExInfoStandard,                   //__in   GET_FILEEX_INFO_LEVELS fInfoLevelId,
                               &sourceAttr))                            //__out  LPVOID lpFileInformation
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceLink)), L"GetFileAttributesEx");

    setFileTimeRaw(targetLink, &sourceAttr.ftCreationTime, sourceAttr.ftLastWriteTime, ProcSymlink::DIRECT); //throw FileError

#elif defined ZEN_LINUX
    struct ::stat sourceInfo = {};
    if (::lstat(sourceLink.c_str(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceLink)), L"lstat");

    setFileTime(targetLink, sourceInfo.st_mtime, ProcSymlink::DIRECT); //throw FileError

#elif defined ZEN_MAC
    struct ::stat sourceInfo = {};
    if (::lstat(sourceLink.c_str(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceLink)), L"lstat");

    if (::copyfile(sourceLink.c_str(), targetLink.c_str(), 0, COPYFILE_XATTR | COPYFILE_NOFOLLOW) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(replaceCpy(_("Cannot copy attributes from %x to %y."), L"%x", L"\n" + fmtPath(sourceLink)), L"%y", L"\n" + fmtPath(targetLink)), L"copyfile");

    setFileTimeRaw(targetLink, &sourceInfo.st_birthtimespec, sourceInfo.st_mtimespec, ProcSymlink::DIRECT); //throw FileError
#endif

    if (copyFilePermissions)
        copyItemPermissions(sourceLink, targetLink, ProcSymlink::DIRECT); //throw FileError
}


namespace
{
#ifdef ZEN_WIN
/*
            CopyFileEx()    BackupRead()      FileRead()
            --------------------------------------------
Attributes       YES            NO               NO
create time      NO             NO               NO
ADS              YES            YES              NO
Encrypted        YES            NO(silent fail!) NO
Compressed       NO             NO               NO
Sparse           NO             YES              NO
Nonstandard FS   YES                  UNKNOWN    -> error writing ADS to Samba, issues reading from NAS, error copying files having "blocked" state... ect.
PERF               -                 6% faster

Mark stream as compressed: FSCTL_SET_COMPRESSION - compatible with both BackupRead() and FileRead()


Current support for combinations of NTFS extended attributes:

source attr | tf normal | tf compressed | tf encrypted | handled by
============|==================================================================
    ---     |    ---           -C-             E--       copyFileWindowsDefault
    --S     |    --S           -CS             E-S       copyFileWindowsBackupStream
    -C-     |    -C-           -C-             E--       copyFileWindowsDefault
    -CS     |    -CS           -CS             E-S       copyFileWindowsBackupStream
    E--     |    E--           E--             E--       copyFileWindowsDefault
    E-S     |    E-- (NOK)     E-- (NOK)       E-- (NOK) copyFileWindowsDefault -> may fail with ERROR_DISK_FULL for large sparse files!!

tf  := target folder
E   := encrypted
C   := compressed
S   := sparse
NOK := current behavior is not optimal/OK yet.

Note: - if target parent folder is compressed or encrypted, both attributes are added automatically during file creation!
      - "compressed" and "encrypted" are mutually exclusive: http://support.microsoft.com/kb/223093/en-us
*/


//due to issues on non-NTFS volumes, we should use the copy-as-sparse routine only if required and supported!
template <class Function>
bool canCopyAsSparse(DWORD fileAttrSource, Function getTargetFsFlags) //throw ()
{
    const bool sourceIsEncrypted = (fileAttrSource & FILE_ATTRIBUTE_ENCRYPTED)   != 0;
    const bool sourceIsSparse    = (fileAttrSource & FILE_ATTRIBUTE_SPARSE_FILE) != 0;

    if (sourceIsEncrypted || !sourceIsSparse) //BackupRead() silently fails reading encrypted files!
        return false; //small perf optimization: don't check "targetFile" if not needed

    DWORD targetFsFlags = 0;
    if (!getTargetFsFlags(targetFsFlags))
        return false;
    assert(targetFsFlags != 0);

    return (targetFsFlags & FILE_SUPPORTS_SPARSE_FILES) != 0;
}


#ifdef ZEN_WIN_VISTA_AND_LATER
bool canCopyAsSparse(DWORD fileAttrSource, HANDLE hTargetFile) //throw ()
{
    return canCopyAsSparse(fileAttrSource, [&](DWORD& targetFsFlags) -> bool
    {
        return ::GetVolumeInformationByHandleW(hTargetFile,    //_In_ HANDLE hFile,
        nullptr,        //_Out_writes_opt_(nVolumeNameSize) LPWSTR lpVolumeNameBuffer,
        0,              //_In_ DWORD nVolumeNameSize,
        nullptr,        //_Out_opt_ LPDWORD lpVolumeSerialNumber,
        nullptr,        //_Out_opt_ LPDWORD lpMaximumComponentLength,
        &targetFsFlags, //_Out_opt_ LPDWORD lpFileSystemFlags,
        nullptr,        //_Out_writes_opt_(nFileSystemNameSize) LPWSTR lpFileSystemNameBuffer,
        0) != 0;        //_In_ DWORD nFileSystemNameSize
    });
}
#endif


bool canCopyAsSparse(DWORD fileAttrSource, const Zstring& targetFile) //throw ()
{
    return canCopyAsSparse(fileAttrSource, [&targetFile](DWORD& targetFsFlags) -> bool
    {
        const DWORD bufferSize = MAX_PATH + 1;
        std::vector<wchar_t> buffer(bufferSize);

        //full pathName need not yet exist!
        if (!::GetVolumePathName(targetFile.c_str(), //__in   LPCTSTR lpszFileName,
        &buffer[0],         //__out  LPTSTR lpszVolumePathName,
        bufferSize))        //__in   DWORD cchBufferLength
            return false;

        const Zstring volumePath = appendSeparator(&buffer[0]);

        return ::GetVolumeInformation(volumePath.c_str(), //__in_opt   LPCTSTR lpRootPathName
        nullptr,        //__out_opt  LPTSTR lpVolumeNameBuffer,
        0,              //__in       DWORD nVolumeNameSize,
        nullptr,        //__out_opt  LPDWORD lpVolumeSerialNumber,
        nullptr,        //__out_opt  LPDWORD lpMaximumComponentLength,
        &targetFsFlags, //__out_opt  LPDWORD lpFileSystemFlags,
        nullptr,        //__out      LPTSTR lpFileSystemNameBuffer,
        0) != 0;             //__in       DWORD nFileSystemNameSize
    });
}


bool canCopyAsSparse(const Zstring& sourceFile, const Zstring& targetFile) //throw ()
{
    //follow symlinks!
    HANDLE hSource = ::CreateFile(applyLongPathPrefix(sourceFile).c_str(), //_In_      LPCTSTR lpFileName,
                                  0,                                       //_In_      DWORD dwDesiredAccess,
                                  FILE_SHARE_READ  |  //all shared modes are required to read files that are open in other applications
                                  FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,  //_In_      DWORD dwShareMode,
                                  nullptr,            //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  OPEN_EXISTING,      //_In_      DWORD dwCreationDisposition,
                                  0,                  //_In_      DWORD dwFlagsAndAttributes,
                                  nullptr);           //_In_opt_  HANDLE hTemplateFile
    if (hSource == INVALID_HANDLE_VALUE)
        return false;
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hSource));

    BY_HANDLE_FILE_INFORMATION fileInfoSource = {};
    if (!::GetFileInformationByHandle(hSource, &fileInfoSource))
        return false;

    return canCopyAsSparse(fileInfoSource.dwFileAttributes, targetFile); //throw ()
}

//=============================================================================================

InSyncAttributes copyFileWindowsBackupStream(const Zstring& sourceFile, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                             const Zstring& targetFile,
                                             const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    //try to get backup read and write privileges: help solve most "access denied" errors with FILE_FLAG_BACKUP_SEMANTICS:
    //https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/1998ebf2/
#ifdef TODO_MinFFS_activatePrivilege
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) {}
    try { activatePrivilege(SE_RESTORE_NAME); }
    catch (const FileError&) {}
#endif//TODO_MinFFS_activatePrivilege

    //open sourceFile for reading
    HANDLE hFileSource = ::CreateFile(applyLongPathPrefix(sourceFile).c_str(), //_In_      LPCTSTR lpFileName,
                                      GENERIC_READ,                            //_In_      DWORD dwDesiredAccess,
                                      FILE_SHARE_READ | FILE_SHARE_DELETE,     //_In_      DWORD dwShareMode,
                                      nullptr,                                 //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                      OPEN_EXISTING,                           //_In_      DWORD dwCreationDisposition,
                                      //FILE_FLAG_OVERLAPPED must not be used!
                                      //FILE_FLAG_NO_BUFFERING should not be used!
                                      FILE_FLAG_SEQUENTIAL_SCAN |
                                      FILE_FLAG_BACKUP_SEMANTICS,              //_In_      DWORD dwFlagsAndAttributes,
                                      nullptr);                                //_In_opt_  HANDLE hTemplateFile
    if (hFileSource == INVALID_HANDLE_VALUE)
    {
        const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

        const std::wstring errorMsg = replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile));
        std::wstring errorDescr = formatSystemError(L"CreateFile", ec);

        //if file is locked throw "ErrorFileLocked" instead!
        if (ec == ERROR_SHARING_VIOLATION ||
            ec == ERROR_LOCK_VIOLATION)
        {
#ifdef ZEN_WIN_VISTA_AND_LATER //(try to) enhance error message
            const std::wstring procList = vista::getLockingProcesses(sourceFile); //noexcept
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
#endif
            throw ErrorFileLocked(errorMsg, errorDescr);
        }

        throw FileError(errorMsg, errorDescr);
    }
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFileSource));

    //----------------------------------------------------------------------
    BY_HANDLE_FILE_INFORMATION fileInfoSource = {};
    if (!::GetFileInformationByHandle(hFileSource, &fileInfoSource))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceFile)), L"GetFileInformationByHandle");

    //encrypted files cannot be read with BackupRead which would fail silently!
    const bool sourceIsEncrypted = (fileInfoSource.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    if (sourceIsEncrypted)
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), L"BackupRead: Source file is encrypted.");
    //----------------------------------------------------------------------

    const DWORD validAttribs = FILE_ATTRIBUTE_NORMAL   | //"This attribute is valid only if used alone."
                               FILE_ATTRIBUTE_READONLY |
                               FILE_ATTRIBUTE_HIDDEN   |
                               FILE_ATTRIBUTE_SYSTEM   |
                               FILE_ATTRIBUTE_ARCHIVE  |           //those two are not set properly (not worse than ::CopyFileEx())
                               FILE_ATTRIBUTE_NOT_CONTENT_INDEXED; //
    //FILE_ATTRIBUTE_ENCRYPTED -> no!

    //create targetFile and open it for writing
    HANDLE hFileTarget = ::CreateFile(applyLongPathPrefix(targetFile).c_str(), //_In_      LPCTSTR lpFileName,
                                      GENERIC_READ | GENERIC_WRITE,            //_In_      DWORD dwDesiredAccess,
                                      //read access required for FSCTL_SET_COMPRESSION
                                      FILE_SHARE_DELETE,                       //_In_      DWORD dwShareMode,
                                      //FILE_SHARE_DELETE is required to rename file while handle is open!
                                      nullptr,                                 //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                      CREATE_NEW,                              //_In_      DWORD dwCreationDisposition,
                                      //FILE_FLAG_OVERLAPPED must not be used! FILE_FLAG_NO_BUFFERING should not be used!
                                      (fileInfoSource.dwFileAttributes & validAttribs) |
                                      FILE_FLAG_SEQUENTIAL_SCAN |
                                      FILE_FLAG_BACKUP_SEMANTICS,              //_In_      DWORD dwFlagsAndAttributes,
                                      nullptr);                                //_In_opt_  HANDLE hTemplateFile
    if (hFileTarget == INVALID_HANDLE_VALUE)
    {
        const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile));
        const std::wstring errorDescr = formatSystemError(L"CreateFile", ec);

        if (ec == ERROR_FILE_EXISTS || //confirmed to be used
            ec == ERROR_ALREADY_EXISTS) //comment on msdn claims, this one is used on Windows Mobile 6
            throw ErrorTargetExisting(errorMsg, errorDescr);

        //if (ec == ERROR_PATH_NOT_FOUND) throw ErrorTargetPathMissing(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    ZEN_ON_SCOPE_FAIL(try { removeFile(targetFile); }
    catch (FileError&) {} );   //transactional behavior: guard just after opening target and before managing hFileTarget
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFileTarget));

    //----------------------------------------------------------------------
    BY_HANDLE_FILE_INFORMATION fileInfoTarget = {};
    if (!::GetFileInformationByHandle(hFileTarget, &fileInfoTarget))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(targetFile)), L"GetFileInformationByHandle");

    //return up-to-date file attributes
    InSyncAttributes newAttrib;
    newAttrib.fileSize         = get64BitUInt(fileInfoSource.nFileSizeLow, fileInfoSource.nFileSizeHigh);
    newAttrib.modificationTime = filetimeToTimeT(fileInfoSource.ftLastWriteTime); //no DST hack (yet)
    newAttrib.sourceFileId     = extractFileId(fileInfoSource);
    newAttrib.targetFileId     = extractFileId(fileInfoTarget);

    //#################### copy NTFS compressed attribute #########################
    const bool sourceIsCompressed = (fileInfoSource.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;
    const bool targetIsCompressed = (fileInfoTarget.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0; //already set by CreateFile if target parent folder is compressed!
    if (sourceIsCompressed && !targetIsCompressed)
    {
        USHORT cmpState = COMPRESSION_FORMAT_DEFAULT;
        DWORD bytesReturned = 0;
        if (!::DeviceIoControl(hFileTarget,           //_In_         HANDLE hDevice,
                               FSCTL_SET_COMPRESSION, //_In_         DWORD dwIoControlCode,
                               &cmpState,             //_In_opt_     LPVOID lpInBuffer,
                               sizeof(cmpState),      //_In_         DWORD nInBufferSize,
                               nullptr,               //_Out_opt_    LPVOID lpOutBuffer,
                               0,                     //_In_         DWORD nOutBufferSize,
                               &bytesReturned,        //_Out_opt_    LPDWORD lpBytesReturned,
                               nullptr))              //_Inout_opt_  LPOVERLAPPED lpOverlapped
        {} //may legitimately fail with ERROR_INVALID_FUNCTION if:
        // - target folder is encrypted
        // - target volume does not support compressed attribute -> unlikely in this context
    }
    //#############################################################################

    //although it seems the sparse attribute is set automatically by BackupWrite, we are required to do this manually: http://support.microsoft.com/kb/271398/en-us
    //Quote: It is the responsibility of the backup utility to apply file attributes to a file after it is restored by using BackupWrite.
    //The application should retrieve the attributes by using GetFileAttributes prior to creating a backup with BackupRead.
    //If a file originally had the sparse attribute (FILE_ATTRIBUTE_SPARSE_FILE), the backup utility must explicitly set the
    //attribute on the restored file.

#ifdef ZEN_WIN_VISTA_AND_LATER
    if (canCopyAsSparse(fileInfoSource.dwFileAttributes, hFileTarget)) //throw ()
#else
    if (canCopyAsSparse(fileInfoSource.dwFileAttributes, targetFile)) //throw ()
#endif
    {
        DWORD bytesReturned = 0;
        if (!::DeviceIoControl(hFileTarget,      //_In_         HANDLE hDevice,
                               FSCTL_SET_SPARSE, //_In_         DWORD dwIoControlCode,
                               nullptr,          //_In_opt_     LPVOID lpInBuffer,
                               0,                //_In_         DWORD nInBufferSize,
                               nullptr,          //_Out_opt_    LPVOID lpOutBuffer,
                               0,                //_In_         DWORD nOutBufferSize,
                               &bytesReturned,   //_Out_opt_    LPDWORD lpBytesReturned,
                               nullptr))         //_Inout_opt_  LPOVERLAPPED lpOverlapped
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtPath(targetFile)), L"DeviceIoControl, FSCTL_SET_SPARSE");
    }

    //----------------------------------------------------------------------
    const DWORD BUFFER_SIZE = std::max(128 * 1024, static_cast<int>(sizeof(WIN32_STREAM_ID))); //must be greater than sizeof(WIN32_STREAM_ID)!
    std::vector<BYTE> buffer(BUFFER_SIZE);

    LPVOID contextRead  = nullptr; //manage context for BackupRead()/BackupWrite()
    LPVOID contextWrite = nullptr; //

    ZEN_ON_SCOPE_EXIT(
        if (contextRead ) ::BackupRead (0, nullptr, 0, nullptr, true, false, &contextRead); //MSDN: "lpContext must be passed [...] all other parameters are ignored."
        if (contextWrite) ::BackupWrite(0, nullptr, 0, nullptr, true, false, &contextWrite); );

    //stream-copy sourceFile to targetFile
    bool eof = false;
    bool someBytesRead = false; //try to detect failure reading encrypted files
    do
    {
        DWORD bytesRead = 0;
        if (!::BackupRead(hFileSource,   //__in   HANDLE hFile,
                          &buffer[0],    //__out  LPBYTE lpBuffer,
                          BUFFER_SIZE,   //__in   DWORD nNumberOfBytesToRead,
                          &bytesRead,    //__out  LPDWORD lpNumberOfBytesRead,
                          false,         //__in   BOOL bAbort,
                          false,         //__in   BOOL bProcessSecurity,
                          &contextRead)) //__out  LPVOID *lpContext
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), L"BackupRead"); //better use fine-granular error messages "reading/writing"!

        if (bytesRead > BUFFER_SIZE)
            throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), L"BackupRead: buffer overflow."); //user should never see this

        if (bytesRead < BUFFER_SIZE)
            eof = true;

        DWORD bytesWritten = 0;
        if (!::BackupWrite(hFileTarget,    //__in   HANDLE hFile,
                           &buffer[0],     //__in   LPBYTE lpBuffer,
                           bytesRead,      //__in   DWORD nNumberOfBytesToWrite,
                           &bytesWritten,  //__out  LPDWORD lpNumberOfBytesWritten,
                           false,          //__in   BOOL bAbort,
                           false,          //__in   BOOL bProcessSecurity,
                           &contextWrite)) //__out  LPVOID *lpContext
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile)), L"BackupWrite");

        if (bytesWritten != bytesRead)
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile)), L"BackupWrite: incomplete write."); //user should never see this

        //total bytes transferred may be larger than file size! context information + ADS or smaller (sparse, compressed)!
        if (onUpdateCopyStatus) onUpdateCopyStatus(bytesRead); //throw X!

        if (bytesRead > 0)
            someBytesRead = true;
    }
    while (!eof);

    //::BackupRead() silently fails reading encrypted files -> double check!
    if (!someBytesRead && get64BitUInt(fileInfoSource.nFileSizeLow, fileInfoSource.nFileSizeHigh) != 0U)
        //note: there is no guaranteed ordering relation beween bytes transferred and file size! Consider ADS (>) and compressed/sparse files (<)!
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), L"BackupRead: unknown error"); //user should never see this -> this method is called only if "canCopyAsSparse()"

    //time needs to be set at the end: BackupWrite() changes modification time
    if (!::SetFileTime(hFileTarget,
                       &fileInfoSource.ftCreationTime,
                       nullptr,
                       &fileInfoSource.ftLastWriteTime))
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(targetFile)), L"SetFileTime");

    return newAttrib;
}


DEFINE_NEW_FILE_ERROR(ErrorFallbackToCopyAsBackupStream);


struct CallbackData
{
    CallbackData(const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                 const Zstring& sourceFile,
                 const Zstring& targetFile) :
        sourceFile_(sourceFile),
        targetFile_(targetFile),
        onUpdateCopyStatus_(onUpdateCopyStatus) {}

    const Zstring& sourceFile_;
    const Zstring& targetFile_;
    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus_; //optional

    std::exception_ptr exception; //out
    BY_HANDLE_FILE_INFORMATION fileInfoSrc{}; //out: modified by CopyFileEx() at beginning
    BY_HANDLE_FILE_INFORMATION fileInfoTrg{}; //

    std::int64_t bytesReported = 0; //used internally to calculate bytes transferred delta
};


DWORD CALLBACK copyCallbackInternal(LARGE_INTEGER totalFileSize,
                                    LARGE_INTEGER totalBytesTransferred,
                                    LARGE_INTEGER streamSize,
                                    LARGE_INTEGER streamBytesTransferred,
                                    DWORD dwStreamNumber,
                                    DWORD dwCallbackReason,
                                    HANDLE hSourceFile,
                                    HANDLE hDestinationFile,
                                    LPVOID lpData)
{
    /*
    this callback is invoked for block sizes managed by Windows, these may vary from e.g. 64 kB up to 1MB. It seems this depends on file size amongst others.
    Note: for 0-sized files this callback is invoked just ONCE!

    symlink handling:
        if source is a symlink and COPY_FILE_COPY_SYMLINK is     specified, this callback is NOT invoked!
        if source is a symlink and COPY_FILE_COPY_SYMLINK is NOT specified, this callback is called and hSourceFile is a handle to the *target* of the link!

    file time handling:
        ::CopyFileEx() will (only) copy file modification time over from source file AFTER the last invokation of this callback
        => it is possible to adapt file creation time of target in here, but NOT file modification time!
        CAVEAT: if ::CopyFileEx() fails to set modification time, it silently ignores this error and returns success!!! (confirmed with Process Monitor)

    alternate data stream handling:
        CopyFileEx() processes multiple streams one after another, stream 1 is the file data stream and always available!
        Each stream is initialized with CALLBACK_STREAM_SWITCH and provides *new* hSourceFile, hDestinationFile.
        Calling GetFileInformationByHandle() on hDestinationFile for stream > 1 results in ERROR_ACCESS_DENIED!
        totalBytesTransferred contains size of *all* streams and so can be larger than the "file size" file attribute
    */

    CallbackData& cbd = *static_cast<CallbackData*>(lpData);

    try
    {
        if (dwCallbackReason == CALLBACK_STREAM_SWITCH &&  //called up-front for every file (even if 0-sized)
            dwStreamNumber == 1) //consider ADS!
        {
            //#################### return source file attributes ################################
            if (!::GetFileInformationByHandle(hSourceFile, &cbd.fileInfoSrc))
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(cbd.sourceFile_)), L"GetFileInformationByHandle");

            if (!::GetFileInformationByHandle(hDestinationFile, &cbd.fileInfoTrg))
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(cbd.targetFile_)), L"GetFileInformationByHandle");

            //#################### switch to sparse file copy if req. #######################
#ifdef ZEN_WIN_VISTA_AND_LATER
            if (canCopyAsSparse(cbd.fileInfoSrc.dwFileAttributes, hDestinationFile)) //throw ()
#else
            if (canCopyAsSparse(cbd.fileInfoSrc.dwFileAttributes, cbd.targetFile_)) //throw ()
#endif
                throw ErrorFallbackToCopyAsBackupStream(L"sparse, callback"); //use a different copy routine!

            //#################### copy file creation time ################################
            ::SetFileTime(hDestinationFile, &cbd.fileInfoSrc.ftCreationTime, nullptr, nullptr); //no error handling!
            //=> not really needed here, creation time is set anyway at the end of copyFileWindowsDefault()!

            //#################### copy NTFS compressed attribute #########################
            const bool sourceIsCompressed = (cbd.fileInfoSrc.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;
            const bool targetIsCompressed = (cbd.fileInfoTrg.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0; //already set by CopyFileEx if target parent folder is compressed!
            if (sourceIsCompressed && !targetIsCompressed)
            {
                USHORT cmpState = COMPRESSION_FORMAT_DEFAULT;
                DWORD bytesReturned = 0;
                if (!::DeviceIoControl(hDestinationFile,      //_In_         HANDLE hDevice,
                                       FSCTL_SET_COMPRESSION, //_In_         DWORD dwIoControlCode,
                                       &cmpState,             //_In_opt_     LPVOID lpInBuffer,
                                       sizeof(cmpState),      //_In_         DWORD nInBufferSize,
                                       nullptr,               //_Out_opt_    LPVOID lpOutBuffer,
                                       0,                     //_In_         DWORD nOutBufferSize,
                                       &bytesReturned,        //_Out_opt_    LPDWORD lpBytesReturned
                                       nullptr))              //_Inout_opt_  LPOVERLAPPED lpOverlapped
                {} //may legitimately fail with ERROR_INVALID_FUNCTION if

                // - if target folder is encrypted
                // - target volume does not support compressed attribute
                //#############################################################################
            }
        }

        if (cbd.onUpdateCopyStatus_ && totalBytesTransferred.QuadPart >= 0) //should always be true, but let's still check
        {
            cbd.onUpdateCopyStatus_(totalBytesTransferred.QuadPart - cbd.bytesReported); //throw X!
            cbd.bytesReported = totalBytesTransferred.QuadPart;
        }
    }
    catch (...)
    {
        cbd.exception = std::current_exception();
        return PROGRESS_CANCEL;
    }
    return PROGRESS_CONTINUE;
}


InSyncAttributes copyFileWindowsDefault(const Zstring& sourceFile, //throw FileError, ErrorTargetExisting, ErrorFileLocked, ErrorFallbackToCopyAsBackupStream
                                        const Zstring& targetFile,
                                        const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
#ifdef TODO_MinFFS_activatePrivilege
    //try to get backup read and write privileges: may help solve some "access denied" errors
    bool backupPrivilegesActive = true;
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) { backupPrivilegesActive = false; }
    try { activatePrivilege(SE_RESTORE_NAME); }
    catch (const FileError&) { backupPrivilegesActive = false; }
#endif//TODO_MinFFS_activatePrivilege

    auto guardTarget = zen::makeGuard<ScopeGuardRunMode::ON_FAIL>([&] { try { removeFile(targetFile); } catch (FileError&) {} });
    //transactional behavior: guard just before starting copy, we don't trust ::CopyFileEx(), do we? ;)

    DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;

    //encrypted destination is not supported with Windows 2000! -> whatever
    copyFlags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION; //allow copying from encrypted to non-encrypted location

    //if (vistaOrLater()) //see http://blogs.technet.com/b/askperf/archive/2007/05/08/slow-large-file-copy-issues.aspx
    //  copyFlags |= COPY_FILE_NO_BUFFERING; //no perf difference at worst, improvement for large files (20% in test NTFS -> NTFS)
    // - this flag may cause file corruption! https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/65f48357/
    // - documentation on CopyFile2() even states: "It is not recommended to pause copies that are using this flag."
    //=> it's not worth it! instead of skipping buffering at kernel-level (=> also NO prefetching!!!), skip it at user-level: memory mapped files!
    //   however, perf-measurements for memory mapped files show: it's also not worth it!

    CallbackData cbd(onUpdateCopyStatus, sourceFile, targetFile);

    const bool success = ::CopyFileEx(applyLongPathPrefix(sourceFile).c_str(), //__in      LPCTSTR lpExistingFileName,
                                      applyLongPathPrefix(targetFile).c_str(), //__in      LPCTSTR lpNewFileName,
                                      copyCallbackInternal, //__in_opt  LPPROGRESS_ROUTINE lpProgressRoutine,
                                      &cbd,                 //__in_opt  LPVOID lpData,
                                      nullptr,              //__in_opt  LPBOOL pbCancel,
                                      copyFlags) != FALSE;  //__in      DWORD dwCopyFlags
    if (cbd.exception)
        std::rethrow_exception(cbd.exception); //throw ?, process errors in callback first!

    if (!success)
    {
        const DWORD ec = ::GetLastError(); //copy before directly/indirectly making other system calls!

        //don't suppress "lastError == ERROR_REQUEST_ABORTED": a user aborted operation IS an error condition!

        //trying to copy huge sparse files may directly fail with ERROR_DISK_FULL before entering the callback function
        if (canCopyAsSparse(sourceFile, targetFile)) //noexcept
            throw ErrorFallbackToCopyAsBackupStream(L"sparse, copy failure");

#ifdef TODO_MinFFS_activatePrivilege
        if (ec == ERROR_ACCESS_DENIED && backupPrivilegesActive)
#else//TODO_MinFFS_activatePrivilege
        if (ec == ERROR_ACCESS_DENIED)
#endif//TODO_MinFFS_activatePrivilege
            //chances are good this will work with copyFileWindowsBackupStream: https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/1998ebf2/
            throw ErrorFallbackToCopyAsBackupStream(L"access denied");

        //copying ADS may incorrectly fail with ERROR_FILE_NOT_FOUND: https://sourceforge.net/p/freefilesync/discussion/help/thread/a18a2c02/
        if (ec == ERROR_FILE_NOT_FOUND &&
            cbd.fileInfoSrc.nNumberOfLinks > 0 &&
            cbd.fileInfoTrg.nNumberOfLinks > 0)
            throw ErrorFallbackToCopyAsBackupStream(L"bogus file not found");

        //assemble error message...
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot copy file %x to %y."), L"%x", L"\n" + fmtPath(sourceFile)), L"%y", L"\n" + fmtPath(targetFile));
        std::wstring errorDescr = formatSystemError(L"CopyFileEx", ec);

        //if file is locked throw "ErrorFileLocked" instead!
        if (ec == ERROR_SHARING_VIOLATION ||
            ec == ERROR_LOCK_VIOLATION)
        {
#ifdef ZEN_WIN_VISTA_AND_LATER //(try to) enhance error message
            const std::wstring procList = vista::getLockingProcesses(sourceFile); //noexcept
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
#endif
            throw ErrorFileLocked(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), errorDescr);
        }

        //if target is existing this functions is expected to throw ErrorTargetExisting!!!
        if (ec == ERROR_FILE_EXISTS || //confirmed to be used
            ec == ERROR_ALREADY_EXISTS) //not sure if used -> better be safe than sorry!!!
        {
            guardTarget.dismiss(); //don't delete file that existed previously!
            throw ErrorTargetExisting(errorMsg, errorDescr);
        }

        //if (lastError == ERROR_PATH_NOT_FOUND) throw ErrorTargetPathMissing(errorMsg, errorDescr); //could this also be source path missing!?

        try //add more meaningful message
        {
            //trying to copy > 4GB file to FAT/FAT32 volume gives obscure ERROR_INVALID_PARAMETER (FAT can indeed handle files up to 4 Gig, tested!)
            if (ec == ERROR_INVALID_PARAMETER &&
                isFatDrive(targetFile) &&
                getFilesize(sourceFile) >= 4U * std::uint64_t(1024U * 1024 * 1024)) //throw FileError
                errorDescr += L"\nFAT volumes cannot store files larger than 4 gigabytes.";
            //see "Limitations of the FAT32 File System": http://support.microsoft.com/kb/314463/en-us

            //note: ERROR_INVALID_PARAMETER can also occur when copying to a SharePoint server or MS SkyDrive and the target file path is of a restricted type.
        }
        catch (FileError&) {}

        throw FileError(errorMsg, errorDescr);
    }

    //caveat: - ::CopyFileEx() silently *ignores* failure to set modification time!!! => we always need to set it again but with proper error checking!
    //        - perf: recent measurements show no slow down at all for buffered USB sticks!
    setFileTimeRaw(targetFile, &cbd.fileInfoSrc.ftCreationTime, cbd.fileInfoSrc.ftLastWriteTime, ProcSymlink::FOLLOW); //throw FileError

    InSyncAttributes newAttrib;
    newAttrib.fileSize         = get64BitUInt(cbd.fileInfoSrc.nFileSizeLow, cbd.fileInfoSrc.nFileSizeHigh);
    newAttrib.modificationTime = filetimeToTimeT(cbd.fileInfoSrc.ftLastWriteTime);
    newAttrib.sourceFileId     = extractFileId(cbd.fileInfoSrc);
    newAttrib.targetFileId     = extractFileId(cbd.fileInfoTrg);
    return newAttrib;
}


//another layer to support copying sparse files and handle some access denied errors
inline
InSyncAttributes copyFileWindowsSelectRoutine(const Zstring& sourceFile, const Zstring& targetFile, const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    try
    {
        return copyFileWindowsDefault(sourceFile, targetFile, onUpdateCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked, ErrorFallbackToCopyAsBackupStream
    }
    catch (ErrorFallbackToCopyAsBackupStream&)
    {
        return copyFileWindowsBackupStream(sourceFile, targetFile, onUpdateCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
}


//another layer of indirection solving 8.3 name clashes
inline
InSyncAttributes copyFileOsSpecific(const Zstring& sourceFile,
                                    const Zstring& targetFile,
                                    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    try
    {
        return copyFileWindowsSelectRoutine(sourceFile, targetFile, onUpdateCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
    catch (const ErrorTargetExisting&)
    {
        //try to handle issues with already existing short 8.3 file names on Windows
        if (have8dot3NameClash(targetFile))
        {
            Fix8Dot3NameClash dummy(targetFile); //throw FileError; move clashing file path to the side
            return copyFileWindowsSelectRoutine(sourceFile, targetFile, onUpdateCopyStatus); //throw FileError; the short file path name clash is solved, this should work now
        }
        throw;
    }
}


#elif defined ZEN_LINUX || defined ZEN_MAC
InSyncAttributes copyFileOsSpecific(const Zstring& sourceFile, //throw FileError, ErrorTargetExisting
                                    const Zstring& targetFile,
                                    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    FileInput fileIn(sourceFile); //throw FileError

    struct ::stat sourceInfo = {};
    if (::fstat(fileIn.getHandle(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceFile)), L"fstat");

    const int fdTarget = ::open(targetFile.c_str(), O_WRONLY | O_CREAT | O_EXCL,
                                sourceInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)); //analog to "cp" which copies "mode" (considering umask) by default
    //=> need copyItemPermissions() only for "chown" and umask-agnostic permissions
    if (fdTarget == -1)
    {
        const int ec = errno; //copy before making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile));
        const std::wstring errorDescr = formatSystemError(L"open", ec);

        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    if (onUpdateCopyStatus) onUpdateCopyStatus(0); //throw X!

    InSyncAttributes newAttrib;
    ZEN_ON_SCOPE_FAIL( try { removeFile(targetFile); }
    catch (FileError&) {} );
    //transactional behavior: place guard after ::open() and before lifetime of FileOutput:
    //=> don't delete file that existed previously!!!
    {
        FileOutput fileOut(fdTarget, targetFile); //pass ownership
        if (onUpdateCopyStatus) onUpdateCopyStatus(0); //throw X!

        copyStream(fileIn, fileOut, std::min(fileIn .optimalBlockSize(),
                                             fileOut.optimalBlockSize()), onUpdateCopyStatus); //throw FileError, X

        struct ::stat targetInfo = {};
        if (::fstat(fileOut.getHandle(), &targetInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(targetFile)), L"fstat");

        newAttrib.fileSize         = sourceInfo.st_size;
#ifdef  ZEN_MAC
        newAttrib.modificationTime = sourceInfo.st_mtimespec.tv_sec; //use same time variable like setFileTimeRaw() for consistency
#else
        newAttrib.modificationTime = sourceInfo.st_mtime;
#endif
        newAttrib.sourceFileId     = extractFileId(sourceInfo);
        newAttrib.targetFileId     = extractFileId(targetInfo);

#ifdef ZEN_MAC
        //using ::copyfile with COPYFILE_DATA seems to trigger bugs unlike our stream-based copying!
        //=> use ::copyfile for extended attributes only: https://sourceforge.net/p/freefilesync/discussion/help/thread/91384c8a/
        //http://blog.plasticsfuture.org/2006/03/05/the-state-of-backup-and-cloning-tools-under-mac-os-x/
        //docs:   http://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/copyfile.3.html
        //source: http://www.opensource.apple.com/source/copyfile/copyfile-103.92.1/copyfile.c
        if (::fcopyfile(fileIn.getHandle(), fileOut.getHandle(), 0, COPYFILE_XATTR) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(replaceCpy(_("Cannot copy attributes from %x to %y."), L"%x", L"\n" + fmtPath(sourceFile)), L"%y", L"\n" + fmtPath(targetFile)), L"copyfile");
#endif

        fileOut.close(); //throw FileError -> optional, but good place to catch errors when closing stream!
    } //close output file handle before setting file time

    //we cannot set the target file times (::futimes) while the file descriptor is still open after a write operation:
    //this triggers bugs on samba shares where the modification time is set to current time instead.
    //Linux: http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=340236
    //       http://comments.gmane.org/gmane.linux.file-systems.cifs/2854
    //OS X:  https://sourceforge.net/p/freefilesync/discussion/help/thread/881357c0/
#ifdef ZEN_MAC
    setFileTimeRaw(targetFile, &sourceInfo.st_birthtimespec, sourceInfo.st_mtimespec, ProcSymlink::FOLLOW); //throw FileError
    //sourceInfo.st_birthtime; -> only seconds-precision
    //sourceInfo.st_mtime;     ->
#else
    setFileTime(targetFile, sourceInfo.st_mtime, ProcSymlink::FOLLOW); //throw FileError
#endif

    return newAttrib;
}
#endif

/*
               ------------------
               |File Copy Layers|
               ------------------
                  copyNewFile
                        |
               copyFileOsSpecific (solve 8.3 issue on Windows)
                        |
              copyFileWindowsSelectRoutine
              /                           \
copyFileWindowsDefault(::CopyFileEx)  copyFileWindowsBackupStream(::BackupRead/::BackupWrite)
*/
}


InSyncAttributes zen::copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                  const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    const InSyncAttributes attr = copyFileOsSpecific(sourceFile, targetFile, onUpdateCopyStatus); //throw FileError, ErrorTargetExisting, ErrorFileLocked

    //at this point we know we created a new file, so it's fine to delete it for cleanup!
    ZEN_ON_SCOPE_FAIL(try { removeFile(targetFile); }
    catch (FileError&) {});

    if (copyFilePermissions)
        copyItemPermissions(sourceFile, targetFile, ProcSymlink::FOLLOW); //throw FileError

    return attr;
}
