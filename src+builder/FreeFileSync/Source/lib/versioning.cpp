#include "versioning.h"
#include <map>
#include <cstddef> //required by GCC 4.8.1 to find ptrdiff_t
#include <zen/file_access.h>
#include <zen/file_traverser.h>
#include <zen/string_tools.h>

using namespace zen;


namespace
{
Zstring getExtension(const Zstring& relativePath) //including "." if extension is existing, returns empty string otherwise
{
    auto iterSep = find_last(relativePath.begin(), relativePath.end(), FILE_NAME_SEPARATOR);
    auto iterName = iterSep != relativePath.end() ? iterSep + 1 : relativePath.begin(); //find beginning of short name
    auto iterDot = find_last(iterName, relativePath.end(), Zstr('.')); //equal to relativePath.end() if file has no extension!!
    return Zstring(&*iterDot, relativePath.end() - iterDot);
};
}

bool impl::isMatchingVersion(const Zstring& shortname, const Zstring& shortnameVersion) //e.g. ("Sample.txt", "Sample.txt 2012-05-15 131513.txt")
{
    auto it = shortnameVersion.begin();
    auto last = shortnameVersion.end();

    auto nextDigit = [&]() -> bool
    {
        if (it == last || !isDigit(*it))
            return false;
        ++it;
        return true;
    };
    auto nextDigits = [&](size_t count) -> bool
    {
        while (count-- > 0)
            if (!nextDigit())
                return false;
        return true;
    };
    auto nextChar = [&](Zchar c) -> bool
    {
        if (it == last || *it != c)
            return false;
        ++it;
        return true;
    };
    auto nextStringI = [&](const Zstring& str) -> bool //windows: ignore case!
    {
        if (last - it < static_cast<ptrdiff_t>(str.size()) || !EqualFilename()(str, Zstring(&*it, str.size())))
            return false;
        it += str.size();
        return true;
    };

    return nextStringI(shortname) && //versioned file starts with original name
           nextChar(Zstr(' ')) && //validate timestamp: e.g. "2012-05-15 131513"; Regex: \d{4}-\d{2}-\d{2} \d{6}
           nextDigits(4)       && //YYYY
           nextChar(Zstr('-')) && //
           nextDigits(2)       && //MM
           nextChar(Zstr('-')) && //
           nextDigits(2)       && //DD
           nextChar(Zstr(' ')) && //
           nextDigits(6)       && //HHMMSS
           nextStringI(getExtension(shortname)) &&
           it == last;
}


