// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "file_access.h"
#include <map>
#include <algorithm>
#include <stdexcept>
#include "int64.h"
#include "file_traverser.h"
#include "scope_guard.h"
#include "symlink_target.h"
#include "file_io.h"
#include "file_id_def.h"

#ifdef ZEN_WIN
    #include <Aclapi.h>
    #include "privilege.h"
    #include "dll.h"
    #include "long_path_prefix.h"
    #include "win_ver.h"
    #include "IFileOperation/file_op.h"

#elif defined ZEN_LINUX
    #include <sys/vfs.h> //statfs
    #include <fcntl.h> //AT_SYMLINK_NOFOLLOW, UTIME_OMIT
    #ifdef HAVE_SELINUX
        #include <selinux/selinux.h>
    #endif

#elif defined ZEN_MAC
    #include <sys/mount.h> //statfs
    #include <copyfile.h>
#endif

#if defined ZEN_LINUX || defined ZEN_MAC
    #include <sys/stat.h>
    #include <sys/time.h> //lutimes
#endif

using namespace zen;


bool zen::fileExists(const Zstring& filepath)
{
    //symbolic links (broken or not) are also treated as existing files!
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(filepath).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0; //returns true for (file-)symlinks also

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::stat(filepath.c_str(), &fileInfo) == 0) //follow symlinks!
        return S_ISREG(fileInfo.st_mode);
#endif
    return false;
}


bool zen::dirExists(const Zstring& dirpath)
{
    //symbolic links (broken or not) are also treated as existing directories!
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(dirpath).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0; //returns true for (dir-)symlinks also

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat dirInfo = {};
    if (::stat(dirpath.c_str(), &dirInfo) == 0) //follow symlinks!
        return S_ISDIR(dirInfo.st_mode);
#endif
    return false;
}


bool zen::symlinkExists(const Zstring& linkname)
{
#ifdef ZEN_WIN
    WIN32_FIND_DATA linkInfo = {};
    const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(linkname).c_str(), &linkInfo);
    if (searchHandle != INVALID_HANDLE_VALUE)
    {
        ::FindClose(searchHandle);
        return isSymlink(linkInfo);
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat linkInfo = {};
    if (::lstat(linkname.c_str(), &linkInfo) == 0)
        return S_ISLNK(linkInfo.st_mode);
#endif
    return false;
}


bool zen::somethingExists(const Zstring& objname)
{
#ifdef ZEN_WIN
    const DWORD attr = ::GetFileAttributes(applyLongPathPrefix(objname).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return true;
    const DWORD lastError = ::GetLastError();

    //handle obscure file permission problem where ::GetFileAttributes() fails with ERROR_ACCESS_DENIED or ERROR_SHARING_VIOLATION
    //while parent directory traversal is successful: e.g. "C:\pagefile.sys"
    if (lastError != ERROR_PATH_NOT_FOUND && //perf: short circuit for common "not existing" error codes
        lastError != ERROR_FILE_NOT_FOUND && //
        lastError != ERROR_BAD_NETPATH    && //
        lastError != ERROR_BAD_NET_NAME)     //
    {
        WIN32_FIND_DATA fileInfo = {};
        const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(objname).c_str(), &fileInfo);
        if (searchHandle != INVALID_HANDLE_VALUE)
        {
            ::FindClose(searchHandle);
            return true;
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::lstat(objname.c_str(), &fileInfo) == 0)
        return true;
#endif
    return false;
}


namespace
{
#ifdef ZEN_WIN
//fast ::GetVolumePathName() clone: let's hope it's not too simple (doesn't honor mount points)
Zstring getVolumeNameFast(const Zstring& filepath)
{
    //this call is expensive: ~1.5 ms!
    //    if (!::GetVolumePathName(filepath.c_str(), //__in   LPCTSTR lpszFileName,
    //                             fsName,           //__out  LPTSTR lpszVolumePathName,
    //                             BUFFER_SIZE))     //__in   DWORD cchBufferLength
    // ...
    //    Zstring volumePath = appendSeparator(fsName);

    const Zstring nameFmt = appendSeparator(removeLongPathPrefix(filepath)); //throw()

    if (startsWith(nameFmt, Zstr("\\\\"))) //UNC path: "\\ComputerName\SharedFolder\"
    {
        size_t nameSize = nameFmt.size();
        const size_t posFirstSlash = nameFmt.find(Zstr("\\"), 2);
        if (posFirstSlash != Zstring::npos)
        {
            nameSize = posFirstSlash + 1;
            const size_t posSecondSlash = nameFmt.find(Zstr("\\"), posFirstSlash + 1);
            if (posSecondSlash != Zstring::npos)
                nameSize = posSecondSlash + 1;
        }
        return Zstring(nameFmt.c_str(), nameSize); //include trailing backslash!
    }
    else //local path: "C:\Folder\"
    {
        const size_t pos = nameFmt.find(Zstr(":\\"));
        if (pos == 1) //expect single letter volume
            return Zstring(nameFmt.c_str(), 3);
    }

    return Zstring();
}


bool isFatDrive(const Zstring& filepath) //throw()
{
    const Zstring volumePath = getVolumeNameFast(filepath);
    if (volumePath.empty())
        return false;

    const DWORD bufferSize = MAX_PATH + 1;
    wchar_t fsName[bufferSize] = {};

    //suprisingly fast: ca. 0.03 ms per call!
    if (!::GetVolumeInformation(appendSeparator(volumePath).c_str(), //__in_opt   LPCTSTR lpRootPathName,
                                nullptr,     //__out      LPTSTR lpVolumeNameBuffer,
                                0,           //__in       DWORD nVolumeNameSize,
                                nullptr,     //__out_opt  LPDWORD lpVolumeSerialNumber,
                                nullptr,     //__out_opt  LPDWORD lpMaximumComponentLength,
                                nullptr,     //__out_opt  LPDWORD lpFileSystemFlags,
                                fsName,      //__out      LPTSTR lpFileSystemNameBuffer,
                                bufferSize)) //__in       DWORD nFileSystemNameSize
    {
        assert(false); //shouldn't happen
        return false;
    }
    //DST hack seems to be working equally well for FAT and FAT32 (in particular creation time has 10^-2 s precision as advertised)
    return fsName == Zstring(L"FAT") ||
           fsName == Zstring(L"FAT32");
}


//(try to) enhance error messages by showing which processes lock the file
Zstring getLockingProcessNames(const Zstring& filepath) //throw(), empty string if none found or error occurred
{
    if (vistaOrLater())
    {
        using namespace fileop;
        const DllFun<FunType_getLockingProcesses> getLockingProcesses(getDllName(), funName_getLockingProcesses);
        const DllFun<FunType_freeString>          freeString         (getDllName(), funName_freeString);

        if (getLockingProcesses && freeString)
            if (const wchar_t* procList = getLockingProcesses(filepath.c_str()))
            {
                ZEN_ON_SCOPE_EXIT(freeString(procList));
                return procList;
            }
    }
    return Zstring();
}
#endif
}


std::uint64_t zen::getFilesize(const Zstring& filepath) //throw FileError
{
#ifdef ZEN_WIN
    {
        WIN32_FIND_DATA fileInfo = {};
        const HANDLE searchHandle = ::FindFirstFile(applyLongPathPrefix(filepath).c_str(), &fileInfo);
        if (searchHandle == INVALID_HANDLE_VALUE)
            throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"FindFirstFile", getLastError());
        ::FindClose(searchHandle);

        if (!isSymlink(fileInfo))
            return get64BitUInt(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);
    }
    //        WIN32_FILE_ATTRIBUTE_DATA sourceAttr = {};
    //        if (!::GetFileAttributesEx(applyLongPathPrefix(filepath).c_str(), //__in   LPCTSTR lpFileName,
    //                                   GetFileExInfoStandard,                 //__in   GET_FILEEX_INFO_LEVELS fInfoLevelId,
    //                                   &sourceAttr))                          //__out  LPVOID lpFileInformation

    //open handle to target of symbolic link
    const HANDLE hFile = ::CreateFile(applyLongPathPrefix(filepath).c_str(),                  //_In_      LPCTSTR lpFileName,
                                      0,                                                      //_In_      DWORD dwDesiredAccess,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, //_In_      DWORD dwShareMode,
                                      nullptr,                                                //_In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                      OPEN_EXISTING,                                          //_In_      DWORD dwCreationDisposition,
                                      FILE_FLAG_BACKUP_SEMANTICS, /*needed to open a directory*/ //_In_      DWORD dwFlagsAndAttributes,
                                      nullptr);                                               //_In_opt_  HANDLE hTemplateFile
    if (hFile == INVALID_HANDLE_VALUE)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"CreateFile", getLastError());
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFile));

    BY_HANDLE_FILE_INFORMATION fileInfoHnd = {};
    if (!::GetFileInformationByHandle(hFile, &fileInfoHnd))
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"GetFileInformationByHandle", getLastError());

    return get64BitUInt(fileInfoHnd.nFileSizeLow, fileInfoHnd.nFileSizeHigh);

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat fileInfo = {};
    if (::stat(filepath.c_str(), &fileInfo) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"stat", getLastError());

    return fileInfo.st_size;
#endif
}


std::uint64_t zen::getFreeDiskSpace(const Zstring& path) //throw FileError
{
#ifdef ZEN_WIN
    ULARGE_INTEGER bytesFree = {};
    if (!::GetDiskFreeSpaceEx(appendSeparator(path).c_str(), //__in_opt   LPCTSTR lpDirectoryName, -> "UNC name [...] must include a trailing backslash, for example, "\\MyServer\MyShare\"
                              &bytesFree,                    //__out_opt  PULARGE_INTEGER lpFreeBytesAvailable,
                              nullptr,                       //__out_opt  PULARGE_INTEGER lpTotalNumberOfBytes,
                              nullptr))                      //__out_opt  PULARGE_INTEGER lpTotalNumberOfFreeBytes
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(path)), L"GetDiskFreeSpaceEx", getLastError());

    return get64BitUInt(bytesFree.LowPart, bytesFree.HighPart);

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::statfs info = {};
    if (::statfs(path.c_str(), &info) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(path)), L"statfs", getLastError());

    return static_cast<std::uint64_t>(info.f_bsize) * info.f_bavail;
