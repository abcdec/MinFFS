// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "comparison.h"
#include <stdexcept>
#include <numeric>
#include <zen/perf.h>
#include <zen/scope_guard.h>
#include <zen/process_priority.h>
#include <zen/symlink_target.h>
#include <zen/format_unit.h>
#include <zen/stl_tools.h>
#include "algorithm.h"
#include "lib/parallel_scan.h"
#include "lib/resolve_path.h"
#include "lib/dir_exist_async.h"
#include "lib/binary.h"
#include "lib/cmp_filetime.h"
#include "lib/status_handler_impl.h"
#include "lib/parallel_scan.h"

using namespace zen;


std::vector<FolderPairCfg> zen::extractCompareCfg(const MainConfiguration& mainCfg, int fileTimeTolerance)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    std::vector<FolderPairCfg> output;
    std::transform(allPairs.begin(), allPairs.end(), std::back_inserter(output),
                   [&](const FolderPairEnh& enhPair) -> FolderPairCfg
    {
        return FolderPairCfg(enhPair.dirpathPhraseLeft, enhPair.dirpathPhraseRight,
        enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->compareVar       : mainCfg.cmpConfig.compareVar,
        enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->handleSymlinks   : mainCfg.cmpConfig.handleSymlinks,
        fileTimeTolerance,
        enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->optTimeShiftHours : mainCfg.cmpConfig.optTimeShiftHours,

        normalizeFilters(mainCfg.globalFilter, enhPair.localFilter),

        enhPair.altSyncConfig.get() ? enhPair.altSyncConfig->directionCfg : mainCfg.syncCfg.directionCfg);
    });
    return output;
}

//------------------------------------------------------------------------------------------
namespace
{
struct ResolvedFolderPair
{
    ResolvedFolderPair(const Zstring& left, const Zstring& right) :
        dirpathLeft(left),
        dirpathRight(right) {}

    Zstring dirpathLeft;  //resolved directory names
    Zstring dirpathRight; //
};


std::vector<ResolvedFolderPair> resolveDirectoryNames(const std::vector<FolderPairCfg>& cfgList)
{
    std::vector<ResolvedFolderPair> output;

    for (const FolderPairCfg& fpCfg : cfgList)
        output.emplace_back(getFormattedDirectoryPath(fpCfg.dirpathPhraseLeft),
                            getFormattedDirectoryPath(fpCfg.dirpathPhraseRight));
    warn_static("get volume by name for idle HDD! => call async getFormattedDirectoryPath, but currently not thread-safe")
    return output;
}


struct ResolutionInfo
{
    std::vector<ResolvedFolderPair> resolvedPairs;
    std::set<Zstring, LessFilename> existingDirs;
};


ResolutionInfo resolveFolderPairs(const std::vector<FolderPairCfg>& cfgList,
                                  bool allowUserInteraction,
                                  ProcessCallback& callback)
{
    ResolutionInfo output;

    tryReportingError([&]
    {
        //support "retry" for environment variable and and variable driver letter resolution!
        output.resolvedPairs = resolveDirectoryNames(cfgList);
        assert(output.resolvedPairs.size() == cfgList.size()); //postcondition!

        std::set<Zstring, LessFilename> dirpaths;
        for (const ResolvedFolderPair& fp : output.resolvedPairs)
        {
            dirpaths.insert(fp.dirpathLeft);
            dirpaths.insert(fp.dirpathRight);
        }

        const DirectoryStatus dirStatus = getExistingDirsUpdating(dirpaths, allowUserInteraction, callback); //check *all* directories on each try!
        output.existingDirs = dirStatus.existing;

        if (!dirStatus.missing.empty())
        {
            std::wstring msg = _("Cannot find the following folders:") + L"\n";
            for (const Zstring& dirpath : dirStatus.missing)
                msg += std::wstring(L"\n") + dirpath;
            throw FileError(msg, _("You can ignore this error to consider each folder as empty. The folders then will be created automatically during synchronization."));
        }
    }, callback); //throw X?

    return output;
}


void checkForIncompleteInput(const std::vector<ResolvedFolderPair>& folderPairs, bool& warningInputFieldEmpty, ProcessCallback& callback)
{
    bool havePartialPair = false;
    bool haveFullPair    = false;

    for (const ResolvedFolderPair& fp : folderPairs)
        if (fp.dirpathLeft.empty() != fp.dirpathRight.empty())
            havePartialPair = true;
        else if (!fp.dirpathLeft.empty())
            haveFullPair = true;

    if (havePartialPair == haveFullPair) //error if: all empty or exist both full and partial pairs -> support single-dir scenario
        callback.reportWarning(_("A folder input field is empty.") + L" \n\n" +
                               _("The corresponding folder will be considered as empty."), warningInputFieldEmpty);
}


//check whether one side is subdirectory of other side (folder pair wise!)
//similar check if one directory is read/written by multiple pairs not before beginning of synchronization
void checkFolderDependency(const std::vector<ResolvedFolderPair>& folderPairs, bool& warningDependentFolders, ProcessCallback& callback) //returns warning message, empty if all ok
{
    std::vector<std::pair<Zstring, Zstring>> dependentDirs;

    auto havePathDependency = [](const Zstring& lhs, const Zstring& rhs)
    {
        return EqualFilename()(Zstring(lhs.c_str(), std::min(lhs.length(), rhs.length())), //note: this is NOT an equivalence relation!
                               Zstring(rhs.c_str(), std::min(lhs.length(), rhs.length())));
    };

    for (const ResolvedFolderPair& fp : folderPairs)
        if (!fp.dirpathLeft.empty() && !fp.dirpathRight.empty()) //empty folders names may be accepted by user
        {
            if (havePathDependency(fp.dirpathLeft, fp.dirpathRight)) //test wheter leftDirectory begins with rightDirectory or the other way round
                dependentDirs.emplace_back(fp.dirpathLeft, fp.dirpathRight);
        }

    if (!dependentDirs.empty())
    {
        std::wstring warningMsg = _("The following folder paths are dependent from each other:");
        for (auto it = dependentDirs.begin(); it != dependentDirs.end(); ++it)
            warningMsg += std::wstring(L"\n\n") +
                          it->first + L"\n" +
                          it->second;

        callback.reportWarning(warningMsg, warningDependentFolders);
    }
}

//#############################################################################################################################

class ComparisonBuffer
{
public:
    ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, ProcessCallback& callback);