namespace
{
/*
- handle not existing source
- create target super directories if missing
*/
template <class Function>
void moveItemToVersioning(const Zstring& itempath, //throw FileError
                          const Zstring& relativePath,
                          const Zstring& versioningDirectory,
                          const Zstring& timestamp,
                          VersioningStyle versioningStyle,
                          Function moveObj) //move source -> target; may throw FileError
{
    assert(!startsWith(relativePath, FILE_NAME_SEPARATOR));
    assert(!endsWith  (relativePath, FILE_NAME_SEPARATOR));

    Zstring targetPath;
    switch (versioningStyle)
    {
        case VER_STYLE_REPLACE:
            targetPath = appendSeparator(versioningDirectory) + relativePath;
            break;

        case VER_STYLE_ADD_TIMESTAMP:
            //assemble time-stamped version name
            targetPath = appendSeparator(versioningDirectory) + relativePath + Zstr(' ') + timestamp + getExtension(relativePath);
            assert(impl::isMatchingVersion(afterLast(relativePath, FILE_NAME_SEPARATOR), afterLast(targetPath, FILE_NAME_SEPARATOR))); //paranoid? no!
            break;
    }

    try
    {
        moveObj(itempath, targetPath); //throw FileError
    }
    catch (FileError&) //expected to fail if target directory is not yet existing!
    {
        if (!somethingExists(itempath)) //no source at all is not an error (however a directory as source when a file is expected, *is* an error!)
            return; //object *not* processed

        //create intermediate directories if missing
        const Zstring targetDir = beforeLast(targetPath, FILE_NAME_SEPARATOR);
        if (!dirExists(targetDir)) //->(minor) file system race condition!
        {
            makeDirectory(targetDir); //throw FileError
            moveObj(itempath, targetPath); //throw FileError -> this should work now!
        }
        else
            throw;
    }
}


//move source to target across volumes
//no need to check if: - super-directories of target exist - source exists: done by moveItemToVersioning()
//if target already exists, it is overwritten, even if it is a different type, e.g. a directory!
template <class Function>
void moveObject(const Zstring& sourceFile, //throw FileError
                const Zstring& targetFile,
                Function copyDelete) //throw FileError; fallback if move failed
{
    assert(fileExists(sourceFile) || symlinkExists(sourceFile) || !somethingExists(sourceFile)); //we process files and symlinks only

    auto removeTarget = [&]
    {
        //remove target object
        if (dirExists(targetFile)) //directory or dir-symlink
            removeDirectory(targetFile); //throw FileError; we do not expect targetFile to be a directory in general => no callback required
        else //file or (broken) file-symlink
            removeFile(targetFile); //throw FileError
    };

    //first try to move directly without copying
    try
    {
        renameFile(sourceFile, targetFile); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
        return; //great, we get away cheaply!
    }
    //if moving failed treat as error (except when it tried to move to a different volume: in this case we will copy the file)
    catch (const ErrorDifferentVolume&)
    {
        removeTarget(); //throw FileError
        copyDelete();   //
    }
    catch (const ErrorTargetExisting&)
    {
        removeTarget(); //throw FileError
        try
        {
            renameFile(sourceFile, targetFile); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
        }
        catch (const ErrorDifferentVolume&)
        {
            copyDelete(); //throw FileError
        }
    }
}


void moveFile(const Zstring& sourceFile, //throw FileError
              const Zstring& targetFile,
              const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus) //may be nullptr
{
    auto copyDelete = [&]
    {
        assert(!somethingExists(targetFile));

        //create target
        if (symlinkExists(sourceFile))
            copySymlink(sourceFile, targetFile, false); //throw FileError; don't copy filesystem permissions
        else
            copyFile(sourceFile, targetFile, false, true, nullptr, onUpdateCopyStatus); //throw FileError - permissions "false", transactional copy "true"

        //delete source
        removeFile(sourceFile); //throw FileError; newly copied file is NOT deleted if exception is thrown here!
    };

    moveObject(sourceFile, targetFile, copyDelete); //throw FileError
}


void moveDirSymlink(const Zstring& sourceLink, const Zstring& targetLink) //throw FileError
{
    moveObject(sourceLink, //throw FileError
               targetLink,
               [&]
    {
        //create target
        copySymlink(sourceLink, targetLink, false); //throw FileError; don't copy filesystem permissions

        //delete source
        removeDirectory(sourceLink); //throw FileError; newly copied link is NOT deleted if exception is thrown here!
    });
}


class TraverseFilesOneLevel : public TraverseCallback
{
public:
    TraverseFilesOneLevel(std::vector<Zstring>& files, std::vector<Zstring>& dirs) : files_(files), dirs_(dirs) {}

private:
    void onFile(const Zchar* shortName, const Zstring& filepath, const FileInfo& details) override
    {
        files_.push_back(shortName);
    }

    HandleLink onSymlink(const Zchar* shortName, const Zstring& linkpath, const SymlinkInfo& details) override
    {
        if (dirExists(linkpath)) //dir symlink
            dirs_.push_back(shortName);
        else //file symlink, broken symlink
            files_.push_back(shortName);
        return LINK_SKIP;
    }

    TraverseCallback* onDir(const Zchar* shortName, const Zstring& dirpath) override
    {
        dirs_.push_back(shortName);
        return nullptr; //DON'T traverse into subdirs; moveDirectory works recursively!
    }

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                         override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zchar* shortName) override { throw FileError(msg); }