#endif
}


bool zen::removeFile(const Zstring& filepath) //throw FileError
{
#ifdef ZEN_WIN
    const wchar_t functionName[] = L"DeleteFile";
    const Zstring& filepathFmt = applyLongPathPrefix(filepath);
    if (!::DeleteFile(filepathFmt.c_str()))
#elif defined ZEN_LINUX || defined ZEN_MAC
    const wchar_t functionName[] = L"unlink";
    if (::unlink(filepath.c_str()) != 0)
#endif
    {
        ErrorCode lastError = getLastError();
#ifdef ZEN_WIN
        if (lastError == ERROR_ACCESS_DENIED) //function fails if file is read-only
        {
            ::SetFileAttributes(filepathFmt.c_str(), FILE_ATTRIBUTE_NORMAL); //(try to) normalize file attributes

            if (::DeleteFile(filepathFmt.c_str())) //now try again...
                return true;
            lastError = ::GetLastError();
        }
#endif
        if (!somethingExists(filepath)) //warning: changes global error code!!
            return false; //neither file nor any other object (e.g. broken symlink) with that name existing - caveat: what if "access is denied"!?!??!?!?

        //begin of "regular" error reporting
        const std::wstring errorMsg = replaceCpy(_("Cannot delete file %x."), L"%x", fmtFileName(filepath));
        std::wstring errorDescr = formatSystemError(functionName, lastError);

#ifdef ZEN_WIN
        if (lastError == ERROR_SHARING_VIOLATION || //-> enhance error message!
            lastError == ERROR_LOCK_VIOLATION)
        {
            const Zstring procList = getLockingProcessNames(filepath); //throw()
            if (!procList.empty())
                errorDescr =  _("The file is locked by another process:") + L"\n" + procList;
        }
#endif
        throw FileError(errorMsg, errorDescr);
    }
    return true;
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
void renameFile_sub(const Zstring& oldName, const Zstring& newName) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
#ifdef ZEN_WIN
    const Zstring oldNameFmt = applyLongPathPrefix(oldName);
    const Zstring newNameFmt = applyLongPathPrefix(newName);

    if (!::MoveFileEx(oldNameFmt.c_str(), //__in      LPCTSTR lpExistingFileName,
                      newNameFmt.c_str(), //__in_opt  LPCTSTR lpNewFileName,
                      0))                 //__in      DWORD dwFlags
    {
        DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!

        if (lastError == ERROR_ACCESS_DENIED) //MoveFileEx may fail to rename a read-only file on a SAMBA-share -> (try to) handle this
        {
            const DWORD oldAttr = ::GetFileAttributes(oldNameFmt.c_str());
            if (oldAttr != INVALID_FILE_ATTRIBUTES && (oldAttr & FILE_ATTRIBUTE_READONLY))
            {
                if (::SetFileAttributes(oldNameFmt.c_str(), FILE_ATTRIBUTE_NORMAL)) //remove readonly-attribute
                {
                    //try again...
                    if (::MoveFileEx(oldNameFmt.c_str(), //__in      LPCTSTR lpExistingFileName,
                                     newNameFmt.c_str(), //__in_opt  LPCTSTR lpNewFileName,
                                     0))                 //__in      DWORD dwFlags
                    {
                        //(try to) restore file attributes
                        ::SetFileAttributes(newNameFmt.c_str(), oldAttr); //don't handle error
                        return;
                    }
                    else
                    {
                        lastError = ::GetLastError(); //use error code from second call to ::MoveFileEx()
                        //cleanup: (try to) restore file attributes: assume oldName is still existing
                        ::SetFileAttributes(oldNameFmt.c_str(), oldAttr);
                    }
                }
            }
        }

        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x", L"\n" + fmtFileName(oldName)), L"%y", L"\n" + fmtFileName(newName));
        std::wstring errorDescr = formatSystemError(L"MoveFileEx", lastError);

        //try to enhance error message:
        if (lastError == ERROR_SHARING_VIOLATION ||
            lastError == ERROR_LOCK_VIOLATION)
        {
            const Zstring procList = getLockingProcessNames(oldName); //throw()
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
        }

        if (lastError == ERROR_NOT_SAME_DEVICE)
            throw ErrorDifferentVolume(errorMsg, errorDescr);
        else if (lastError == ERROR_ALREADY_EXISTS || //-> used on Win7 x64
                 lastError == ERROR_FILE_EXISTS)      //-> used by XP???
            throw ErrorTargetExisting(errorMsg, errorDescr);
        else
            throw FileError(errorMsg, errorDescr);
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
    if (::rename(oldName.c_str(), newName.c_str()) != 0)
    {
        const int lastError = errno; //copy before directly or indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x", L"\n" + fmtFileName(oldName)), L"%y", L"\n" + fmtFileName(newName));
        const std::wstring errorDescr = formatSystemError(L"rename", lastError);

        if (lastError == EXDEV)
            throw ErrorDifferentVolume(errorMsg, errorDescr);
        else if (lastError == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        else
            throw FileError(errorMsg, errorDescr);
    }
#endif
}


#ifdef ZEN_WIN
/*small wrapper around
::GetShortPathName()
::GetLongPathName() */
template <typename Function>
Zstring getFilenameFmt(const Zstring& filepath, Function fun) //throw(); returns empty string on error
{
    const Zstring filepathFmt = applyLongPathPrefix(filepath);

    const DWORD bufferSize = fun(filepathFmt.c_str(), nullptr, 0);
    if (bufferSize == 0)
        return Zstring();

    std::vector<wchar_t> buffer(bufferSize);

    const DWORD charsWritten = fun(filepathFmt.c_str(), //__in   LPCTSTR lpszShortPath,
                                   &buffer[0],          //__out  LPTSTR  lpszLongPath,
                                   bufferSize);         //__in   DWORD   cchBuffer
    if (charsWritten == 0 || charsWritten >= bufferSize)
        return Zstring();

    return &buffer[0];
}


Zstring findUnused8Dot3Name(const Zstring& filepath) //find a unique 8.3 short name
{
    const Zstring pathPrefix = contains(filepath, FILE_NAME_SEPARATOR) ?
                               (beforeLast(filepath, FILE_NAME_SEPARATOR) + FILE_NAME_SEPARATOR) : Zstring();

    Zstring extension = afterLast(afterLast(filepath, FILE_NAME_SEPARATOR), Zchar('.')); //extension needn't contain reasonable data
    if (extension.empty())
        extension = Zstr("FFS");
    else if (extension.length() > 3)
        extension.resize(3);

    for (int index = 0; index < 100000000; ++index) //filepath must be representable by <= 8 characters
    {
        const Zstring output = pathPrefix + numberTo<Zstring>(index) + Zchar('.') + extension;
        if (!somethingExists(output)) //ensure uniqueness
            return output;
    }
    throw std::runtime_error(std::string("100000000 files, one for each number, exist in this directory? You're kidding...") + utfCvrtTo<std::string>(pathPrefix));
}


bool have8dot3NameClash(const Zstring& filepath)
{
    if (!contains(filepath, FILE_NAME_SEPARATOR))
        return false;

    if (somethingExists(filepath)) //name OR directory!
    {
        const Zstring origName  = afterLast(filepath, FILE_NAME_SEPARATOR); //returns the whole string if ch not found
        const Zstring shortName = afterLast(getFilenameFmt(filepath, ::GetShortPathName), FILE_NAME_SEPARATOR); //throw() returns empty string on error
        const Zstring longName  = afterLast(getFilenameFmt(filepath, ::GetLongPathName ), FILE_NAME_SEPARATOR); //

        if (!shortName.empty() &&
            !longName .empty() &&
            EqualFilename()(origName,  shortName) &&
            !EqualFilename()(shortName, longName))
        {
            //for filepath short and long file name are equal and another unrelated file happens to have the same short name
            //e.g. filepath == "TESTWE~1", but another file is existing named "TestWeb" with short name ""TESTWE~1"
            return true;
        }
    }
    return false;
}

class Fix8Dot3NameClash //throw FileError
{
public:
    Fix8Dot3NameClash(const Zstring& filepath)
    {
        const Zstring longName = afterLast(getFilenameFmt(filepath, ::GetLongPathName), FILE_NAME_SEPARATOR); //throw() returns empty string on error

        unrelatedFile = beforeLast(filepath, FILE_NAME_SEPARATOR) + FILE_NAME_SEPARATOR + longName;

        //find another name in short format: this ensures the actual short name WILL be renamed as well!
        unrelatedFileParked = findUnused8Dot3Name(filepath);

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
void zen::renameFile(const Zstring& oldName, const Zstring& newName) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
    try
    {
        renameFile_sub(oldName, newName); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
    }
    catch (const ErrorTargetExisting&)
    {
#ifdef ZEN_WIN
        //try to handle issues with already existing short 8.3 file names on Windows
        if (have8dot3NameClash(newName))
        {
            Fix8Dot3NameClash dummy(newName); //throw FileError; move clashing filepath to the side
            //now try again...
            renameFile_sub(oldName, newName); //throw FileError
            return;
        }
#endif
        throw;
    }
}


namespace
{
class CollectFilesFlat : public zen::TraverseCallback
{
public:
    CollectFilesFlat(std::vector<Zstring>& files, std::vector<Zstring>& dirs) :
        files_(files),
        dirs_(dirs) {}

    void onFile(const Zchar* shortName, const Zstring& filepath, const FileInfo& details) override
    {
        files_.push_back(filepath);
    }
    HandleLink onSymlink(const Zchar* shortName, const Zstring& linkpath, const SymlinkInfo& details) override
    {
        if (dirExists(linkpath)) //dir symlink
            dirs_.push_back(linkpath);
        else //file symlink, broken symlink
            files_.push_back(linkpath);
        return LINK_SKIP;
    }
    TraverseCallback* onDir(const Zchar* shortName, const Zstring& dirpath) override
    {
        dirs_.push_back(dirpath);
        return nullptr; //DON'T traverse into subdirs; removeDirectory works recursively!
    }
    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                         override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zchar* shortName) override { throw FileError(msg); }

private:
    CollectFilesFlat           (const CollectFilesFlat&) = delete;
    CollectFilesFlat& operator=(const CollectFilesFlat&) = delete;

    std::vector<Zstring>& files_;
    std::vector<Zstring>& dirs_;
};


void removeDirectoryImpl(const Zstring& directory, //throw FileError
                         const std::function<void (const Zstring& filepath)>& onBeforeFileDeletion,
                         const std::function<void (const Zstring& dirpath )>& onBeforeDirDeletion)
{
    assert(somethingExists(directory)); //[!]

#ifdef ZEN_WIN
    const Zstring directoryFmt = applyLongPathPrefix(directory); //support for \\?\-prefix

    //(try to) normalize file attributes: actually NEEDED for symbolic links also!
    ::SetFileAttributes(directoryFmt.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif

    //attention: check if directory is a symlink! Do NOT traverse into it deleting contained files!!!
    if (symlinkExists(directory)) //remove symlink directly
    {
        if (onBeforeDirDeletion)
            onBeforeDirDeletion(directory); //once per symlink
#ifdef ZEN_WIN
        const wchar_t functionName[] = L"RemoveDirectory";
        if (!::RemoveDirectory(directoryFmt.c_str()))
#elif defined ZEN_LINUX || defined ZEN_MAC
        const wchar_t functionName[] = L"unlink";
        if (::unlink(directory.c_str()) != 0)
#endif
            throwFileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtFileName(directory)), functionName, getLastError());
    }
    else
    {
        std::vector<Zstring> fileList;
        std::vector<Zstring> dirList;
        {
            //get all files and directories from current directory (WITHOUT subdirectories!)
            CollectFilesFlat cff(fileList, dirList);
            traverseFolder(directory, cff); //don't follow symlinks
        }

        //delete directories recursively
        for (const Zstring& dirpath : dirList)
            removeDirectoryImpl(dirpath, onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError; call recursively to correctly handle symbolic links

        //delete files
        for (const Zstring& filepath : fileList)
        {
            if (onBeforeFileDeletion)
                onBeforeFileDeletion(filepath); //call once per file
            removeFile(filepath); //throw FileError
        }

        //parent directory is deleted last
        if (onBeforeDirDeletion)
            onBeforeDirDeletion(directory); //and once per folder
#ifdef ZEN_WIN
        const wchar_t functionName[] = L"RemoveDirectory";
        if (!::RemoveDirectory(directoryFmt.c_str()))
#elif defined ZEN_LINUX || defined ZEN_MAC
        const wchar_t functionName[] = L"rmdir";
        if (::rmdir(directory.c_str()) != 0)
#endif
            throwFileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtFileName(directory)), functionName, getLastError());
        //may spuriously fail with ERROR_DIR_NOT_EMPTY(145) even though all child items have
        //successfully been *marked* for deletion, but some application still has a handle open!
        //e.g. Open "C:\Test\Dir1\Dir2" (filled with lots of files) in Explorer, then delete "C:\Test\Dir1" via ::RemoveDirectory() => Error 145
        //Sample code: http://us.generation-nt.com/answer/createfile-directory-handles-removing-parent-help-29126332.html
    }
}


#ifdef ZEN_WIN
void setFileTimeRaw(const Zstring& filepath,
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
            ::SetFileAttributes(applyLongPathPrefix(filepath).c_str(), attribs);
        );

        auto removeReadonly = [&]() -> bool //throw FileError; may need to remove the readonly-attribute (e.g. on FAT usb drives)
        {
            if (attribs == INVALID_FILE_ATTRIBUTES)
            {
                const DWORD tmpAttr = ::GetFileAttributes(applyLongPathPrefix(filepath).c_str());
                if (tmpAttr == INVALID_FILE_ATTRIBUTES)
                    throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(filepath)), L"GetFileAttributes", getLastError());

                if (tmpAttr & FILE_ATTRIBUTE_READONLY)
                {
                    if (!::SetFileAttributes(applyLongPathPrefix(filepath).c_str(), FILE_ATTRIBUTE_NORMAL))
                        throwFileError(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtFileName(filepath)), L"SetFileAttributes", getLastError());

                    attribs = tmpAttr; //reapplied on scope exit
                    return true;
                }
            }
            return false;
        };

        auto openFile = [&](bool conservativeApproach)
        {
            return ::CreateFile(applyLongPathPrefix(filepath).c_str(), //_In_      LPCTSTR lpFileName,
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
                    const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
                    if (lastError == ERROR_ACCESS_DENIED)
                        if (removeReadonly()) //throw FileError
                            continue;

                    //3. after these herculean stunts we give up...
                    throwFileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(filepath)), L"CreateFile", lastError);
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
            ErrorCode lastError = getLastError(); //copy before directly or indirectly making other system calls!

            //function may fail if file is read-only: https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3514569&group_id=234430
            if (lastError == ERROR_ACCESS_DENIED)
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
                            throwFileError(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtFileName(filepath)), L"SetFileInformationByHandle", getLastError());
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

                            lastError = ERROR_SUCCESS;
                        }
                }
            }

            std::wstring errorMsg = replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(filepath));

            //add more meaningful message: FAT accepts only a subset of the NTFS date range
            if (lastError == ERROR_INVALID_PARAMETER &&
                isFatDrive(filepath))
            {
                //we need a low-level reliable routine to format a potentially invalid date => don't use strftime!!!
                auto fmtDate = [](const FILETIME& ft) -> Zstring
                {
                    SYSTEMTIME st = {};
                    if (!::FileTimeToSystemTime(&ft,  //__in   const FILETIME *lpFileTime,
                                                &st)) //__out  LPSYSTEMTIME lpSystemTime
                        return Zstring();

                    Zstring dateTime;
                    {
                        const int bufferSize = ::GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, nullptr, 0);
                        if (bufferSize > 0)
                        {
                            std::vector<wchar_t> buffer(bufferSize);
                            if (::GetDateFormat(LOCALE_USER_DEFAULT, //_In_       LCID Locale,
                            0,                   //_In_       DWORD dwFlags,
                            &st,				  //_In_opt_   const SYSTEMTIME *lpDate,
                            nullptr,			  //_In_opt_   LPCTSTR lpFormat,
                            &buffer[0],		  //_Out_opt_  LPTSTR lpDateStr,
                            bufferSize) > 0)		  //_In_       int cchDate
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

            if (lastError != ERROR_SUCCESS)
                throwFileError(errorMsg, L"SetFileTime", lastError);
        }
    }
#ifndef NDEBUG //verify written data: mainly required to check consistency of DST hack
    FILETIME creationTimeDbg  = {};
    FILETIME lastWriteTimeDbg = {};

    HANDLE hFile = ::CreateFile(applyLongPathPrefix(filepath).c_str(),                  //_In_      LPCTSTR lpFileName,
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
#endif
}


void zen::removeDirectory(const Zstring& directory, //throw FileError
                          const std::function<void (const Zstring& filepath)>& onBeforeFileDeletion,
                          const std::function<void (const Zstring& dirpath)>&  onBeforeDirDeletion)
{
    //no error situation if directory is not existing! manual deletion relies on it!
    if (!somethingExists(directory))
        return; //neither directory nor any other object (e.g. broken symlink) with that name existing
    removeDirectoryImpl(directory, onBeforeFileDeletion, onBeforeDirDeletion);
}


void zen::setFileTime(const Zstring& filepath, std::int64_t modTime, ProcSymlink procSl) //throw FileError
{
#ifdef ZEN_WIN
    setFileTimeRaw(filepath, nullptr, timetToFileTime(modTime), procSl); //throw FileError

#elif defined ZEN_LINUX || defined ZEN_MAC
    //sigh, we can't use utimensat on NTFS volumes on Ubuntu: silent failure!!! what morons are programming this shit???

    //    struct ::timespec newTimes[2] = {};
    //    newTimes[0].tv_nsec = UTIME_OMIT; //omit access time
    //    newTimes[1].tv_sec = to<time_t>(modTime); //modification time (seconds)
    //
    //    if (::utimensat(AT_FDCWD, filepath.c_str(), newTimes, procSl == SYMLINK_DIRECT ? AT_SYMLINK_NOFOLLOW : 0) != 0)
    //        throwFileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(filepath)), L"utimensat", getLastError());

    //=> fallback to "retarded-idiot version"! -- DarkByte

    //OS X: utime() is obsoleted by utimes()! utimensat() not yet implemented

    struct ::timeval newTimes[2] = {};
    newTimes[0].tv_sec = ::time(nullptr); //access time (seconds)
    newTimes[1].tv_sec = modTime;         //modification time (seconds)

    const int rv = procSl == ProcSymlink::FOLLOW ?
                   :: utimes(filepath.c_str(), newTimes) :
                   ::lutimes(filepath.c_str(), newTimes);
    if (rv != 0)
        throwFileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(filepath)), L"utimes", getLastError());
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
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(dirpath)), L"GetVolumePathName", getLastError());

    DWORD fsFlags = 0;
    if (!::GetVolumeInformation(&buffer[0], //__in_opt   LPCTSTR lpRootPathName,
                                nullptr,    //__out      LPTSTR  lpVolumeNameBuffer,
                                0,          //__in       DWORD   nVolumeNameSize,
                                nullptr,    //__out_opt  LPDWORD lpVolumeSerialNumber,
                                nullptr,    //__out_opt  LPDWORD lpMaximumComponentLength,
                                &fsFlags,   //__out_opt  LPDWORD lpFileSystemFlags,
                                nullptr,    //__out      LPTSTR  lpFileSystemNameBuffer,
                                0))         //__in       DWORD   nFileSystemNameSize
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(dirpath)), L"GetVolumeInformation", getLastError());

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

        throwFileError(replaceCpy(_("Cannot read security context of %x."), L"%x", fmtFileName(source)), L"getfilecon", getLastError());
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
        throwFileError(replaceCpy(_("Cannot write security context of %x."), L"%x", fmtFileName(target)), L"setfilecon", getLastError());
}
#endif //HAVE_SELINUX