    //create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedLinks are appended!
    std::shared_ptr<BaseDirPair> compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::list<std::shared_ptr<BaseDirPair>> compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const;

private:
    ComparisonBuffer           (const ComparisonBuffer&) = delete;
    ComparisonBuffer& operator=(const ComparisonBuffer&) = delete;

    std::shared_ptr<BaseDirPair> performComparison(const ResolvedFolderPair& fp,
                                                   const FolderPairCfg& fpCfg,
                                                   std::vector<FilePair*>& undefinedFiles,
                                                   std::vector<SymlinkPair*>& undefinedLinks) const;

    std::map<DirectoryKey, DirectoryValue> directoryBuffer; //contains only *existing* directories
    ProcessCallback& callback_;
};


ComparisonBuffer::ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, ProcessCallback& callback) : callback_(callback)
{
    class CbImpl : public FillBufferCallback
    {
    public:
        CbImpl(ProcessCallback& pcb) :
            callback_(pcb),
            itemsReported(0) {}

        void reportStatus(const std::wstring& statusMsg, int itemsTotal) override
        {
            callback_.updateProcessedData(itemsTotal - itemsReported, 0); //processed bytes are reported in subfunctions!
            itemsReported = itemsTotal;

            callback_.reportStatus(statusMsg); //may throw
            //callback_.requestUiRefresh(); //already called by reportStatus()
        }

        HandleError reportError(const std::wstring& msg, size_t retryNumber) override
        {
            switch (callback_.reportError(msg, retryNumber))
            {
                case ProcessCallback::IGNORE_ERROR:
                    return ON_ERROR_IGNORE;

                case ProcessCallback::RETRY:
                    return ON_ERROR_RETRY;
            }

            assert(false);
            return ON_ERROR_IGNORE;
        }

    private:
        ProcessCallback& callback_;
        int itemsReported;
    } cb(callback);

    fillBuffer(keysToRead, //in
               directoryBuffer, //out
               cb,
               UI_UPDATE_INTERVAL / 2); //every ~50 ms
}


//--------------------assemble conflict descriptions---------------------------

//const wchar_t arrowLeft [] = L"\u2190";
//const wchar_t arrowRight[] = L"\u2192"; unicode arrows -> too small
const wchar_t arrowLeft [] = L"<--";
const wchar_t arrowRight[] = L"-->";


//check for very old dates or dates in the future
std::wstring getConflictInvalidDate(const Zstring& fileNameFull, std::int64_t utcTime)
{
    return replaceCpy(_("File %x has an invalid date."), L"%x", fmtFileName(fileNameFull)) + L"\n" +
           _("Date:") + L" " + utcToLocalTimeString(utcTime);
}