    std::vector<Zstring>& files_;
    std::vector<Zstring>& dirs_;
};
}


bool FileVersioner::revisionFile(const Zstring& filepath, const Zstring& relativePath, const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus) //throw FileError
{
    return revisionFileImpl(filepath, relativePath, nullptr, onUpdateCopyStatus); //throw FileError
}


bool FileVersioner::revisionFileImpl(const Zstring& filepath, //throw FileError
                                     const Zstring& relativePath,
                                     const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove,
                                     const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    bool moveSuccessful = false;

    moveItemToVersioning(filepath, //throw FileError
                         relativePath,
                         versioningDirectory_,
                         timeStamp_,
                         versioningStyle_,
                         [&](const Zstring& source, const Zstring& target)
    {
        if (onBeforeFileMove)
            onBeforeFileMove(source, target); //if we're called by revisionDirImpl() we know that "source" exists!
        //when called by revisionFile(), "source" might not exist, however onBeforeFileMove() is not propagated in this case!

        moveFile(source, target, onUpdateCopyStatus); //throw FileError
        moveSuccessful = true;
    });
    return moveSuccessful;
}


void FileVersioner::revisionDir(const Zstring& dirpath, const Zstring& relativePath, //throw FileError
                                const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove,
                                const std::function<void(const Zstring& dirFrom,  const Zstring& dirTo )>& onBeforeDirMove,
                                const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    //no error situation if directory is not existing! manual deletion relies on it!
    if (!somethingExists(dirpath))
        return; //neither directory nor any other object (e.g. broken symlink) with that name existing
    revisionDirImpl(dirpath, relativePath, onBeforeFileMove, onBeforeDirMove, onUpdateCopyStatus); //throw FileError
}


void FileVersioner::revisionDirImpl(const Zstring& dirpath, const Zstring& relativePath, //throw FileError
                                    const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove,
                                    const std::function<void(const Zstring& dirFrom,  const Zstring& dirTo )>& onBeforeDirMove,
                                    const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus)
{
    assert(somethingExists(dirpath)); //[!]

    //create target
    if (symlinkExists(dirpath)) //on Linux there is just one type of symlink, and since we do revision file symlinks, we should revision dir symlinks as well!
    {
        moveItemToVersioning(dirpath, //throw FileError
                             relativePath,
                             versioningDirectory_,
                             timeStamp_,
                             versioningStyle_,
                             [&](const Zstring& source, const Zstring& target)
        {
            if (onBeforeDirMove)
                onBeforeDirMove(source, target);
            moveDirSymlink(source, target); //throw FileError
        });
    }
    else
    {
        assert(!startsWith(relativePath, FILE_NAME_SEPARATOR));
        assert(endsWith(dirpath, relativePath)); //usually, yes, but we might relax this in the future
        const Zstring targetDir = appendSeparator(versioningDirectory_) + relativePath;

        //makeDirectory(targetDir); //FileError -> create only when needed in moveFileToVersioning(); avoids empty directories

        //traverse source directory one level
        std::vector<Zstring> fileList; //list of *short* names
        std::vector<Zstring> dirList;  //
        {
            TraverseFilesOneLevel tol(fileList, dirList); //throw FileError
            traverseFolder(dirpath, tol);                 //
        }

        const Zstring dirpathPf = appendSeparator(dirpath);
        const Zstring relpathPf = appendSeparator(relativePath);

        //move files
        for (const Zstring& shortname : fileList)
            revisionFileImpl(dirpathPf + shortname, //throw FileError
                             relpathPf + shortname,
                             onBeforeFileMove, onUpdateCopyStatus);

        //move items in subdirectories
        for (const Zstring& shortname : dirList)
            revisionDirImpl(dirpathPf + shortname, //throw FileError
                            relpathPf + shortname,
                            onBeforeFileMove, onBeforeDirMove, onUpdateCopyStatus);

        //delete source
        if (onBeforeDirMove)
            onBeforeDirMove(dirpath, targetDir);
        removeDirectory(dirpath); //throw FileError
    }
}