//copy permissions for files, directories or symbolic links: requires admin rights
void copyObjectPermissions(const Zstring& source, const Zstring& target, ProcSymlink procSl) //throw FileError
{
#ifdef ZEN_WIN
    //in contrast to ::SetSecurityInfo(), ::SetFileSecurity() seems to honor the "inherit DACL/SACL" flags
    //CAVEAT: if a file system does not support ACLs, GetFileSecurity() will return successfully with a *valid* security descriptor containing *no* ACL entries!

    //NOTE: ::GetFileSecurity()/::SetFileSecurity() do NOT follow Symlinks! getResolvedFilePath() requires Vista or later!
    const Zstring sourceResolved = procSl == ProcSymlink::FOLLOW && symlinkExists(source) ? getResolvedFilePath(source) : source; //throw FileError
    const Zstring targetResolved = procSl == ProcSymlink::FOLLOW && symlinkExists(target) ? getResolvedFilePath(target) : target; //

    //setting privileges requires admin rights!
    try
    {
        //enable privilege: required to read/write SACL information (only)
        activatePrivilege(SE_SECURITY_NAME); //throw FileError
        //Note: trying to copy SACL (SACL_SECURITY_INFORMATION) may return ERROR_PRIVILEGE_NOT_HELD (1314) on Samba shares. This is not due to missing privileges!
        //However, this is okay, since copying NTFS permissions doesn't make sense in this case anyway

        //enable privilege: required to copy owner information
        activatePrivilege(SE_RESTORE_NAME); //throw FileError

        //the following privilege may be required according to http://msdn.microsoft.com/en-us/library/aa364399(VS.85).aspx (although not needed nor active in my tests)
        activatePrivilege(SE_BACKUP_NAME); //throw FileError
    }
    catch (const FileError& e)//add some more context description (e.g. user is not an admin)
    {
        throw FileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(sourceResolved)), e.toString());
    }


    std::vector<char> buffer(10000); //example of actually required buffer size: 192 bytes
    for (;;)
    {
        DWORD bytesNeeded = 0;
        if (::GetFileSecurity(applyLongPathPrefix(sourceResolved).c_str(), //__in LPCTSTR lpFileName, -> long path prefix IS needed, although it is NOT mentioned on MSDN!!!
                              OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                              DACL_SECURITY_INFORMATION  | SACL_SECURITY_INFORMATION, //__in       SECURITY_INFORMATION RequestedInformation,
                              reinterpret_cast<PSECURITY_DESCRIPTOR>(&buffer[0]),     //__out_opt  PSECURITY_DESCRIPTOR pSecurityDescriptor,
                              static_cast<DWORD>(buffer.size()), //__in       DWORD nLength,
                              &bytesNeeded))                     //__out      LPDWORD lpnLengthNeeded
            break;
        //failure: ...
        if (bytesNeeded > buffer.size())
            buffer.resize(bytesNeeded);
        else
            throwFileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(sourceResolved)), L"GetFileSecurity", getLastError());
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
        throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(targetResolved)), L"SetFileSecurity", getLastError());

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
    copySecurityContext(source, target, procSl); //throw FileError