//check for changed files with same modification date
std::wstring getConflictSameDateDiffSize(const FilePair& fileObj)
{
    return replaceCpy(_("Files %x have the same date but a different size."), L"%x", fmtFileName(fileObj.getPairRelativePath())) + L"\n" +
           L"    " + arrowLeft  + L" " + _("Date:") + L" " + utcToLocalTimeString(fileObj.getLastWriteTime<LEFT_SIDE >()) + L"    " + _("Size:") + L" " + toGuiString(fileObj.getFileSize<LEFT_SIDE>()) + L"\n" +
           L"    " + arrowRight + L" " + _("Date:") + L" " + utcToLocalTimeString(fileObj.getLastWriteTime<RIGHT_SIDE>()) + L"    " + _("Size:") + L" " + toGuiString(fileObj.getFileSize<RIGHT_SIDE>());
}


std::wstring getConflictSkippedBinaryComparison(const FilePair& fileObj)
{
    return replaceCpy(_("Content comparison was skipped for excluded files %x."), L"%x", fmtFileName(fileObj.getPairRelativePath()));
}


std::wstring getDescrDiffMetaShortnameCase(const FileSystemObject& fsObj)
{
    return _("Items differ in attributes only") + L"\n" +
           L"    " + arrowLeft  + L" " + fmtFileName(fsObj.getItemName<LEFT_SIDE >()) + L"\n" +
           L"    " + arrowRight + L" " + fmtFileName(fsObj.getItemName<RIGHT_SIDE>());
}


template <class FileOrLinkPair>
std::wstring getDescrDiffMetaDate(const FileOrLinkPair& fileObj)
{
    return _("Items differ in attributes only") + L"\n" +
           L"    " + arrowLeft  + L" " + _("Date:") + L" " + utcToLocalTimeString(fileObj.template getLastWriteTime<LEFT_SIDE >()) + L"\n" +
           L"    " + arrowRight + L" " + _("Date:") + L" " + utcToLocalTimeString(fileObj.template getLastWriteTime<RIGHT_SIDE>());
}

//-----------------------------------------------------------------------------

void categorizeSymlinkByTime(SymlinkPair& linkObj, int fileTimeTolerance, unsigned int optTimeShiftHours)
{
    //categorize symlinks that exist on both sides
    switch (compareFileTime(linkObj.getLastWriteTime<LEFT_SIDE>(),
                            linkObj.getLastWriteTime<RIGHT_SIDE>(), fileTimeTolerance, optTimeShiftHours))
    {
        case TimeResult::EQUAL:
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncDir's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp

            if (linkObj.getItemName<LEFT_SIDE>() == linkObj.getItemName<RIGHT_SIDE>())
                linkObj.setCategory<FILE_EQUAL>();
            else
                linkObj.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(linkObj));
            break;

        case TimeResult::LEFT_NEWER:
            linkObj.setCategory<FILE_LEFT_NEWER>();
            break;

        case TimeResult::RIGHT_NEWER:
            linkObj.setCategory<FILE_RIGHT_NEWER>();
            break;

        case TimeResult::LEFT_INVALID:
            linkObj.setCategoryConflict(getConflictInvalidDate(linkObj.getFullPath<LEFT_SIDE>(), linkObj.getLastWriteTime<LEFT_SIDE>()));
            break;

        case TimeResult::RIGHT_INVALID:
            linkObj.setCategoryConflict(getConflictInvalidDate(linkObj.getFullPath<RIGHT_SIDE>(), linkObj.getLastWriteTime<RIGHT_SIDE>()));
            break;
    }
}


std::shared_ptr<BaseDirPair> ComparisonBuffer::compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    std::shared_ptr<BaseDirPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* linkObj : uncategorizedLinks)
        categorizeSymlinkByTime(*linkObj, fpConfig.fileTimeTolerance, fpConfig.optTimeShiftHours);

    //categorize files that exist on both sides
    for (FilePair* fileObj : uncategorizedFiles)
    {
        switch (compareFileTime(fileObj->getLastWriteTime<LEFT_SIDE>(),
                                fileObj->getLastWriteTime<RIGHT_SIDE>(), fpConfig.fileTimeTolerance, fpConfig.optTimeShiftHours))
        {
            case TimeResult::EQUAL:
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncDir's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp
                if (fileObj->getFileSize<LEFT_SIDE>() == fileObj->getFileSize<RIGHT_SIDE>())
                {
                    if (fileObj->getItemName<LEFT_SIDE>() == fileObj->getItemName<RIGHT_SIDE>())
                        fileObj->setCategory<FILE_EQUAL>();
                    else
                        fileObj->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*fileObj));
                }
                else
                    fileObj->setCategoryConflict(getConflictSameDateDiffSize(*fileObj)); //same date, different filesize
                break;

            case TimeResult::LEFT_NEWER:
                fileObj->setCategory<FILE_LEFT_NEWER>();
                break;

            case TimeResult::RIGHT_NEWER:
                fileObj->setCategory<FILE_RIGHT_NEWER>();
                break;

            case TimeResult::LEFT_INVALID:
                fileObj->setCategoryConflict(getConflictInvalidDate(fileObj->getFullPath<LEFT_SIDE>(), fileObj->getLastWriteTime<LEFT_SIDE>()));
                break;

            case TimeResult::RIGHT_INVALID:
                fileObj->setCategoryConflict(getConflictInvalidDate(fileObj->getFullPath<RIGHT_SIDE>(), fileObj->getLastWriteTime<RIGHT_SIDE>()));
                break;
        }
    }
    return output;
}