/*
namespace
{
class TraverseVersionsOneLevel : public TraverseCallback
{
public:
    TraverseVersionsOneLevel(std::vector<Zstring>& files, std::function<void()> updateUI) : files_(files), updateUI_(updateUI) {}

private:
    void onFile(const Zchar* shortName, const Zstring& filepath, const FileInfo& details) override { files_.push_back(shortName); updateUI_(); }
    HandleLink onSymlink(const Zchar* shortName, const Zstring& linkpath, const SymlinkInfo& details) override { files_.push_back(shortName); updateUI_(); return LINK_SKIP; }
    std::shared_ptr<TraverseCallback> onDir(const Zchar* shortName, const Zstring& dirpath) override { updateUI_(); return nullptr; } //DON'T traverse into subdirs
    HandleError reportDirError (const std::wstring& msg)                         override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, const Zchar* shortName) override { throw FileError(msg); }

    std::vector<Zstring>& files_;
    std::function<void()> updateUI_;
};
}

void FileVersioner::limitVersions(std::function<void()> updateUI) //throw FileError
{
    if (versionCountLimit_ < 0) //no limit!
        return;

    //buffer map "directory |-> list of immediate child file and symlink short names"
    std::map<Zstring, std::vector<Zstring>, LessFilename> dirBuffer;

    auto getVersionsBuffered = [&](const Zstring& dirpath) -> const std::vector<Zstring>&
    {
        auto it = dirBuffer.find(dirpath);
        if (it != dirBuffer.end())
            return it->second;

        std::vector<Zstring> fileShortNames;
        TraverseVersionsOneLevel tol(fileShortNames, updateUI); //throw FileError
        traverseFolder(dirpath, tol);

        auto& newEntry = dirBuffer[dirpath]; //transactional behavior!!!
        newEntry.swap(fileShortNames);       //-> until C++11 emplace is available

        return newEntry;
    };

    std::for_each(fileRelNames.begin(), fileRelNames.end(),
                  [&](const Zstring& relativePath) //e.g. "subdir\Sample.txt"
    {
        const Zstring filepath = appendSeparator(versioningDirectory_) + relativePath; //e.g. "D:\Revisions\subdir\Sample.txt"
        const Zstring parentDir = beforeLast(filepath, FILE_NAME_SEPARATOR);    //e.g. "D:\Revisions\subdir"
        const Zstring shortname = afterLast(relativePath, FILE_NAME_SEPARATOR); //e.g. "Sample.txt"; returns the whole string if seperator not found

        const std::vector<Zstring>& allVersions = getVersionsBuffered(parentDir);

        //filter out only those versions that match the given relative name
        std::vector<Zstring> matches; //e.g. "Sample.txt 2012-05-15 131513.txt"

        std::copy_if(allVersions.begin(), allVersions.end(), std::back_inserter(matches),
        [&](const Zstring& shortnameVer) { return impl::isMatchingVersion(shortname, shortnameVer); });

        //take advantage of version naming convention to find oldest versions
        if (matches.size() <= static_cast<size_t>(versionCountLimit_))
            return;
        std::nth_element(matches.begin(), matches.end() - versionCountLimit_, matches.end(), LessFilename()); //windows: ignore case!

        //delete obsolete versions
        std::for_each(matches.begin(), matches.end() - versionCountLimit_,
                      [&](const Zstring& shortnameVer)
        {
            updateUI();
            const Zstring fullnameVer = parentDir + FILE_NAME_SEPARATOR + shortnameVer;
            try
            {
                removeFile(fullnameVer); //throw FileError
            }
            catch (FileError&)
            {
#ifdef ZEN_WIN //if it's a directory symlink:
                if (symlinkExists(fullnameVer) && dirExists(fullnameVer))
                    removeDirectory(fullnameVer); //throw FileError
                else
#endif
                    throw;
            }
        });
    });
}
*/