#endif

    struct ::stat fileInfo = {};
    if (procSl == ProcSymlink::FOLLOW)
    {
        if (::stat(source.c_str(), &fileInfo) != 0)
            throwFileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(source)), L"stat", getLastError());

        if (::chown(target.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"chown", getLastError());

        if (::chmod(target.c_str(), fileInfo.st_mode) != 0)
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"chmod", getLastError());
    }
    else
    {
        if (::lstat(source.c_str(), &fileInfo) != 0)
            throwFileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(source)), L"lstat", getLastError());

        if (::lchown(target.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"lchown", getLastError());

        if (!symlinkExists(target) && //setting access permissions doesn't make sense for symlinks on Linux: there is no lchmod()
            ::chmod(target.c_str(), fileInfo.st_mode) != 0)
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"chmod", getLastError());
    }

#elif defined ZEN_MAC
    copyfile_flags_t flags = COPYFILE_ACL | COPYFILE_STAT; //unfortunately COPYFILE_STAT copies modtime, too!
    if (procSl == ProcSymlink::DIRECT)
        flags |= COPYFILE_NOFOLLOW;

    if (::copyfile(source.c_str(), target.c_str(), 0, flags) != 0)
        throwFileError(replaceCpy(replaceCpy(_("Cannot copy permissions from %x to %y."), L"%x", L"\n" + fmtFileName(source)), L"%y", L"\n" + fmtFileName(target)), L"copyfile", getLastError());

	//owner is *not* copied with ::copyfile():

    struct ::stat fileInfo = {};
    if (procSl == ProcSymlink::FOLLOW)
    {
        if (::stat(source.c_str(), &fileInfo) != 0)
            throwFileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(source)), L"stat", getLastError());

        if (::chown(target.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"chown", getLastError());
    }
    else
    {
        if (::lstat(source.c_str(), &fileInfo) != 0)
            throwFileError(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtFileName(source)), L"lstat", getLastError());

        if (::lchown(target.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            throwFileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtFileName(target)), L"lchown", getLastError());
    }
#endif
}


void makeDirectoryRecursively(const Zstring& directory) //FileError, ErrorTargetExisting
{
    assert(!endsWith(directory, FILE_NAME_SEPARATOR)); //even "C:\" should be "C:" as input!

    try
    {
        makeDirectoryPlain(directory, Zstring(), false); //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
    }
    catch (const ErrorTargetPathMissing&)
    {
        //we need to create parent directories first
        const Zstring dirParent = beforeLast(directory, FILE_NAME_SEPARATOR);
        if (!dirParent.empty())
        {
            //recurse...
            try
            {
                makeDirectoryRecursively(dirParent); //throw FileError, (ErrorTargetExisting)
            }
            catch (const ErrorTargetExisting&) { /*parent directory created externally in the meantime? => NOT AN ERROR*/ }

            //now try again...
            makeDirectoryPlain(directory, Zstring(), false); //throw FileError, (ErrorTargetExisting), (ErrorTargetPathMissing)
            return;
        }
        throw;
    }
}
}


void zen::makeDirectory(const Zstring& directory, bool failIfExists) //throw FileError, ErrorTargetExisting
{
    //remove trailing separator (even for C:\ root directories)
    const Zstring dirFormatted = endsWith(directory, FILE_NAME_SEPARATOR) ?
                                 beforeLast(directory, FILE_NAME_SEPARATOR) :
                                 directory;

    try
    {
        makeDirectoryRecursively(dirFormatted); //FileError, ErrorTargetExisting
    }
    catch (const ErrorTargetExisting&)
    {
        //avoid any file system race-condition by *not* checking directory existence again here!!!
        if (failIfExists)
            throw;
    }
    catch (const FileError&)
    {
        /*
        could there be situations where a directory/network path exists,
        but creation fails with error different than "ErrorTargetExisting"??
        - creation of C:\ fails with ERROR_ACCESS_DENIED rather than ERROR_ALREADY_EXISTS
        */
        if (somethingExists(directory)) //a file system race-condition! don't use dirExists() => harmonize with ErrorTargetExisting!
        {
            assert(false);
            if (failIfExists)
                throw; //do NOT convert to ErrorTargetExisting: if "failIfExists", not getting a ErrorTargetExisting *atomically* is unexpected!
        }
        else
            throw;
    }
}


void zen::makeDirectoryPlain(const Zstring& directory, //throw FileError, ErrorTargetExisting, ErrorTargetPathMissing
                             const Zstring& templateDir,
                             bool copyFilePermissions)
{
#ifdef ZEN_WIN
    //special handling for volume root: trying to create existing root directory results in ERROR_ACCESS_DENIED rather than ERROR_ALREADY_EXISTS!
    Zstring dirTmp = removeLongPathPrefix(endsWith(directory, FILE_NAME_SEPARATOR) ?
                                          beforeLast(directory, FILE_NAME_SEPARATOR) :
                                          directory);
    if (dirTmp.size() == 2 &&
        std::iswalpha(dirTmp[0]) && dirTmp[1] == L':')
    {
        dirTmp += FILE_NAME_SEPARATOR; //we do not support "C:" to represent a relative path!

        const ErrorCode lastError = somethingExists(dirTmp) ? ERROR_ALREADY_EXISTS : ERROR_PATH_NOT_FOUND; //don't use dirExists() => harmonize with ErrorTargetExisting!

        const std::wstring errorMsg = replaceCpy(_("Cannot create directory %x."), L"%x", fmtFileName(dirTmp));
        const std::wstring errorDescr = formatSystemError(L"CreateDirectory", lastError);

        if (lastError == ERROR_ALREADY_EXISTS)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr); //[!] this is NOT a ErrorTargetPathMissing case!
    }

    //don't use ::CreateDirectoryEx:
    //- it may fail with "wrong parameter (error code 87)" when source is on mapped online storage
    //- automatically copies symbolic links if encountered: unfortunately it doesn't copy symlinks over network shares but silently creates empty folders instead (on XP)!
    //- it isn't able to copy most junctions because of missing permissions (although target path can be retrieved alternatively!)
    if (!::CreateDirectory(applyLongPathPrefixCreateDir(directory).c_str(), //__in      LPCTSTR lpPathName,
                           nullptr))                                        //__in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes
    {
        DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!

        //handle issues with already existing short 8.3 file names on Windows
        if (lastError == ERROR_ALREADY_EXISTS)
            if (have8dot3NameClash(directory))
            {
                Fix8Dot3NameClash dummy(directory); //throw FileError; move clashing object to the side

                //now try again...
                if (::CreateDirectory(applyLongPathPrefixCreateDir(directory).c_str(), nullptr))
                    lastError = ERROR_SUCCESS;
                else
                    lastError = ::GetLastError();
            }

        if (lastError != ERROR_SUCCESS)
        {
            const std::wstring errorMsg = replaceCpy(_("Cannot create directory %x."), L"%x", fmtFileName(directory));
            const std::wstring errorDescr = formatSystemError(L"CreateDirectory", lastError);

            if (lastError == ERROR_ALREADY_EXISTS)
                throw ErrorTargetExisting(errorMsg, errorDescr);
            else if (lastError == ERROR_PATH_NOT_FOUND)
                throw ErrorTargetPathMissing(errorMsg, errorDescr);
            throw FileError(errorMsg, errorDescr);
        }
    }

