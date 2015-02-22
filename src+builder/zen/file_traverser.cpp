// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "file_traverser.h"
#include "file_error.h"
#include "int64.h"

#ifdef ZEN_WIN
    #include "long_path_prefix.h"
    #include "file_access.h"
    #include "symlink_target.h"
#elif defined ZEN_MAC
        #include "osx_string.h"
#endif

#if defined ZEN_LINUX || defined ZEN_MAC
    #include <cstddef> //offsetof
	#include <unistd.h> //::pathconf()
    #include <sys/stat.h>
    #include <dirent.h>
#endif

using namespace zen;


void zen::traverseFolder(const Zstring& dirPath,
                         const std::function<void (const FileInfo&    fi)>& onFile,         
                         const std::function<void (const DirInfo&     di)>& onDir,          
                         const std::function<void (const SymlinkInfo& si)>& onLink,         
                         const std::function<void (const std::wstring& errorMsg)>& onError)
{
    try
    {
#ifdef ZEN_WIN
        WIN32_FIND_DATA findData = {};
        HANDLE hDir = ::FindFirstFile(applyLongPathPrefix(appendSeparator(dirPath) + L'*').c_str(), &findData);
        if (hDir == INVALID_HANDLE_VALUE)
        {
            const DWORD lastError = ::GetLastError(); //copy before making other system calls!
            if (lastError == ERROR_FILE_NOT_FOUND)
            {
                //1. directory may not exist *or* 2. it is completely empty: not all directories contain "., .." entries, e.g. a drive's root directory; NetDrive
                // -> FindFirstFile() is a nice example of violation of API design principle of single responsibility
                if (dirExists(dirPath)) //yes, a race-condition, still the best we can do
                    return;
            }
            throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirPath)), L"FindFirstFile", lastError);
        }
        ZEN_ON_SCOPE_EXIT(::FindClose(hDir));

		bool firstIteration = true;
        for (;;)
        {			
			if (firstIteration) //keep ::FindNextFile at the start of the for-loop to support "continue"!
				firstIteration = false;
			else
            if (!::FindNextFile(hDir, &findData))
            {
                const DWORD lastError = ::GetLastError();
                if (lastError == ERROR_NO_MORE_FILES) //not an error situation
                    return;
                //else we have a problem... report it:
                throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPath)), L"FindNextFile", lastError);
            }

            //skip "." and ".."
            const Zchar* const shortName = findData.cFileName;
            if (shortName[0] == L'.' &&
                (shortName[1] == 0 || (shortName[1] == L'.' && shortName[2] == 0)))
                continue;

            const Zstring& itempath = appendSeparator(dirPath) + shortName;

            if (zen::isSymlink(findData)) //check first!
            {
                if (onLink)
                    onLink({ shortName, itempath, filetimeToTimeT(findData.ftLastWriteTime) });
            }
            else if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                if (onDir)
                    onDir({ shortName, itempath });
            }
            else //a file
            {
                if (onFile)
                    onFile({ shortName, itempath, get64BitUInt(findData.nFileSizeLow, findData.nFileSizeHigh), filetimeToTimeT(findData.ftLastWriteTime) });
            }
        }

#elif defined ZEN_LINUX || defined ZEN_MAC
        const Zstring dirPathFmt = //remove trailing slash
            dirPath.size() > 1 && endsWith(dirPath, FILE_NAME_SEPARATOR) ?  //exception: allow '/'
            beforeLast(dirPath, FILE_NAME_SEPARATOR) :
            dirPath;

        /* quote: "Since POSIX.1 does not specify the size of the d_name field, and other nonstandard fields may precede
                   that field within the dirent structure, portable applications that use readdir_r() should allocate
                   the buffer whose address is passed in entry as follows:
                       len = offsetof(struct dirent, d_name) + pathconf(dirPath, _PC_NAME_MAX) + 1
                       entryp = malloc(len); */
        const size_t nameMax = std::max<long>(::pathconf(dirPathFmt.c_str(), _PC_NAME_MAX), 10000); //::pathconf may return long(-1)
		std::vector<char> buffer(offsetof(struct ::dirent, d_name) + nameMax + 1);
#ifdef ZEN_MAC
		std::vector<char> bufferUtfDecomposed;
#endif

        DIR* dirObj = ::opendir(dirPathFmt.c_str()); //directory must NOT end with path separator, except "/"
        if (!dirObj)
            throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirPathFmt)), L"opendir", getLastError());
        ZEN_ON_SCOPE_EXIT(::closedir(dirObj)); //never close nullptr handles! -> crash

        for (;;)
        {
            struct ::dirent* dirEntry = nullptr;
            if (::readdir_r(dirObj, reinterpret_cast< ::dirent*>(&buffer[0]), &dirEntry) != 0)
                throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPathFmt)), L"readdir_r", getLastError());
            //don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx

            if (!dirEntry) //no more items
                return;

            //don't return "." and ".."
            const char* shortName = dirEntry->d_name;
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
                        shortName = &bufferUtfDecomposed[0];
                }
            }
            //const char* sampleDecomposed  = "\x6f\xcc\x81.txt";
            //const char* samplePrecomposed = "\xc3\xb3.txt";
#endif
            const Zstring& itempath = appendSeparator(dirPathFmt) + shortName;

            struct ::stat statData = {};
			try
			{
					if (::lstat(itempath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
							throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(itempath)), L"lstat", getLastError());
			}
		    catch (const FileError& e)
			{
				if (onError)
					onError(e.toString());
				continue; //ignore error: skip file
			}           

            if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
            {
                if (onLink)
                    onLink({ shortName, itempath, statData.st_mtime});
            }
            else if (S_ISDIR(statData.st_mode)) //a directory
            {
                if (onDir)
                    onDir({ shortName, itempath });
            }
            else //a file or named pipe, ect.
            {
                if (onFile)
                    onFile({ shortName, itempath, makeUnsigned(statData.st_size), statData.st_mtime });
            }
            /*
            It may be a good idea to not check "S_ISREG(statData.st_mode)" explicitly and to not issue an error message on other types to support these scenarios:
            - RTS setup watch (essentially wants to read directories only)
            - removeDirectory (wants to delete everything; pipes can be deleted just like files via "unlink")

            However an "open" on a pipe will block (https://sourceforge.net/p/freefilesync/bugs/221/), so the copy routines need to be smarter!!
            */
        }
#endif
    }
    catch (const FileError& e)
    {
        if (onError)
            onError(e.toString());
    }
}
