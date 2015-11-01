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

#include <zen/sys_error.h>
#include <zen/symlink_target.h>
#include <zen/int64.h>

#include <cstddef> //offsetof
#include <sys/stat.h>
#include <dirent.h>

//implementation header for native.cpp, not for reuse!!!

namespace
{
using namespace zen;
using ABF = AbstractBaseFolder;


inline
ABF::FileId convertToAbstractFileId(const zen::FileId& fid)
{
    if (fid == zen::FileId())
        return ABF::FileId();

    ABF::FileId out(reinterpret_cast<const char*>(&fid.first), sizeof(fid.first));
    out.append(reinterpret_cast<const char*>(&fid.second), sizeof(fid.second));
    return out;
}


class DirTraverser
{
public:
    static void execute(const Zstring& baseDirectory, ABF::TraverserCallback& sink)
    {
        DirTraverser(baseDirectory, sink);
    }

private:
    DirTraverser(const Zstring& baseDirectory, ABF::TraverserCallback& sink)
    {
#ifdef MinFFS_PATCH
		// MinFFS: MinGW does not have pathconf and _PC_NAME_MAX. So
		// use a fixed number. Since the code above will likely result
		// in 10000 for Windows and 10K path name is kind of
		// ridiculously large side for most of use, use this number
		// anyway.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
		const size_t nameMax = 10000;
#else
        /* quote: "Since POSIX.1 does not specify the size of the d_name field, and other nonstandard fields may precede
                   that field within the dirent structure, portable applications that use readdir_r() should allocate
                   the buffer whose address is passed in entry as follows:
                       len = offsetof(struct dirent, d_name) + pathconf(dirPath, _PC_NAME_MAX) + 1
                       entryp = malloc(len); */
        const size_t nameMax = std::max<long>(::pathconf(baseDirectory.c_str(), _PC_NAME_MAX), 10000); //::pathconf may return long(-1)
#endif//MinFFS_PATCH
        buffer.resize(offsetof(struct ::dirent, d_name) + nameMax + 1);

        traverse(baseDirectory, sink);
    }

    DirTraverser           (const DirTraverser&) = delete;
    DirTraverser& operator=(const DirTraverser&) = delete;

    void traverse(const Zstring& dirPath, ABF::TraverserCallback& sink)
    {
        tryReportingDirError([&]
        {
            traverseWithException(dirPath, sink); //throw FileError
        }, sink);
    }

    void traverseWithException(const Zstring& dirPath, ABF::TraverserCallback& sink) //throw FileError
    {
#ifdef MinFFS_PATCH
	HANDLE dirHandle, fileHandle;
	WIN32_FIND_DATA fileAttr;
	BY_HANDLE_FILE_INFORMATION handleFileInfo;
	Zstring findPattern = dirPath + L"\\*.*";
	std::vector<WIN32_FIND_DATA>fileVector;
	
	dirHandle = ::FindFirstFile(findPattern.c_str(), &fileAttr);
	
	if (dirHandle == INVALID_HANDLE_VALUE) {
	    if (::GetLastError() != ERROR_NO_MORE_FILES)
	    {
		THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtPath(dirPath)), L"FindFirstFile");
		//don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx
	    }
	    return;
	}
	do {
	    fileVector.push_back(fileAttr);
	} while (::FindNextFile(dirHandle, &fileAttr));
	::FindClose(dirHandle);
	
	for (auto fileAttr_ : fileVector) {
	    
	    //don't return "." and ".."
	    const Zstring shortName(fileAttr_.cFileName); //evaluate dirEntry *before* going into recursion => we use a single "buffer"!
	    
	    if (shortName.c_str()[0] == 0)
	    {
		THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtPath(dirPath)), L"FindNextFile: Data corruption: Found item without name.");
	    }
	    
	    if (shortName.c_str()[0] == L'.' && (shortName.c_str()[1] == 0 || (shortName.c_str()[1] == L'.' && shortName.c_str()[2] == 0)))
	    {
		continue;
	    }
	    
	    const Zstring& itempath = appendSeparator(dirPath) + shortName;
	    
	    if (fileAttr_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) //a directory
	    {
		if (std::unique_ptr<ABF::TraverserCallback> trav = sink.onDir({ shortName.c_str() }))
		{
		    traverse(itempath, *trav);
		}
	    }
	    else //a file or named pipe, ect.
	    {
		// Long file name support upto 32,767 wide characters for Unicode based path needs "\\?\" prefixed on CreateFile
		fileHandle = CreateFile((L"\\\\?\\"+itempath).c_str(), 0, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (fileHandle == INVALID_HANDLE_VALUE)
		{
		    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itempath)), L"CreateFile");
		}
		GetFileInformationByHandle(fileHandle, &handleFileInfo);
		CloseHandle(fileHandle);
		
		std::uint64_t fileSize = (static_cast<std::uint64_t>(fileAttr_.nFileSizeHigh) << 32) + static_cast<std::uint64_t>(fileAttr_.nFileSizeLow);
		std::int64_t lastWriteWinFileTime = (static_cast<std::int64_t>(fileAttr_.ftLastWriteTime.dwHighDateTime) << 32) + static_cast<std::int64_t>(fileAttr_.ftLastWriteTime.dwLowDateTime);
		// FILETIME structure contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
		// convert to Linux epoch which is number of seconds since Jan. 1st 1970 UTC
		std::int64_t lastWriteTimeLinuxEpoc = (lastWriteWinFileTime / 10000000LL) - 11644473600LL;
		
                ABF::TraverserCallback::FileInfo fi = {
		    shortName.c_str(),                                       // const Zchar*
		    makeUnsigned(fileSize),                                  // std::uint64_t  unit: bytes
		    lastWriteTimeLinuxEpoc,                                  // std::int64_t   number of seconds since Jan. 1st 1970 UTC
		    convertToAbstractFileId(extractFileId(handleFileInfo)),  // const FileId&  optional: initial if not supported!
		    nullptr                                                  // SymlinkInfo*   only filled if file is a followed symlink
		};
		sink.onFile(fi);
	    }
	    /*
	      It may be a good idea to not check "S_ISREG(statData.st_mode)" explicitly and to not issue an error message on other types to support these scenarios:
	      - RTS setup watch (essentially wants to read directories only)
	      - removeDirectory (wants to delete everything; pipes can be deleted just like files via "unlink")
	      
	      However an "open" on a pipe will block (https://sourceforge.net/p/freefilesync/bugs/221/), so the copy routines need to be smarter!!
	    */
	}
	return;