void categorizeSymlinkByContent(SymlinkPair& linkObj, int fileTimeTolerance, unsigned int optTimeShiftHours, ProcessCallback& callback)
{
    //categorize symlinks that exist on both sides
    Zstring targetPathRawL;
    Zstring targetPathRawR;
    Opt<std::wstring> errMsg = tryReportingError([&]
    {
        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtFileName(linkObj.getFullPath<LEFT_SIDE>())));
        targetPathRawL = getSymlinkTargetRaw(linkObj.getFullPath<LEFT_SIDE>()); //throw FileError

        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtFileName(linkObj.getFullPath<RIGHT_SIDE>())));
        targetPathRawR = getSymlinkTargetRaw(linkObj.getFullPath<RIGHT_SIDE>()); //throw FileError
    }, callback); //throw X?

    if (errMsg)
        linkObj.setCategoryConflict(*errMsg);
    else
    {
        if (targetPathRawL == targetPathRawR
#ifdef ZEN_WIN //type of symbolic link is relevant for Windows only
            &&
            dirExists(linkObj.getFullPath<LEFT_SIDE >()) == //check if dir-symlink
            dirExists(linkObj.getFullPath<RIGHT_SIDE>())    //
#endif
           )
        {
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncDir's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp

            //symlinks have same "content"
            if (linkObj.getItemName<LEFT_SIDE>() != linkObj.getItemName<RIGHT_SIDE>())
                linkObj.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(linkObj));
            else if (!sameFileTime(linkObj.getLastWriteTime<LEFT_SIDE>(),
                                   linkObj.getLastWriteTime<RIGHT_SIDE>(), fileTimeTolerance, optTimeShiftHours))
                linkObj.setCategoryDiffMetadata(getDescrDiffMetaDate(linkObj));
            else
                linkObj.setCategory<FILE_EQUAL>();
        }
        else
            linkObj.setCategory<FILE_DIFFERENT_CONTENT>();
    }
}


std::list<std::shared_ptr<BaseDirPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
{
    std::list<std::shared_ptr<BaseDirPair>> output;
    if (workLoad.empty())
        return output;

    //PERF_START;
    std::vector<FilePair*> filesToCompareBytewise;

    //process folder pairs one after another
    for (const auto& w : workLoad)
    {
        std::vector<FilePair*> undefinedFiles;
        std::vector<SymlinkPair*> uncategorizedLinks;
        //do basis scan and retrieve candidates for binary comparison (files existing on both sides)

        output.push_back(performComparison(w.first, w.second, undefinedFiles, uncategorizedLinks));

        //content comparison of file content happens AFTER finding corresponding files and AFTER filtering
        //in order to separate into two processes (scanning and comparing)
        for (FilePair* fileObj : undefinedFiles)
            //pre-check: files have different content if they have a different filesize (must not be FILE_EQUAL: see InSyncFile)
            if (fileObj->getFileSize<LEFT_SIDE>() != fileObj->getFileSize<RIGHT_SIDE>())
                fileObj->setCategory<FILE_DIFFERENT_CONTENT>();
            else
            {
                //perf: skip binary comparison for excluded rows (e.g. via time span and size filter)!
                //both soft and hard filter were already applied in ComparisonBuffer::performComparison()!
                if (!fileObj->isActive())
                    fileObj->setCategoryConflict(getConflictSkippedBinaryComparison(*fileObj));
                else
                    filesToCompareBytewise.push_back(fileObj);
            }

        //finish symlink categorization
        for (SymlinkPair* linkObj : uncategorizedLinks)
            categorizeSymlinkByContent(*linkObj, w.second.fileTimeTolerance, w.second.optTimeShiftHours, callback_);
    }

    //finish categorization...
    const size_t objectsTotal = filesToCompareBytewise.size();

    std::uint64_t bytesTotal = 0; //left and right filesizes are equal
    for (FilePair* fileObj : filesToCompareBytewise)
        bytesTotal += fileObj->getFileSize<LEFT_SIDE>();

    callback_.initNewPhase(static_cast<int>(objectsTotal), //may throw
                           bytesTotal,
                           ProcessCallback::PHASE_COMPARING_CONTENT);

    const std::wstring txtComparingContentOfFiles = _("Comparing content of files %x");

    //compare files (that have same size) bytewise...
    for (FilePair* fileObj : filesToCompareBytewise)
    {
        callback_.reportStatus(replaceCpy(txtComparingContentOfFiles, L"%x", fmtFileName(fileObj->getPairRelativePath()), false));

        //check files that exist in left and right model but have different content

        bool haveSameContent = false;
        Opt<std::wstring> errMsg = tryReportingError([&]
        {
            StatisticsReporter statReporter(1, fileObj->getFileSize<LEFT_SIDE>(), callback_);

            auto onUpdateStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            haveSameContent = filesHaveSameContent(fileObj->getFullPath<LEFT_SIDE>(),
                                                   fileObj->getFullPath<RIGHT_SIDE>(), onUpdateStatus); //throw FileError
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        }, callback_); //throw X?

        if (errMsg)
            fileObj->setCategoryConflict(*errMsg);
        else
        {
            if (haveSameContent)
            {
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncDir's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp
                if (fileObj->getItemName<LEFT_SIDE>() != fileObj->getItemName<RIGHT_SIDE>())
                    fileObj->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*fileObj));
                else if (!sameFileTime(fileObj->getLastWriteTime<LEFT_SIDE>(),
                                       fileObj->getLastWriteTime<RIGHT_SIDE>(), fileObj->root().getFileTimeTolerance(), fileObj->root().getTimeShift()))
                    fileObj->setCategoryDiffMetadata(getDescrDiffMetaDate(*fileObj));
                else
                    fileObj->setCategory<FILE_EQUAL>();
            }
            else
                fileObj->setCategory<FILE_DIFFERENT_CONTENT>();
        }
    }
    return output;
}

