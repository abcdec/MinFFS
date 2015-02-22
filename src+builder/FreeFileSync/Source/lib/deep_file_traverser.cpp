// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "deep_file_traverser.h"
#include <zen/sys_error.h>
#include <zen/symlink_target.h>
#include <zen/int64.h>
#include <cstddef> //offsetof
#include <sys/stat.h>
#include <dirent.h>

#ifdef ZEN_MAC
    #include <zen/osx_string.h>
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
                                sink.onFile({ shortName, itempath, makeUnsigned(statDataTrg.st_size), statDataTrg.st_mtime, extractFileId(statDataTrg), &linkInfo });
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
                sink.onFile({ shortName, itempath, makeUnsigned(statData.st_size), statData.st_mtime, extractFileId(statData), nullptr });
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
}


void zen::deepTraverseFolder(const Zstring& dirpath, TraverseCallback& sink) { DirTraverser::execute(dirpath, sink); }