#else
        //no need to check for endless recursion: Linux has a fixed limit on the number of symbolic links in a path

        DIR* dirObj = ::opendir(dirPath.c_str()); //directory must NOT end with path separator, except "/"
        if (!dirObj)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open directory %x."), L"%x", fmtPath(dirPath)), L"opendir");
        ZEN_ON_SCOPE_EXIT(::closedir(dirObj)); //never close nullptr handles! -> crash

        for (;;)
        {
            struct ::dirent* dirEntry = nullptr;
            if (::readdir_r(dirObj, reinterpret_cast< ::dirent*>(&buffer[0]), &dirEntry) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtPath(dirPath)), L"readdir_r");
            //don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx

            if (!dirEntry) //no more items
                return;

            //don't return "." and ".."
            const char* itemName = dirEntry->d_name; //evaluate dirEntry *before* going into recursion => we use a single "buffer"!

            if (itemName[0] == 0) throw FileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtPath(dirPath)), L"readdir_r: Data corruption; item is missing a name.");
            if (itemName[0] == '.' &&
                (itemName[1] == 0 || (itemName[1] == '.' && itemName[2] == 0)))
                continue;

			const Zstring& itemPath = appendSeparator(dirPath) + itemName;

            struct ::stat statData = {};
            if (!tryReportingItemError([&]
        {
            if (::lstat(itemPath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
                    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), L"lstat");
            }, sink, itemName))
            continue; //ignore error: skip file

            if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
            {
                const ABF::TraverserCallback::SymlinkInfo linkInfo = { itemName, statData.st_mtime };

                switch (sink.onSymlink(linkInfo))
                {
                    case ABF::TraverserCallback::LINK_FOLLOW:
                    {
                        //try to resolve symlink (and report error on failure!!!)
                        struct ::stat statDataTrg = {};

                        bool validLink = tryReportingItemError([&]
                        {
                            if (::stat(itemPath.c_str(), &statDataTrg) != 0)
                                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(itemPath)), L"stat");
                        }, sink, itemName);

                        if (validLink)
                        {
                            if (S_ISDIR(statDataTrg.st_mode)) //a directory
                            {
                                if (std::unique_ptr<ABF::TraverserCallback> trav = sink.onDir({ itemName }))
                                    traverse(itemPath, *trav);
                            }
                            else //a file or named pipe, ect.
                            {
                                ABF::TraverserCallback::FileInfo fi = { itemName, makeUnsigned(statDataTrg.st_size), statDataTrg.st_mtime, convertToAbstractFileId(extractFileId(statDataTrg)), &linkInfo };
                                sink.onFile(fi);
                            }
                        }
                        // else //broken symlink -> ignore: it's client's responsibility to handle error!
                    }
                    break;

                    case ABF::TraverserCallback::LINK_SKIP:
                        break;
                }
            }
            else if (S_ISDIR(statData.st_mode)) //a directory
            {
                if (std::unique_ptr<ABF::TraverserCallback> trav = sink.onDir({ itemName }))
                    traverse(itemPath, *trav);
            }
            else //a file or named pipe, ect.
            {
                ABF::TraverserCallback::FileInfo fi = { itemName, makeUnsigned(statData.st_size), statData.st_mtime, convertToAbstractFileId(extractFileId(statData)), nullptr /*symlinkInfo*/ };
                sink.onFile(fi);
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

    std::vector<char> buffer;
};
}