//-----------------------------------------------------------------------------------------------

class MergeSides
{
public:
    MergeSides(const std::map<Zstring, std::wstring, LessFilename>& failedItemReads,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedLinksOut) :
        failedItemReads_(failedItemReads),
        undefinedFiles(undefinedFilesOut),
        undefinedLinks(undefinedLinksOut) {}

    void execute(const DirContainer& leftSide, const DirContainer& rightSide, HierarchyObject& output)
    {
        auto it = failedItemReads_.find(Zstring()); //empty path if read-error for whole base directory

        mergeTwoSides(leftSide, rightSide,
                      it != failedItemReads_.end() ? &it->second : nullptr,
                      output);
    }

private:
    void mergeTwoSides(const DirContainer& leftSide, const DirContainer& rightSide, const std::wstring* errorMsg, HierarchyObject& output);

    template <SelectedSide side>
    void fillOneSide(const DirContainer& dirCont, const std::wstring* errorMsg, HierarchyObject& output);

    const std::wstring* checkFailedRead(FileSystemObject& fsObj, const std::wstring* errorMsg);

    const std::map<Zstring, std::wstring, LessFilename>& failedItemReads_; //base-relative paths or empty if read-error for whole base directory
    std::vector<FilePair*>& undefinedFiles;
    std::vector<SymlinkPair*>& undefinedLinks;
};


inline
const std::wstring* MergeSides::checkFailedRead(FileSystemObject& fsObj, const std::wstring* errorMsg)
{
    if (!errorMsg)
    {
        auto it = failedItemReads_.find(fsObj.getPairRelativePath());
        if (it != failedItemReads_.end())
            errorMsg = &it->second;
    }

    if (errorMsg)
    {
        fsObj.setActive(false);
        fsObj.setCategoryConflict(*errorMsg);
    }
    return errorMsg;
}