#elif defined ZEN_LINUX || defined ZEN_MAC
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //= default for newly created directory

    struct ::stat dirInfo = {};
	if (!templateDir.empty())
        if (::stat(templateDir.c_str(), &dirInfo) == 0)
		{
			mode = dirInfo.st_mode; //analog to "cp" which copies "mode" (considering umask) by default 
			mode |= S_IRWXU; //FFS only: we need full access to copy child items! "cp" seems to apply permissions *after* copying child items
		}
		//=> need copyObjectPermissions() only for "chown" and umask-agnostic permissions

    if (::mkdir(directory.c_str(), mode) != 0)
    {
        const int lastError = errno; //copy before directly or indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot create directory %x."), L"%x", fmtFileName(directory));
        const std::wstring errorDescr = formatSystemError(L"mkdir", lastError);

        if (lastError == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        else if (lastError == ENOENT)
            throw ErrorTargetPathMissing(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    }
#endif

    if (!templateDir.empty())
    {
#ifdef ZEN_WIN
        //optional: try to copy file attributes (dereference symlinks and junctions)
        const HANDLE hDirSrc = ::CreateFile(zen::applyLongPathPrefix(templateDir).c_str(),          //_In_      LPCTSTR lpFileName,
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
                ::SetFileAttributes(applyLongPathPrefix(directory).c_str(), dirInfo.dwFileAttributes);
                //copy "read-only and system attributes": http://blogs.msdn.com/b/oldnewthing/archive/2003/09/30/55100.aspx

                const bool isEncrypted  = (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED)  != 0;
                const bool isCompressed = (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;

                if (isEncrypted)
                    ::EncryptFile(directory.c_str()); //seems no long path is required (check passed!)

                HANDLE hDirTrg = ::CreateFile(applyLongPathPrefix(directory).c_str(), //_In_      LPCTSTR lpFileName,
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
        /*int rv =*/ ::copyfile(templateDir.c_str(), directory.c_str(), 0, COPYFILE_XATTR);
#endif

        zen::ScopeGuard guardNewDir = zen::makeGuard([&] { try { removeDirectory(directory); } catch (FileError&) {} }); //ensure cleanup:

        //enforce copying file permissions: it's advertized on GUI...
        if (copyFilePermissions)
            copyObjectPermissions(templateDir, directory, ProcSymlink::FOLLOW); //throw FileError

        guardNewDir.dismiss(); //target has been created successfully!
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
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L"\n" + fmtFileName(sourceLink)), L"%y", L"\n" + fmtFileName(targetLink)),
                        replaceCpy(_("Cannot find system function %x."), L"%x", L"\"CreateSymbolicLinkW\""));

    const wchar_t functionName[] = L"CreateSymbolicLinkW";
    if (!createSymbolicLink(targetLink.c_str(), //__in  LPTSTR lpSymlinkFileName, - seems no long path prefix is required...
                            linkPath.c_str(),   //__in  LPTSTR lpTargetFileName,
                            (isDirLink ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))) //__in  DWORD dwFlags
#elif defined ZEN_LINUX || defined ZEN_MAC
    const wchar_t functionName[] = L"symlink";
    if (::symlink(linkPath.c_str(), targetLink.c_str()) != 0)
#endif
        throwFileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L"\n" + fmtFileName(sourceLink)), L"%y", L"\n" + fmtFileName(targetLink)), functionName, getLastError());

    //allow only consistent objects to be created -> don't place before ::symlink, targetLink may already exist!
    zen::ScopeGuard guardNewLink = zen::makeGuard([&]
    {
        try
        {
#ifdef ZEN_WIN
            if (isDirLink)
                removeDirectory(targetLink); //throw FileError
            else
#endif
                removeFile(targetLink); //throw FileError
        }
        catch (FileError&) {}
    });

    //file times: essential for sync'ing a symlink: enforce this! (don't just try!)
#ifdef ZEN_WIN
    WIN32_FILE_ATTRIBUTE_DATA sourceAttr = {};
    if (!::GetFileAttributesEx(applyLongPathPrefix(sourceLink).c_str(), //__in   LPCTSTR lpFileName,
                               GetFileExInfoStandard,                   //__in   GET_FILEEX_INFO_LEVELS fInfoLevelId,
                               &sourceAttr))                            //__out  LPVOID lpFileInformation
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(sourceLink)), L"GetFileAttributesEx", getLastError());

    setFileTimeRaw(targetLink, &sourceAttr.ftCreationTime, sourceAttr.ftLastWriteTime, ProcSymlink::DIRECT); //throw FileError

#elif defined ZEN_LINUX || defined ZEN_MAC
    struct ::stat srcInfo = {};
    if (::lstat(sourceLink.c_str(), &srcInfo) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(sourceLink)), L"lstat", getLastError());

    setFileTime(targetLink, srcInfo.st_mtime, ProcSymlink::DIRECT); //throw FileError
#endif

#ifdef ZEN_MAC
    if (::copyfile(sourceLink.c_str(), targetLink.c_str(), 0, COPYFILE_XATTR | COPYFILE_NOFOLLOW) != 0)
        throwFileError(replaceCpy(replaceCpy(_("Cannot copy attributes from %x to %y."), L"%x", L"\n" + fmtFileName(sourceLink)), L"%y", L"\n" + fmtFileName(targetLink)), L"copyfile", getLastError());
#endif

    if (copyFilePermissions)
        copyObjectPermissions(sourceLink, targetLink, ProcSymlink::DIRECT); //throw FileError

    guardNewLink.dismiss(); //target has been created successfully!
}


namespace
{
#ifdef ZEN_WIN
/*
            CopyFileEx()    BackupRead()      FileRead()
			--------------------------------------------
Attributes       YES            NO			     NO
create time      NO             NO               NO
ADS			     YES            YES				 NO
Encrypted	     YES            NO(silent fail!) NO
Compressed	     NO             NO		         NO
Sparse		     NO             YES				 NO
Nonstandard FS   YES                  UNKNOWN    -> issues writing ADS to Samba, issues reading from NAS, error copying files having "blocked" state... ect.
PERF		       -                 6% faster

Mark stream as compressed: FSCTL_SET_COMPRESSION - compatible with both BackupRead() and FileRead()


Current support for combinations of NTFS extended attributes:

source attr | tf normal | tf compressed | tf encrypted | handled by
============|==================================================================
    ---     |    ---           -C-             E--       copyFileWindowsDefault
    --S     |    --S           -CS             E-S       copyFileWindowsSparse
    -C-     |    -C-           -C-             E--       copyFileWindowsDefault
    -CS     |    -CS           -CS             E-S       copyFileWindowsSparse
    E--     |    E--           E--             E--       copyFileWindowsDefault
    E-S     |    E-- (NOK)     E-- (NOK)       E-- (NOK) copyFileWindowsDefault -> may fail with ERROR_DISK_FULL!!

tf  := target folder
E   := encrypted
C   := compressed
S   := sparse
NOK := current behavior is not optimal/OK yet.

Note: - if target parent folder is compressed or encrypted, both attributes are added automatically during file creation!
      - "compressed" and "encrypted" are mutually exclusive: http://support.microsoft.com/kb/223093/en-us
*/


//due to issues on non-NTFS volumes, we should use the copy-as-sparse routine only if required and supported!
bool canCopyAsSparse(DWORD fileAttrSource, const Zstring& targetFile) //throw ()
{
    const bool sourceIsEncrypted = (fileAttrSource & FILE_ATTRIBUTE_ENCRYPTED)   != 0;
    const bool sourceIsSparse    = (fileAttrSource & FILE_ATTRIBUTE_SPARSE_FILE) != 0;

    if (sourceIsEncrypted || !sourceIsSparse) //BackupRead() silently fails reading encrypted files!
        return false; //small perf optimization: don't check "targetFile" if not needed

    //------------------------------------------------------------------------------------
    const DWORD bufferSize = 10000;
    std::vector<wchar_t> buffer(bufferSize);

    //full pathName need not yet exist!
    if (!::GetVolumePathName(targetFile.c_str(), //__in   LPCTSTR lpszFileName,
                             &buffer[0],         //__out  LPTSTR lpszVolumePathName,
                             bufferSize))        //__in   DWORD cchBufferLength
        return false;

    DWORD fsFlagsTarget = 0;
    if (!::GetVolumeInformation(&buffer[0],     //__in_opt   LPCTSTR lpRootPathName
                                nullptr,        //__out_opt  LPTSTR lpVolumeNameBuffer,
                                0,              //__in       DWORD nVolumeNameSize,
                                nullptr,        //__out_opt  LPDWORD lpVolumeSerialNumber,
                                nullptr,        //__out_opt  LPDWORD lpMaximumComponentLength,
                                &fsFlagsTarget, //__out_opt  LPDWORD lpFileSystemFlags,
                                nullptr,        //__out      LPTSTR lpFileSystemNameBuffer,
                                0))             //__in       DWORD nFileSystemNameSize
        return false;

    const bool targetSupportSparse = (fsFlagsTarget & FILE_SUPPORTS_SPARSE_FILES) != 0;

    return targetSupportSparse;
    //both source and target must not be FAT since copyFileWindowsSparse() does no DST hack! implicitly guaranteed at this point!
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


//precondition: canCopyAsSparse() must return "true"!
void copyFileWindowsSparse(const Zstring& sourceFile,
                           const Zstring& targetFile,
                           const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                           InSyncAttributes* newAttrib) //throw FileError, ErrorTargetExisting, ErrorFileLocked
{
    assert(canCopyAsSparse(sourceFile, targetFile));

    //try to get backup read and write privileges: who knows, maybe this helps solve some obscure "access denied" errors
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) {}
    try { activatePrivilege(SE_RESTORE_NAME); }
    catch (const FileError&) {}

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
        const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!

        const std::wstring errorMsg = replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(sourceFile));
        std::wstring errorDescr = formatSystemError(L"CreateFile", lastError);

