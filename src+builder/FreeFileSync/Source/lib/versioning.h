// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef VERSIONING_HEADER_8760247652438056
#define VERSIONING_HEADER_8760247652438056

#include <vector>
#include <functional>
#include <zen/time.h>
#include <zen/zstring.h>
#include <zen/file_error.h>
#include "../structures.h"

namespace zen
{
//e.g. move C:\Source\subdir\Sample.txt -> D:\Revisions\subdir\Sample.txt 2012-05-15 131513.txt
//scheme: <revisions directory>\<relpath>\<filename>.<ext> YYYY-MM-DD HHMMSS.<ext>
/*
	- ignores missing source files/dirs
	- creates missing intermediate directories
	- does not create empty directories
	- handles symlinks
	- replaces already existing target files/dirs (supports retry)
		=> (unlikely) risk of data loss for naming convention "versioning":
		race-condition if two FFS instances start at the very same second OR multiple folder pairs process the same filepath!!
*/

class FileVersioner
{
public:
    FileVersioner(const Zstring& versioningDirectory, //throw FileError
                  VersioningStyle versioningStyle,
                  const TimeComp& timeStamp) : //max versions per file; < 0 := no limit
        versioningStyle_(versioningStyle),
        versioningDirectory_(versioningDirectory),
        timeStamp_(formatTime<Zstring>(Zstr("%Y-%m-%d %H%M%S"), timeStamp)) //e.g. "2012-05-15 131513"
    {
        if (timeStamp_.size() != 17) //formatTime() returns empty string on error; unexpected length: e.g. problem in year 10000!
            throw FileError(_("Unable to create time stamp for versioning:") + L" \"" + timeStamp_ + L"\"");
    }

    bool revisionFile(const Zstring& filepath, //throw FileError; return "false" if file is not existing
                      const Zstring& relativePath,

                      //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                      const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus); //may be nullptr

    void revisionDir (const Zstring& dirpath,  const Zstring& relativePath, //throw FileError

                      //optional callbacks: may be nullptr
                      const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove, //one call for each *existing* object!
                      const std::function<void(const Zstring& dirFrom,  const Zstring& dirTo )>& onBeforeDirMove,  //
                      //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                      const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus);

    //void limitVersions(std::function<void()> updateUI); //throw FileError; call when done revisioning!

private:
    bool revisionFileImpl(const Zstring& filepath, const Zstring& relativePath,
                          const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove,
                          const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus); //throw FileError

    void revisionDirImpl (const Zstring& filepath,  const Zstring& relativePath,
                          const std::function<void(const Zstring& fileFrom, const Zstring& fileTo)>& onBeforeFileMove,
                          const std::function<void(const Zstring& dirFrom,  const Zstring& dirTo )>& onBeforeDirMove,
                          const std::function<void(std::int64_t bytesDelta)>& onUpdateCopyStatus); //throw FileError

    const VersioningStyle versioningStyle_;
    const Zstring versioningDirectory_;
    const Zstring timeStamp_;

    //std::vector<Zstring> fileRelNames; //store list of revisioned file and symlink relative names for limitVersions()
};

namespace impl //declare for unit tests:
{
bool isMatchingVersion(const Zstring& shortname, const Zstring& shortnameVersion);
}
}

#endif //VERSIONING_HEADER_8760247652438056