template <SelectedSide side>
void MergeSides::fillOneSide(const DirContainer& dirCont, const std::wstring* errorMsg, HierarchyObject& output)
{
    for (const auto& file : dirCont.files)
    {
        FilePair& newItem = output.addSubFile<side>(file.first, file.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& link : dirCont.links)
    {
        SymlinkPair& newItem = output.addSubLink<side>(link.first, link.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& dir : dirCont.dirs)
    {
        DirPair& newDir = output.addSubDir<side>(dir.first);
        const std::wstring* errorMsgNew = checkFailedRead(newDir, errorMsg);
        fillOneSide<side>(dir.second, errorMsgNew, newDir); //recurse
    }
}


//improve merge-perf by over 70% + more natural default sequence
template <class MapType, class ProcessLeftOnly, class ProcessRightOnly, class ProcessBoth> inline
void linearMerge(const MapType& mapLeft, const MapType& mapRight, ProcessLeftOnly lo, ProcessRightOnly ro, ProcessBoth bo)
{
    const auto lessVal = mapLeft.value_comp();

    auto iterLeft  = mapLeft .begin();
    auto iterRight = mapRight.begin();

    auto finishLeft  = [&] { std::for_each(iterLeft,  mapLeft .end(), lo); };
    auto finishRight = [&] { std::for_each(iterRight, mapRight.end(), ro); };

    if (iterLeft  == mapLeft .end()) return finishRight();
    if (iterRight == mapRight.end()) return finishLeft();

    for (;;)
        if (lessVal(*iterLeft, *iterRight))
        {
            lo(*iterLeft);
            if (++iterLeft == mapLeft.end())
                return finishRight();
        }
        else if (lessVal(*iterRight, *iterLeft))
        {
            ro(*iterRight);
            if (++iterRight == mapRight.end())
                return finishLeft();
        }
        else
        {
            bo(*iterLeft, *iterRight);
            ++iterLeft;  //
            ++iterRight; //increment BOTH before checking for end of range!
            if (iterLeft  == mapLeft .end()) return finishRight();
            if (iterRight == mapRight.end()) return finishLeft();
        }
}


void MergeSides::mergeTwoSides(const DirContainer& leftSide, const DirContainer& rightSide, const std::wstring* errorMsg, HierarchyObject& output)
{
    typedef const DirContainer::FileList::value_type FileData;

    linearMerge(leftSide.files, rightSide.files,
    [&](const FileData& fileLeft ) { FilePair& newItem = output.addSubFile<LEFT_SIDE >(fileLeft .first, fileLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const FileData& fileRight) { FilePair& newItem = output.addSubFile<RIGHT_SIDE>(fileRight.first, fileRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const FileData& fileLeft, const FileData& fileRight) //both sides
    {
        FilePair& newItem = output.addSubFile(fileLeft.first,
                                              fileLeft.second,
                                              FILE_EQUAL, //dummy-value until categorization is finished later
                                              fileRight.first,
                                              fileRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedFiles.push_back(&newItem);
        static_assert(IsSameType<HierarchyObject::SubFileVec, FixedList<FilePair>>::value, ""); //HierarchyObject::addSubFile() must NOT invalidate references used in "undefinedFiles"!
    });

    //-----------------------------------------------------------------------------------------------
    typedef const DirContainer::LinkList::value_type LinkData;

    linearMerge(leftSide.links, rightSide.links,
    [&](const LinkData& linkLeft)  { SymlinkPair& newItem = output.addSubLink<LEFT_SIDE >(linkLeft .first, linkLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const LinkData& linkRight) { SymlinkPair& newItem = output.addSubLink<RIGHT_SIDE>(linkRight.first, linkRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const LinkData& linkLeft, const LinkData& linkRight) //both sides
    {
        SymlinkPair& newItem = output.addSubLink(linkLeft.first,
                                                 linkLeft.second,
                                                 SYMLINK_EQUAL, //dummy-value until categorization is finished later
                                                 linkRight.first,
                                                 linkRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedLinks.push_back(&newItem);
    });

    //-----------------------------------------------------------------------------------------------
    typedef const DirContainer::DirList::value_type DirData;

    linearMerge(leftSide.dirs, rightSide.dirs,
                [&](const DirData& dirLeft) //left only
    {
        DirPair& newDir = output.addSubDir<LEFT_SIDE>(dirLeft.first);
        const std::wstring* errorMsgNew = checkFailedRead(newDir, errorMsg);
        this->fillOneSide<LEFT_SIDE>(dirLeft.second, errorMsgNew, newDir); //recurse
    },
    [&](const DirData& dirRight) //right only
    {
        DirPair& newDir = output.addSubDir<RIGHT_SIDE>(dirRight.first);
        const std::wstring* errorMsgNew = checkFailedRead(newDir, errorMsg);
        this->fillOneSide<RIGHT_SIDE>(dirRight.second, errorMsgNew, newDir); //recurse
    },

    [&](const DirData& dirLeft, const DirData& dirRight) //both sides
    {
        DirPair& newDir = output.addSubDir(dirLeft.first, dirRight.first, DIR_EQUAL);
        const std::wstring* errorMsgNew = checkFailedRead(newDir, errorMsg);

        if (!errorMsgNew)
            if (dirLeft.first != dirRight.first)
                newDir.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(newDir));

        mergeTwoSides(dirLeft.second, dirRight.second, errorMsgNew, newDir); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//mark excluded directories (see fillBuffer()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(HierarchyObject& hierObj, const HardFilter& filterProc)
{
    for (DirPair& dirObj : hierObj.refSubDirs())
        stripExcludedDirectories(dirObj, filterProc);

    //remove superfluous directories:
    //   this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
    //	 and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to zen::FixedList!
    hierObj.refSubDirs().remove_if([&](DirPair& dirObj)
    {
        const bool included = filterProc.passDirFilter(dirObj.getPairRelativePath(), nullptr); //subObjMightMatch is false, child items were already excluded during scanning

        if (!included) //falsify only! (e.g. might already be inactive due to read error!)
            dirObj.setActive(false);

        return !included & //don't check active status, but eval filter directly!
               dirObj.refSubDirs ().empty() &&
               dirObj.refSubLinks().empty() &&
               dirObj.refSubFiles().empty();
    });
}


//create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedLinks are appended!
std::shared_ptr<BaseDirPair> ComparisonBuffer::performComparison(const ResolvedFolderPair& fp,
                                                                 const FolderPairCfg& fpCfg,
                                                                 std::vector<FilePair*>& undefinedFiles,
                                                                 std::vector<SymlinkPair*>& undefinedLinks) const
{
    callback_.reportStatus(_("Generating file list..."));
    callback_.forceUiRefresh();

    auto getDirValue = [&](const Zstring& dirpathFmt) -> const DirectoryValue*
    {
        auto it = directoryBuffer.find(DirectoryKey(dirpathFmt, fpCfg.filter.nameFilter, fpCfg.handleSymlinks));
        return it != directoryBuffer.end() ? &it->second : nullptr;
    };

    const DirectoryValue* bufValueLeft  = getDirValue(fp.dirpathLeft);
    const DirectoryValue* bufValueRight = getDirValue(fp.dirpathRight);

    std::map<Zstring, std::wstring, LessFilename> failedReads; //base-relative paths or empty if read-error for whole base directory
    {
        //mix failedDirReads with failedItemReads:
        //mark directory errors already at directory-level (instead for child items only) to show on GUI! See "MergeSides"
        //=> minor pessimization for "excludefilterFailedRead" which needlessly excludes parent folders, too
        if (bufValueLeft ) set_append(failedReads, bufValueLeft ->failedDirReads);
        if (bufValueRight) set_append(failedReads, bufValueRight->failedDirReads);

        if (bufValueLeft ) set_append(failedReads, bufValueLeft ->failedItemReads);
        if (bufValueRight) set_append(failedReads, bufValueRight->failedItemReads);
    }

    Zstring excludefilterFailedRead;
	if (failedReads.find(Zstring()) != failedReads.end()) //empty path if read-error for whole base directory
		excludefilterFailedRead += Zstr("*\n");
	else
		for (const auto& item : failedReads)
			excludefilterFailedRead += item.first + Zstr("\n"); //exclude item AND (potential) child items!

    std::shared_ptr<BaseDirPair> output = std::make_shared<BaseDirPair>(fp.dirpathLeft,
                                                                        bufValueLeft != nullptr, //dir existence must be checked only once: available iff buffer entry exists!
                                                                        fp.dirpathRight,
                                                                        bufValueRight != nullptr,
                                                                        fpCfg.filter.nameFilter->copyFilterAddingExclusion(excludefilterFailedRead),
                                                                        fpCfg.compareVar,
                                                                        fpCfg.fileTimeTolerance,
                                                                        fpCfg.optTimeShiftHours);

    //PERF_START;
    DirContainer emptyDirCont; //WTF!!! => using a temporary in the ternary conditional would implicitly call the DirContainer copy-constructor!!!!!!
    MergeSides(failedReads, undefinedFiles, undefinedLinks).execute(bufValueLeft  ? bufValueLeft ->dirCont : emptyDirCont,
                                                                    bufValueRight ? bufValueRight->dirCont : emptyDirCont, *output);
    //PERF_STOP;

    //##################### in/exclude rows according to filtering #####################
    //NOTE: we need to finish de-activating rows BEFORE binary comparison is run so that it can skip them!

    //attention: some excluded directories are still in the comparison result! (see include filter handling!)
    if (!fpCfg.filter.nameFilter->isNull())
        stripExcludedDirectories(*output, *fpCfg.filter.nameFilter); //mark excluded directories (see fillBuffer()) + remove superfluous excluded subdirectories

    //apply soft filtering (hard filter already applied during traversal!)
    addSoftFiltering(*output, fpCfg.filter.timeSizeFilter);

    //##################################################################################
    return output;
}
}


void zen::compare(xmlAccess::OptionalDialogs& warnings,
                  bool allowUserInteraction,
                  bool runWithBackgroundPriority,
                  bool createDirLocks,
                  std::unique_ptr<LockHolder>& dirLocks,
                  const std::vector<FolderPairCfg>& cfgList,
                  FolderComparison& output,
                  ProcessCallback& callback)
{
    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        try
        {
            backgroundPrio = make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
        }
        catch (const FileError& e) //not an error in this context
        {
            callback.reportInfo(e.toString()); //may throw!
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //not an error in this context
    {
        callback.reportInfo(e.toString()); //may throw!
    }

    //PERF_START;

    callback.reportInfo(_("Starting comparison")); //indicator at the very beginning of the log to make sense of "total time"

    //init process: keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(-1, 0, ProcessCallback::PHASE_SCANNING); //may throw; it's not known how many files will be scanned => -1 objects

    //-------------------some basic checks:------------------------------------------

    const ResolutionInfo& resInfo = resolveFolderPairs(cfgList, allowUserInteraction, callback);

    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != cfgList.size())
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    checkForIncompleteInput(resInfo.resolvedPairs, warnings.warningInputFieldEmpty,  callback);
    checkFolderDependency  (resInfo.resolvedPairs, warnings.warningDependentFolders, callback);

    //list of directories that are *expected* to be existent (and need to be scanned)!

    //-------------------end of basic checks------------------------------------------

    auto dirAvailable = [&](const Zstring& dirpathFmt) { return resInfo.existingDirs.find(dirpathFmt) != resInfo.existingDirs.end(); };

    std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> totalWorkLoad;
    for (size_t i = 0; i < cfgList.size(); ++i)
        totalWorkLoad.emplace_back(resInfo.resolvedPairs[i], cfgList[i]);

    //lock (existing) directories before comparison
    if (createDirLocks)
        dirLocks = zen::make_unique<LockHolder>(resInfo.existingDirs, warnings.warningDirectoryLockFailed, callback);

    try
    {
        //------------------- fill directory buffer ---------------------------------------------------
        std::set<DirectoryKey> dirsToRead;

        for (const auto& w : totalWorkLoad)
        {
            if (dirAvailable(w.first.dirpathLeft)) //only traverse *currently existing* directories: at this point user is aware that non-ex + empty string are seen as empty folder!
                dirsToRead.emplace(w.first.dirpathLeft,  w.second.filter.nameFilter, w.second.handleSymlinks);
            if (dirAvailable(w.first.dirpathRight))
                dirsToRead.emplace(w.first.dirpathRight, w.second.filter.nameFilter, w.second.handleSymlinks);
        }

        FolderComparison outputTmp; //write to output as a transaction!

        //reduce peak memory by restricting lifetime of ComparisonBuffer to have ended when loading potentially huge InSyncDir instance in redetermineSyncDirection()
        {
            //------------ traverse/read folders -----------------------------------------------------
            //PERF_START;
            ComparisonBuffer cmpBuff(dirsToRead, callback);
            //PERF_STOP;

            //process binary comparison as one junk
            std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoadByContent;
            for (const auto& w : totalWorkLoad)
                switch (w.second.compareVar)
                {
                    case CMP_BY_TIME_SIZE:
                        break;
                    case CMP_BY_CONTENT:
                        workLoadByContent.push_back(w);
                        break;
                }
            std::list<std::shared_ptr<BaseDirPair>> outputByContent = cmpBuff.compareByContent(workLoadByContent);

            //write output in expected order
            for (const auto& w : totalWorkLoad)
                switch (w.second.compareVar)
                {
                    case CMP_BY_TIME_SIZE:
                        outputTmp.push_back(cmpBuff.compareByTimeSize(w.first, w.second));
                        break;
                    case CMP_BY_CONTENT:
                        assert(!outputByContent.empty());
                        if (!outputByContent.empty())
                        {
                            outputTmp.push_back(outputByContent.front());
                            outputByContent.pop_front();
                        }
                        break;
                }
        }

        assert(outputTmp.size() == cfgList.size());

        //--------- set initial sync-direction --------------------------------------------------

        for (auto j = begin(outputTmp); j != end(outputTmp); ++j)
        {
            const FolderPairCfg& fpCfg = cfgList[j - outputTmp.begin()];

            callback.reportStatus(_("Calculating sync directions..."));
            callback.forceUiRefresh();
            zen::redetermineSyncDirection(fpCfg.directionCfg, *j,
            [&](const std::wstring& warning) { callback.reportWarning(warning, warnings.warningDatabaseError); });
        }

        //output is written only if everything was processed correctly
        //note: output mustn't change during this process to be in sync with GUI grid view!!!
        outputTmp.swap(output);
    }
    catch (const std::bad_alloc& e)
    {
        callback.reportFatalError(_("Out of memory.") + L" " + utfCvrtTo<std::wstring>(e.what()));
        //we need to maintain the "output.size() == cfgList.size()" contract in ALL cases! => abort
        callback.abortProcessNow(); //throw X
    }
}