        //if file is locked throw "ErrorFileLocked" instead!
        if (lastError == ERROR_SHARING_VIOLATION ||
            lastError == ERROR_LOCK_VIOLATION)
        {
            const Zstring procList = getLockingProcessNames(sourceFile); //throw()
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
            throw ErrorFileLocked(errorMsg, errorDescr);
        }

        throw FileError(errorMsg, errorDescr);
    }
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFileSource));

    //----------------------------------------------------------------------
    BY_HANDLE_FILE_INFORMATION fileInfoSource = {};
    if (!::GetFileInformationByHandle(hFileSource, &fileInfoSource))
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(sourceFile)), L"GetFileInformationByHandle", getLastError());

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
        const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtFileName(targetFile));
        const std::wstring errorDescr = formatSystemError(L"CreateFile", lastError);

        if (lastError == ERROR_FILE_EXISTS || //confirmed to be used
            lastError == ERROR_ALREADY_EXISTS) //comment on msdn claims, this one is used on Windows Mobile 6
            throw ErrorTargetExisting(errorMsg, errorDescr);

        //if (lastError == ERROR_PATH_NOT_FOUND) throw ErrorTargetPathMissing(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    ScopeGuard guardTarget = makeGuard([&] { try { removeFile(targetFile); } catch (FileError&) {} }); //transactional behavior: guard just after opening target and before managing hFileTarget
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hFileTarget));

    //----------------------------------------------------------------------
    BY_HANDLE_FILE_INFORMATION fileInfoTarget = {};
    if (!::GetFileInformationByHandle(hFileTarget, &fileInfoTarget))
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(targetFile)), L"GetFileInformationByHandle", getLastError());

    //return up-to-date file attributes
    if (newAttrib)
    {
        newAttrib->fileSize         = get64BitUInt(fileInfoSource.nFileSizeLow, fileInfoSource.nFileSizeHigh);
        newAttrib->modificationTime = filetimeToTimeT(fileInfoSource.ftLastWriteTime); //no DST hack (yet)
        newAttrib->sourceFileId     = extractFileId(fileInfoSource);
        newAttrib->targetFileId     = extractFileId(fileInfoTarget);
    }

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

    //if (sourceIsSparse && targetSupportsSparse) -> no need to check, this is our precondition!
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
            throwFileError(replaceCpy(_("Cannot write file attributes of %x."), L"%x", fmtFileName(targetFile)), L"DeviceIoControl, FSCTL_SET_SPARSE", getLastError());
    }

    //----------------------------------------------------------------------
    const DWORD BUFFER_SIZE = 128 * 1024; //must be greater than sizeof(WIN32_STREAM_ID)
    std::vector<BYTE> buffer(BUFFER_SIZE);

    LPVOID contextRead  = nullptr; //manage context for BackupRead()/BackupWrite()
    LPVOID contextWrite = nullptr; //

    ZEN_ON_SCOPE_EXIT(
        if (contextRead ) ::BackupRead (0, nullptr, 0, nullptr, true, false, &contextRead); //MSDN: "lpContext must be passed [...] all other parameters are ignored."
        if (contextWrite) ::BackupWrite(0, nullptr, 0, nullptr, true, false, &contextWrite); );

    //stream-copy sourceFile to targetFile
    bool eof = false;
    bool someBytesWritten = false; //try to detect failure reading encrypted files
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
            throwFileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(sourceFile)), L"BackupRead", getLastError()); //better use fine-granular error messages "reading/writing"!

        if (bytesRead > BUFFER_SIZE)
            throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(sourceFile)), L"buffer overflow"); //user should never see this

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
            throwFileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtFileName(targetFile)), L"BackupWrite", getLastError());

        if (bytesWritten != bytesRead)
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtFileName(targetFile)), L"incomplete write"); //user should never see this

        //total bytes transferred may be larger than file size! context information + ADS or smaller (sparse, compressed)!

        //invoke callback method to update progress indicators
        if (onUpdateCopyStatus)
            onUpdateCopyStatus(bytesRead); //throw X!

        if (bytesRead > 0)
            someBytesWritten = true;
    }
    while (!eof);

    //::BackupRead() silently fails reading encrypted files -> double check!
    if (!someBytesWritten && get64BitUInt(fileInfoSource.nFileSizeLow, fileInfoSource.nFileSizeHigh) != 0U)
        //note: there is no guaranteed ordering relation beween bytes transferred and file size! Consider ADS (>) and compressed/sparse files (<)!
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(sourceFile)), L"unknown error"); //user should never see this -> this method is called only if "canCopyAsSparse()"

    //time needs to be set at the end: BackupWrite() changes modification time
    if (!::SetFileTime(hFileTarget,
                       &fileInfoSource.ftCreationTime,
                       nullptr,
                       &fileInfoSource.ftLastWriteTime))
        throwFileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(targetFile)), L"SetFileTime", getLastError());

    guardTarget.dismiss();
}


DEFINE_NEW_FILE_ERROR(ErrorShouldCopyAsSparse);

class ErrorHandling
{
public:
    ErrorHandling() : shouldCopyAsSparse(false) {}

    //call context: copyCallbackInternal()
    void reportErrorShouldCopyAsSparse() { shouldCopyAsSparse = true; }

    void reportUserException(const std::exception_ptr& e) { exception = e; }

    void reportError(const std::wstring& msg, const std::wstring& description) { errorMsg = std::make_pair(msg, description); }

    //call context: copyFileWindowsDefault()
    void evaluateErrors() //throw X
    {
        if (shouldCopyAsSparse)
            throw ErrorShouldCopyAsSparse(L"sparse dummy value");

        if (exception)
            std::rethrow_exception(exception);

        if (!errorMsg.first.empty())
            throw FileError(errorMsg.first, errorMsg.second);
    }

private:
    bool shouldCopyAsSparse;                        //
    std::pair<std::wstring, std::wstring> errorMsg; //these are exclusive!
    std::exception_ptr exception;
};


struct CallbackData
{
    CallbackData(const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                 const Zstring& sourceFile,
                 const Zstring& targetFile) :
        sourceFile_(sourceFile),
        targetFile_(targetFile),
        onUpdateCopyStatus_(onUpdateCopyStatus),
        fileInfoSrc(),
        fileInfoTrg(),
        bytesReported() {}

    const Zstring& sourceFile_;
    const Zstring& targetFile_;
    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus_;

    ErrorHandling errorHandler;
    BY_HANDLE_FILE_INFORMATION fileInfoSrc; //modified by CopyFileEx() at beginning
    BY_HANDLE_FILE_INFORMATION fileInfoTrg; //

    std::int64_t bytesReported; //used internally to calculate bytes transferred delta
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

    symlink handling:
        if source is a symlink and COPY_FILE_COPY_SYMLINK is     specified, this callback is NOT invoked!
        if source is a symlink and COPY_FILE_COPY_SYMLINK is NOT specified, this callback is called and hSourceFile is a handle to the *target* of the link!

    file time handling:
        ::CopyFileEx() will (only) copy file modification time over from source file AFTER the last invokation of this callback
        => it is possible to adapt file creation time of target in here, but NOT file modification time!
    	CAVEAT: if ::CopyFileEx() fails to set modification time, it silently ignores this error and returns success!!!
    	see procmon log in: https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3514569&group_id=234430

    alternate data stream handling:
        CopyFileEx() processes multiple streams one after another, stream 1 is the file data stream and always available!
        Each stream is initialized with CALLBACK_STREAM_SWITCH and provides *new* hSourceFile, hDestinationFile.
        Calling GetFileInformationByHandle() on hDestinationFile for stream > 1 results in ERROR_ACCESS_DENIED!
    	totalBytesTransferred contains size of *all* streams and so can be larger than the "file size" file attribute
    */

    CallbackData& cbd = *static_cast<CallbackData*>(lpData);

    if (dwCallbackReason == CALLBACK_STREAM_SWITCH &&  //called up-front for every file (even if 0-sized)
        dwStreamNumber == 1) //consider ADS!
    {
        //#################### return source file attributes ################################
        if (!::GetFileInformationByHandle(hSourceFile, &cbd.fileInfoSrc))
        {
            const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
            cbd.errorHandler.reportError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(cbd.sourceFile_)), formatSystemError(L"GetFileInformationByHandle", lastError));
            return PROGRESS_CANCEL;
        }

        if (!::GetFileInformationByHandle(hDestinationFile, &cbd.fileInfoTrg))
        {
            const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!
            cbd.errorHandler.reportError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(cbd.targetFile_)), formatSystemError(L"GetFileInformationByHandle", lastError));
            return PROGRESS_CANCEL;
        }

        //#################### switch to sparse file copy if req. #######################
        if (canCopyAsSparse(cbd.fileInfoSrc.dwFileAttributes, cbd.targetFile_)) //throw ()
        {
            cbd.errorHandler.reportErrorShouldCopyAsSparse(); //use a different copy routine!
            return PROGRESS_CANCEL;
        }

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

    //called after copy operation is finished - note: for 0-sized files this callback is invoked just ONCE!
    //if (totalFileSize.QuadPart == totalBytesTransferred.QuadPart && dwStreamNumber == 1) {}
    if (cbd.onUpdateCopyStatus_ && totalBytesTransferred.QuadPart >= 0) //should always be true, but let's still check
        try
        {
            cbd.onUpdateCopyStatus_(totalBytesTransferred.QuadPart - cbd.bytesReported); //throw X!
            cbd.bytesReported = totalBytesTransferred.QuadPart;
        }
        catch (...)
        {
            cbd.errorHandler.reportUserException(std::current_exception());
            return PROGRESS_CANCEL;
        }
    return PROGRESS_CONTINUE;
}


