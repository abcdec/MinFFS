// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "lib/deep_file_traverser.h"
#include <zen/sys_error.h>
#include <zen/symlink_target.h>
#include <zen/int64.h>
#include <cstddef> //offsetof
#include <sys/stat.h>
#include <dirent.h>

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

#ifdef MinFFS_PATCH
		// MinFFS: MinGW does not have pathconf and _PC_NAME_MAX. So
		// use a fixed number. Since the code above will likely result
		// in 10000 for Windows and 10K path name is kind of
		// ridiculously large side for most of use, use this number
		// anyway.  MSDN says 260 charctors.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
		const size_t nameMax = 10000;
#else
		/* quote: "Since POSIX.1 does not specify the size of the d_name field, and other nonstandard fields may precede
                   that field within the dirent structure, portable applications that use readdir_r() should allocate
                   the buffer whose address is passed in entry as follows:
		   len = offsetof(struct dirent, d_name) + pathconf(dirPath, _PC_NAME_MAX) + 1
		   entryp = malloc(len); */
		const size_t nameMax = std::max<long>(::pathconf(directoryFormatted.c_str(), _PC_NAME_MAX), 10000); //::pathconf may return long(-1)
#endif//MinFFS_PATCH
		buffer.resize(offsetof(struct ::dirent, d_name) + nameMax + 1);

		traverse(directoryFormatted, sink);
	    }

	DirTraverser           (const DirTraverser&) = delete;
	DirTraverser& operator=(const DirTraverser&) = delete;

	void traverse(const Zstring& dirPath, TraverseCallback& sink)
	    {
		tryReportingDirError([&]
				     {
					 traverseWithException(dirPath, sink); //throw FileError
				     }, sink);
	    }

	void traverseWithException(const Zstring& dirPath, TraverseCallback& sink) //throw FileError
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
			throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPath)), L"readdir_r", ::GetLastError());
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
			throw FileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPath)), L"Data corruption: Found item without name.");
		    }
		    
		    if (shortName.c_str()[0] == L'.' && (shortName.c_str()[1] == 0 || (shortName.c_str()[1] == L'.' && shortName.c_str()[2] == 0)))
		    {
			continue;
		    }
		    
		    const Zstring& itempath = appendSeparator(dirPath) + shortName;
		    
		    if (fileAttr_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) //a directory
		    {
			if (TraverseCallback* trav = sink.onDir({ shortName.c_str(), itempath }))
			{
			    ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
			    traverse(itempath, *trav);
			}
		    }
		    else //a file or named pipe, ect.
		    {
			// Long file name support upto 32,767 wide characters for Unicode based path needs "\\?\" prefixed on CreateFile
			fileHandle = CreateFile((L"\\\\?\\"+itempath).c_str(), 0, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (fileHandle == INVALID_HANDLE_VALUE)
			{
			    throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(itempath)), L"CreateFile", ::GetLastError());
			}
			GetFileInformationByHandle(fileHandle, &handleFileInfo);
			CloseHandle(fileHandle);
			
			std::uint64_t fileSize = (static_cast<std::uint64_t>(fileAttr_.nFileSizeHigh) << 32) + static_cast<std::uint64_t>(fileAttr_.nFileSizeLow);
			std::int64_t lastWriteWinFileTime = (static_cast<std::int64_t>(fileAttr_.ftLastWriteTime.dwHighDateTime) << 32) + static_cast<std::int64_t>(fileAttr_.ftLastWriteTime.dwLowDateTime);
			// FILETIME structure contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
			// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
			// convert to Linux epoch which is number of seconds since Jan. 1st 1970 UTC
			std::int64_t lastWriteTimeLinuxEpoc = (lastWriteWinFileTime / 10000000LL) - 11644473600LL;
			
			TraverseCallback::FileInfo fi = {
			    shortName.c_str(),              // const Zchar*
			    itempath,                       // const Zstring&
			    makeUnsigned(fileSize),         // std::uint64_t  unit: bytes
			    lastWriteTimeLinuxEpoc,         // std::int64_t   number of seconds since Jan. 1st 1970 UTC
			    extractFileId(handleFileInfo),  // const FileId&  optional: initial if not supported!
			    nullptr                         // SymlinkInfo*   only filled if file is a followed symlink
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
		    throwFileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtFileName(dirPath)), L"opendir", getLastError());
		ZEN_ON_SCOPE_EXIT(::closedir(dirObj)); //never close nullptr handles! -> crash

		for (;;)
		{
		    struct ::dirent* dirEntry = nullptr;
		    if (::readdir_r(dirObj, reinterpret_cast< ::dirent*>(&buffer[0]), &dirEntry) != 0)
			throwFileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPath)), L"readdir_r", getLastError());
		    //don't retry but restart dir traversal on error! http://blogs.msdn.com/b/oldnewthing/archive/2014/06/12/10533529.aspx

		    if (!dirEntry) //no more items
			return;

		    //don't return "." and ".."
		    const char* shortName = dirEntry->d_name; //evaluate dirEntry *before* going into recursion => we use a single "buffer"!

		    if (shortName[0] == 0) throw FileError(replaceCpy(_("Cannot enumerate directory %x."), L"%x", fmtFileName(dirPath)), L"Data corruption: Found item without name.");
		    if (shortName[0] == '.' &&
			(shortName[1] == 0 || (shortName[1] == '.' && shortName[2] == 0)))
			continue;
				
		    const Zstring& itempath = appendSeparator(dirPath) + shortName;

		    struct ::stat statData = {};
		    if (!tryReportingItemError([&]
					       {
						   if (::lstat(itempath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
						       throwFileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtFileName(itempath)), L"lstat", getLastError());
					       }, sink, shortName))
			continue; //ignore error: skip file

		    if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
		    {
			const TraverseCallback::SymlinkInfo linkInfo = { shortName, itempath, statData.st_mtime };

			switch (sink.onSymlink(linkInfo))
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
				    if (TraverseCallback* trav = sink.onDir({ shortName, itempath }))
				    {
					ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
					traverse(itempath, *trav);
				    }
				}
				else //a file or named pipe, ect.
				{
				    TraverseCallback::FileInfo fi = { shortName, itempath, makeUnsigned(statDataTrg.st_size), statDataTrg.st_mtime, extractFileId(statDataTrg), &linkInfo };
				    sink.onFile(fi);
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
			if (TraverseCallback* trav = sink.onDir({ shortName, itempath }))
			{
			    ZEN_ON_SCOPE_EXIT(sink.releaseDirTraverser(trav));
			    traverse(itempath, *trav);
			}
		    }
		    else //a file or named pipe, ect.
		    {
			TraverseCallback::FileInfo fi = { shortName, itempath, makeUnsigned(statData.st_size), statData.st_mtime, extractFileId(statData), nullptr };
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


void zen::deepTraverseFolder(const Zstring& dirPath, TraverseCallback& sink) { DirTraverser::execute(dirPath, sink); }