const bool supportNonEncryptedDestination = winXpOrLater(); //encrypted destination is not supported with Windows 2000
//caveat: function scope static initialization is not thread-safe in VS 2010!


void copyFileWindowsDefault(const Zstring& sourceFile,
                            const Zstring& targetFile,
                            const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                            InSyncAttributes* newAttrib) //throw FileError, ErrorTargetExisting, ErrorFileLocked, ErrorShouldCopyAsSparse
{
    //try to get backup read and write privileges: who knows, maybe this helps solve some obscure "access denied" errors
    try { activatePrivilege(SE_BACKUP_NAME); }
    catch (const FileError&) {}
    try { activatePrivilege(SE_RESTORE_NAME); }
    catch (const FileError&) {}

    zen::ScopeGuard guardTarget = zen::makeGuard([&] { try { removeFile(targetFile); } catch (FileError&) {} });
    //transactional behavior: guard just before starting copy, we don't trust ::CopyFileEx(), do we? ;)

    DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;

    if (supportNonEncryptedDestination)
        copyFlags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION; //allow copying from encrypted to non-encrypted location

    //if (vistaOrLater()) //see http://blogs.technet.com/b/askperf/archive/2007/05/08/slow-large-file-copy-issues.aspx
    //  copyFlags |= COPY_FILE_NO_BUFFERING; //no perf difference at worst, huge improvement for large files (20% in test NTFS -> NTFS)
    //It's a shame this flag causes file corruption! https://sourceforge.net/projects/freefilesync/forums/forum/847542/topic/5177950
    //documentation on CopyFile2() even states: "It is not recommended to pause copies that are using this flag." How dangerous is this thing, why offer it at all???
    //perf advantage: ~15% faster

    CallbackData cbd(onUpdateCopyStatus, sourceFile, targetFile);

    const bool success = ::CopyFileEx(applyLongPathPrefix(sourceFile).c_str(), //__in      LPCTSTR lpExistingFileName,
                                      applyLongPathPrefix(targetFile).c_str(), //__in      LPCTSTR lpNewFileName,
                                      copyCallbackInternal, //__in_opt  LPPROGRESS_ROUTINE lpProgressRoutine,
                                      &cbd,                 //__in_opt  LPVOID lpData,
                                      nullptr,              //__in_opt  LPBOOL pbCancel,
                                      copyFlags) != FALSE;  //__in      DWORD dwCopyFlags

    cbd.errorHandler.evaluateErrors(); //throw ?, process errors in callback first!
    if (!success)
    {
        const DWORD lastError = ::GetLastError(); //copy before directly or indirectly making other system calls!

        //don't suppress "lastError == ERROR_REQUEST_ABORTED": a user aborted operation IS an error condition!

        //trying to copy huge sparse files may directly fail with ERROR_DISK_FULL before entering the callback function
        if (canCopyAsSparse(sourceFile, targetFile)) //noexcept
            throw ErrorShouldCopyAsSparse(L"sparse dummy value2");

        //assemble error message...
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot copy file %x to %y."), L"%x", L"\n" + fmtFileName(sourceFile)), L"%y", L"\n" + fmtFileName(targetFile));
        std::wstring errorDescr = formatSystemError(L"CopyFileEx", lastError);

        //if file is locked throw "ErrorFileLocked" instead!
        if (lastError == ERROR_SHARING_VIOLATION ||
            lastError == ERROR_LOCK_VIOLATION)
        {
            const Zstring procList = getLockingProcessNames(sourceFile); //throw() -> enhance error message!
            if (!procList.empty())
                errorDescr = _("The file is locked by another process:") + L"\n" + procList;
            throw ErrorFileLocked(replaceCpy(_("Cannot read file %x."), L"%x", fmtFileName(sourceFile)), errorDescr);
        }

        //if target is existing this functions is expected to throw ErrorTargetExisting!!!
        if (lastError == ERROR_FILE_EXISTS || //confirmed to be used
            lastError == ERROR_ALREADY_EXISTS) //not sure if used -> better be safe than sorry!!!
        {
            guardTarget.dismiss(); //don't delete file that existed previously!
            throw ErrorTargetExisting(errorMsg, errorDescr);
        }

        //if (lastError == ERROR_PATH_NOT_FOUND) throw ErrorTargetPathMissing(errorMsg, errorDescr); //could this also be source path missing!?

        try //add more meaningful message
        {
            //trying to copy > 4GB file to FAT/FAT32 volume gives obscure ERROR_INVALID_PARAMETER (FAT can indeed handle files up to 4 Gig, tested!)
            if (lastError == ERROR_INVALID_PARAMETER &&
                isFatDrive(targetFile) &&
                getFilesize(sourceFile) >= 4U * std::uint64_t(1024U * 1024 * 1024)) //throw FileError
                errorDescr += L"\nFAT volumes cannot store files larger than 4 gigabyte.";
            //see "Limitations of the FAT32 File System": http://support.microsoft.com/kb/314463/en-us

            //note: ERROR_INVALID_PARAMETER can also occur when copying to a SharePoint server or MS SkyDrive and the target filepath is of a restricted type.
        }
        catch (FileError&) {}

        throw FileError(errorMsg, errorDescr);
    }

    if (newAttrib)
    {
        newAttrib->fileSize         = get64BitUInt(cbd.fileInfoSrc.nFileSizeLow, cbd.fileInfoSrc.nFileSizeHigh);
        newAttrib->modificationTime = filetimeToTimeT(cbd.fileInfoSrc.ftLastWriteTime);
        newAttrib->sourceFileId     = extractFileId(cbd.fileInfoSrc);
        newAttrib->targetFileId     = extractFileId(cbd.fileInfoTrg);
    }
    warn_static("new perf check + investigate improvements now that DST hcak is gone! =>")

    //caveat: - ::CopyFileEx() silently *ignores* failure to set modification time!!! => we always need to set it again but with proper error checking!
    //	      - perf-loss on USB sticks with many small files of about 30%!
    setFileTimeRaw(targetFile, &cbd.fileInfoSrc.ftCreationTime, cbd.fileInfoSrc.ftLastWriteTime, ProcSymlink::FOLLOW); //throw FileError

    guardTarget.dismiss(); //target has been created successfully!
}


//another layer to support copying sparse files
inline
void copyFileWindowsSelectRoutine(const Zstring& sourceFile, const Zstring& targetFile, const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus, InSyncAttributes* sourceAttr)
{
    try
    {
        copyFileWindowsDefault(sourceFile, targetFile, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked, ErrorShouldCopyAsSparse
    }
    catch (ErrorShouldCopyAsSparse&) //we quickly check for this condition within callback of ::CopyFileEx()!
    {
        copyFileWindowsSparse(sourceFile, targetFile, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
}


//another layer of indirection solving 8.3 name clashes
inline
void copyFileOsSpecific(const Zstring& sourceFile,
                        const Zstring& targetFile,
                        const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                        InSyncAttributes* sourceAttr)
{
    try
    {
        copyFileWindowsSelectRoutine(sourceFile, targetFile, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
    catch (const ErrorTargetExisting&)
    {
        //try to handle issues with already existing short 8.3 file names on Windows
        if (have8dot3NameClash(targetFile))
        {
            Fix8Dot3NameClash dummy(targetFile); //throw FileError; move clashing filepath to the side
            copyFileWindowsSelectRoutine(sourceFile, targetFile, onUpdateCopyStatus, sourceAttr); //throw FileError; the short filepath name clash is solved, this should work now
            return;
        }
        throw;
    }
}


#elif defined ZEN_LINUX
void copyFileOsSpecific(const Zstring& sourceFile,
                        const Zstring& targetFile,
                        const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                        InSyncAttributes* newAttrib) //throw FileError, ErrorTargetExisting
{
    FileInputUnbuffered fileIn(sourceFile); //throw FileError

    struct ::stat sourceInfo = {};
    if (::fstat(fileIn.getDescriptor(), &sourceInfo) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(sourceFile)), L"fstat", getLastError());

    zen::ScopeGuard guardTarget = zen::makeGuard([&] { try { removeFile(targetFile); } catch (FileError&) {} }); //transactional behavior: place guard before lifetime of FileOutput
    try
    {
        FileOutputUnbuffered fileOut(targetFile, //throw FileError, ErrorTargetExisting
                                     sourceInfo.st_mode); //analog to "cp" which copies "mode" (considering umask) by default 
		//=> need copyObjectPermissions() only for "chown" and umask-agnostic permissions

        std::vector<char> buffer(128 * 1024); //see comment in FileInputUnbuffered::read
        do
        {
            const size_t bytesRead = fileIn.read(&buffer[0], buffer.size()); //throw FileError

            fileOut.write(&buffer[0], bytesRead); //throw FileError

            if (onUpdateCopyStatus)
                onUpdateCopyStatus(bytesRead); //throw X!
        }
        while (!fileIn.eof());

        //adapt target file modification time:
        {
            //read and return file statistics
            struct ::stat targetInfo = {};
            if (::fstat(fileOut.getDescriptor(), &targetInfo) != 0)
                throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(targetFile)), L"fstat", getLastError());

            if (newAttrib)
            {
                newAttrib->fileSize         = sourceInfo.st_size;
                newAttrib->modificationTime = sourceInfo.st_mtime;
                newAttrib->sourceFileId     = extractFileId(sourceInfo);
                newAttrib->targetFileId     = extractFileId(targetInfo);
            }
        }
    }
    catch (const ErrorTargetExisting&)
    {
        guardTarget.dismiss(); //don't delete file that existed previously!
        throw;
    }

    //we cannot set the target file times while the file descriptor is open and being written:
    //this triggers bugs on samba shares where the modification time is set to current time instead.
    //http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=340236
    //http://comments.gmane.org/gmane.linux.file-systems.cifs/2854
    //on the other hand we thereby have to reopen https://sourceforge.net/p/freefilesync/bugs/230/
    setFileTime(targetFile, sourceInfo.st_mtime, ProcSymlink::FOLLOW); //throw FileError

    guardTarget.dismiss(); //target has been created successfully!
}


#elif defined ZEN_MAC
struct CallbackData
{
    CallbackData(const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                 const Zstring& sourceFile,
                 const Zstring& targetFile) :
        onUpdateCopyStatus_(onUpdateCopyStatus),
        sourceFile_(sourceFile),
        targetFile_(targetFile),
        bytesReported() {}

    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus_; //in
    const Zstring& sourceFile_;
    const Zstring& targetFile_;

    std::pair<std::wstring, std::wstring> errorMsg; //out; these are exclusive!
    std::exception_ptr exception;                   //

    std::int64_t bytesReported; //private to callback
};


int copyFileCallback(int what, int stage, copyfile_state_t state, const char* src, const char* dst, void* ctx)
{
    CallbackData& cbd = *static_cast<CallbackData*>(ctx);

    off_t bytesCopied = 0;
    if (::copyfile_state_get(state, COPYFILE_STATE_COPIED, &bytesCopied) != 0)
    {
        cbd.errorMsg = std::make_pair(replaceCpy(replaceCpy(_("Cannot copy file %x to %y."), L"%x", L"\n" + fmtFileName(cbd.sourceFile_)), L"%y", L"\n" + fmtFileName(cbd.targetFile_)),
                                      formatSystemError(L"copyfile_state_get, COPYFILE_STATE_COPIED", getLastError()));
        return COPYFILE_QUIT;
    }

    if (cbd.onUpdateCopyStatus_)
        try
        {
            cbd.onUpdateCopyStatus_(bytesCopied - cbd.bytesReported); //throw X!
            cbd.bytesReported = bytesCopied;
        }
        catch (...)
        {
            cbd.exception = std::current_exception();
            return COPYFILE_QUIT;
        }
    return COPYFILE_CONTINUE;
}


void copyFileOsSpecific(const Zstring& sourceFile,
                        const Zstring& targetFile,
                        const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                        InSyncAttributes* newAttrib) //throw FileError, ErrorTargetExisting
{
    //http://blog.plasticsfuture.org/2006/03/05/the-state-of-backup-and-cloning-tools-under-mac-os-x/

    auto getCopyErrorMessage = [&] { return replaceCpy(replaceCpy(_("Cannot copy file %x to %y."), L"%x", L"\n" + fmtFileName(sourceFile)), L"%y", L"\n" + fmtFileName(targetFile)); };

    copyfile_state_t copyState = ::copyfile_state_alloc();
    ZEN_ON_SCOPE_EXIT(::copyfile_state_free(copyState));

    CallbackData cbd(onUpdateCopyStatus, sourceFile, targetFile);

    if (::copyfile_state_set(copyState, COPYFILE_STATE_STATUS_CTX, &cbd) != 0)
        throwFileError(getCopyErrorMessage(), L"copyfile_state_set, COPYFILE_STATE_STATUS_CTX", getLastError());

    if (::copyfile_state_set(copyState, COPYFILE_STATE_STATUS_CB, reinterpret_cast<const void*>(&copyFileCallback)) != 0)
        throwFileError(getCopyErrorMessage(), L"copyfile_state_set, COPYFILE_STATE_STATUS_CB", getLastError());

    zen::ScopeGuard guardTarget = zen::makeGuard([&] { try { removeFile(targetFile); } catch (FileError&) {} }); //transactional behavior: docs seem to indicate that copyfile does not clean up

    //http://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/copyfile.3.html
    if (::copyfile(sourceFile.c_str(), targetFile.c_str(), 
				  copyState, 
				  COPYFILE_XATTR | COPYFILE_DATA | COPYFILE_EXCL) != 0) //even though we don't use COPYFILE_STAT, "mode" (considering umask) is still copied! => harmonized with Linux file copy!
    {
        //evaluate first! errno is not set for COPYFILE_QUIT!
        if (cbd.exception)
            std::rethrow_exception(cbd.exception);

        if (!cbd.errorMsg.first.empty())
            throw FileError(cbd.errorMsg.first, cbd.errorMsg.second);

        const int lastError = errno;
        std::wstring errorDescr = formatSystemError(L"copyfile", lastError);

        if (lastError == EEXIST)
        {
            guardTarget.dismiss(); //don't delete file that existed previously!
            throw ErrorTargetExisting(getCopyErrorMessage(), errorDescr);
        }

        throw FileError(getCopyErrorMessage(), errorDescr);
    }

    int fdSource = 0;
    if (::copyfile_state_get(copyState, COPYFILE_STATE_SRC_FD, &fdSource) != 0)
        throwFileError(getCopyErrorMessage(), L"copyfile_state_get, COPYFILE_STATE_SRC_FD", getLastError());

    int fdTarget = 0;
    if (::copyfile_state_get(copyState, COPYFILE_STATE_DST_FD, &fdTarget) != 0)
        throwFileError(getCopyErrorMessage(), L"copyfile_state_get, COPYFILE_STATE_DST_FD", getLastError());

    struct ::stat sourceInfo = {};
    if (::fstat(fdSource, &sourceInfo) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(sourceFile)), L"fstat", getLastError());

    struct ::stat targetInfo = {};
    if (::fstat(fdTarget, &targetInfo) != 0)
        throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(targetFile)), L"fstat", getLastError());

    struct ::timeval newTimes[2] = {};
    newTimes[0].tv_sec = sourceInfo.st_atime; //access time (seconds)
    newTimes[1].tv_sec = sourceInfo.st_mtime; //modification time (seconds)

    if (::futimes(fdTarget, newTimes) != 0)
        throwFileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtFileName(targetFile)), L"futimes", getLastError());

    if (newAttrib)
    {
        newAttrib->fileSize         = sourceInfo.st_size;
        newAttrib->modificationTime = sourceInfo.st_mtime;
        newAttrib->sourceFileId     = extractFileId(sourceInfo);
        newAttrib->targetFileId     = extractFileId(targetInfo);
    }

    guardTarget.dismiss();
}
#endif

/*
      ------------------
      |File Copy Layers|
      ------------------
                  copyFile (setup transactional behavior)
                       |
                copyFileWithPermissions
                       |
               copyFileOsSpecific (solve 8.3 issue)
                        |
			  copyFileWindowsSelectRoutine
	          /                           \
copyFileWindowsDefault(::CopyFileEx)  copyFileWindowsSparse(::BackupRead/::BackupWrite)
*/

inline
void copyFileWithPermissions(const Zstring& sourceFile,
                             const Zstring& targetFile,
                             bool copyFilePermissions,
                             const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                             InSyncAttributes* sourceAttr)
{
    copyFileOsSpecific(sourceFile, targetFile, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked

    if (copyFilePermissions)
    {
        //at this point we know we created a new file, so it's fine to delete it for cleanup!
        zen::ScopeGuard guardTargetFile = zen::makeGuard([&] { try { removeFile(targetFile); } catch (FileError&) {}});

        copyObjectPermissions(sourceFile, targetFile, ProcSymlink::FOLLOW); //throw FileError

        guardTargetFile.dismiss(); //target has been created successfully!
    }
}
}


void zen::copyFile(const Zstring& sourceFile, //throw FileError, ErrorFileLocked
                   const Zstring& targetFile,
                   bool copyFilePermissions,
                   bool transactionalCopy,
                   const std::function<void()>& onDeleteTargetFile,
                   const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus,
                   InSyncAttributes* sourceAttr)
{
    if (transactionalCopy)
    {
        Zstring tmpTarget = targetFile + TEMP_FILE_ENDING;

        for (int i = 0;; ++i)
            try
            {
                copyFileWithPermissions(sourceFile, tmpTarget, copyFilePermissions, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked
                break;
            }
            catch (const ErrorTargetExisting&) //optimistic strategy: assume everything goes well, but recover on error -> minimize file accesses
            {
                if (i == 10) throw; //avoid endless recursion in pathological cases, e.g. https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/36adac33
                tmpTarget = targetFile + Zchar('_') + numberTo<Zstring>(i) + TEMP_FILE_ENDING;
            }

        //transactional behavior: ensure cleanup; not needed before copyFileWithPermissions() which is already transactional
        zen::ScopeGuard guardTempFile = zen::makeGuard([&] { try { removeFile(tmpTarget); } catch (FileError&) {} });

        //have target file deleted (after read access on source and target has been confirmed) => allow for almost transactional overwrite
        if (onDeleteTargetFile)
            onDeleteTargetFile(); //throw X

        //perf: this call is REALLY expensive on unbuffered volumes! ~40% performance decrease on FAT USB stick!
        renameFile(tmpTarget, targetFile); //throw FileError

        /*
        CAVEAT on FAT/FAT32: the sequence of deleting the target file and renaming "file.txt.ffs_tmp" to "file.txt" does
        NOT PRESERVE the creation time of the .ffs_tmp file, but SILENTLY "reuses" whatever creation time the old "file.txt" had!
        This "feature" is called "File System Tunneling":
        http://blogs.msdn.com/b/oldnewthing/archive/2005/07/15/439261.aspx
        http://support.microsoft.com/kb/172190/en-us
        */

        guardTempFile.dismiss();
    }
    else
    {
        /*
           Note: non-transactional file copy solves at least four problems:
            	-> skydrive - doesn't allow for .ffs_tmp extension and returns ERROR_INVALID_PARAMETER
            	-> network renaming issues
            	-> allow for true delete before copy to handle low disk space problems
            	-> higher performance on non-buffered drives (e.g. usb sticks)
        */

        if (onDeleteTargetFile)
            onDeleteTargetFile();

        copyFileWithPermissions(sourceFile, targetFile, copyFilePermissions, onUpdateCopyStatus, sourceAttr); //throw FileError, ErrorTargetExisting, ErrorFileLocked
    }
}
