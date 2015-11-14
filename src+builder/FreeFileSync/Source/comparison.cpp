// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "comparison.h"
#include <zen/process_priority.h>
#include <zen/perf.h>
#include "algorithm.h"
#include "lib/parallel_scan.h"
#include "lib/dir_exist_async.h"
#include "lib/binary.h"
#include "lib/cmp_filetime.h"
#include "lib/status_handler_impl.h"
#include "fs/concrete.h"

using namespace zen;


std::vector<FolderPairCfg> zen::extractCompareCfg(const MainConfiguration& mainCfg, int fileTimeTolerance)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs = { mainCfg.firstPair };
    append(allPairs, mainCfg.additionalPairs);

    std::vector<FolderPairCfg> output;
    std::transform(allPairs.begin(), allPairs.end(), std::back_inserter(output),
                   [&](const FolderPairEnh& enhPair) -> FolderPairCfg
    {
        return FolderPairCfg(enhPair.folderPathPhraseLeft_, enhPair.folderPathPhraseRight_,
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
    ResolvedFolderPair(const AbstractPath& left,
                       const AbstractPath& right) :
        folderPathLeft (left),
        folderPathRight(right) {}

    AbstractPath folderPathLeft;
    AbstractPath folderPathRight;
};


struct ResolvedBaseFolders
{
    std::vector<ResolvedFolderPair> resolvedPairs;
    std::set<AbstractPath, AFS::LessAbstractPath> existingBaseFolders;
};


ResolvedBaseFolders initializeBaseFolders(const std::vector<FolderPairCfg>& cfgList,
                                          bool allowUserInteraction,
                                          ProcessCallback& callback)
{
    ResolvedBaseFolders output;

    tryReportingError([&]
    {
        std::set<AbstractPath, AFS::LessAbstractPath> uniqueBaseFolders;

        //support "retry" for environment variable and and variable driver letter resolution!
        output.resolvedPairs.clear();
        for (const FolderPairCfg& fpCfg : cfgList)
        {
            AbstractPath folderPathLeft  = createAbstractPath(fpCfg.folderPathPhraseLeft_);
            AbstractPath folderPathRight = createAbstractPath(fpCfg.folderPathPhraseRight_);

            uniqueBaseFolders.insert(folderPathLeft);
            uniqueBaseFolders.insert(folderPathRight);

            output.resolvedPairs.emplace_back(folderPathLeft, folderPathRight);
        }

        const FolderStatus status = getFolderStatusNonBlocking(uniqueBaseFolders, allowUserInteraction, callback); //re-check *all* directories on each try!
        output.existingBaseFolders = status.existing;

        if (!status.missing.empty() || !status.failedChecks.empty())
        {
            std::wstring errorMsg = _("Cannot find the following folders:") + L"\n";

            for (const AbstractPath& folderPath : status.missing)
                errorMsg += L"\n" + AFS::getDisplayPath(folderPath);

            for (const auto& fc : status.failedChecks)
                errorMsg += L"\n" + AFS::getDisplayPath(fc.first);

            errorMsg += L"\n\n";
            errorMsg +=  _("If you ignore this error the folders are considered empty. Missing folders are created automatically when needed.");

            if (!status.failedChecks.empty())
            {
                errorMsg += L"\n___________________________________________";
                for (const auto& fc : status.failedChecks)
                    errorMsg += std::wstring(L"\n\n") + replaceCpy(fc.second.toString(), L"\n\n", L"\n");
            }

            throw FileError(errorMsg);
        }
    }, callback); //throw X?

    return output;
}


void checkForIncompleteInput(const std::vector<ResolvedFolderPair>& folderPairs, bool& warningInputFieldEmpty, ProcessCallback& callback)
{
    bool havePartialPair = false;
    bool haveFullPair    = false;

    for (const ResolvedFolderPair& fp : folderPairs)
        if (AFS::isNullPath(fp.folderPathLeft) != AFS::isNullPath(fp.folderPathRight))
            havePartialPair = true;
        else if (!AFS::isNullPath(fp.folderPathLeft))
            haveFullPair = true;

    if (havePartialPair == haveFullPair) //error if: all empty or exist both full and partial pairs -> support single-dir scenario
        callback.reportWarning(_("A folder input field is empty.") + L" \n\n" +
                               _("The corresponding folder will be considered as empty."), warningInputFieldEmpty);
}


//check whether one side is subdirectory of other side (folder pair wise!)
//similar check if one directory is read/written by multiple pairs not before beginning of synchronization
void checkFolderDependency(const std::vector<ResolvedFolderPair>& folderPairs, bool& warningDependentFolders, ProcessCallback& callback) //returns warning message, empty if all ok
{
    std::vector<ResolvedFolderPair> dependentFolderPairs;

    for (const ResolvedFolderPair& fp : folderPairs)
        if (!AFS::isNullPath(fp.folderPathLeft) && !AFS::isNullPath(fp.folderPathRight)) //empty folders names may be accepted by user
            //test wheter leftDirectory begins with rightDirectory or the other way round
            if (AFS::havePathDependency(fp.folderPathLeft, fp.folderPathRight))
                dependentFolderPairs.push_back(fp);

    if (!dependentFolderPairs.empty())
    {
        std::wstring warningMsg = _("The following folder paths are dependent from each other:");
        for (const ResolvedFolderPair& pair : dependentFolderPairs)
            warningMsg += L"\n\n" +
                          AFS::getDisplayPath(pair.folderPathLeft) + L"\n" +
                          AFS::getDisplayPath(pair.folderPathRight);

        callback.reportWarning(warningMsg, warningDependentFolders);
    }
}

//#############################################################################################################################

class ComparisonBuffer
{
public:
    ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, ProcessCallback& callback);

    //create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
    std::shared_ptr<BaseFolderPair> compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::list<std::shared_ptr<BaseFolderPair>> compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const;

private:
    ComparisonBuffer           (const ComparisonBuffer&) = delete;
    ComparisonBuffer& operator=(const ComparisonBuffer&) = delete;

    std::shared_ptr<BaseFolderPair> performComparison(const ResolvedFolderPair& fp,
                                                      const FolderPairCfg& fpCfg,
                                                      std::vector<FilePair*>& undefinedFiles,
                                                      std::vector<SymlinkPair*>& undefinedSymlinks) const;

    std::map<DirectoryKey, DirectoryValue> directoryBuffer; //contains only *existing* directories
    ProcessCallback& callback_;
};


ComparisonBuffer::ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, ProcessCallback& callback) : callback_(callback)
{
    class CbImpl : public FillBufferCallback
    {
    public:
        CbImpl(ProcessCallback& pcb) : callback_(pcb) {}

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
        int itemsReported = 0;
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
std::wstring getConflictInvalidDate(const std::wstring& displayPath, std::int64_t utcTime)
{
    return replaceCpy(_("File %x has an invalid date."), L"%x", fmtPath(displayPath)) + L"\n" +
           _("Date:") + L" " + utcToLocalTimeString(utcTime);
}


//check for changed files with same modification date
std::wstring getConflictSameDateDiffSize(const FilePair& file)
{
    return replaceCpy(_("Files %x have the same date but a different size."), L"%x", fmtPath(file.getPairRelativePath())) + L"\n" +
           L"    " + arrowLeft  + L" " + _("Date:") + L" " + utcToLocalTimeString(file.getLastWriteTime<LEFT_SIDE >()) + L"    " + _("Size:") + L" " + toGuiString(file.getFileSize<LEFT_SIDE>()) + L"\n" +
           L"    " + arrowRight + L" " + _("Date:") + L" " + utcToLocalTimeString(file.getLastWriteTime<RIGHT_SIDE>()) + L"    " + _("Size:") + L" " + toGuiString(file.getFileSize<RIGHT_SIDE>());
}


std::wstring getConflictSkippedBinaryComparison(const FilePair& file)
{
    return replaceCpy(_("Content comparison was skipped for excluded files %x."), L"%x", fmtPath(file.getPairRelativePath()));
}


std::wstring getDescrDiffMetaShortnameCase(const FileSystemObject& fsObj)
{
    return _("Items differ in attributes only") + L"\n" +
           L"    " + arrowLeft  + L" " + fmtPath(fsObj.getItemName<LEFT_SIDE >()) + L"\n" +
           L"    " + arrowRight + L" " + fmtPath(fsObj.getItemName<RIGHT_SIDE>());
}


template <class FileOrLinkPair>
std::wstring getDescrDiffMetaDate(const FileOrLinkPair& file)
{
    return _("Items differ in attributes only") + L"\n" +
           L"    " + arrowLeft  + L" " + _("Date:") + L" " + utcToLocalTimeString(file.template getLastWriteTime<LEFT_SIDE >()) + L"\n" +
           L"    " + arrowRight + L" " + _("Date:") + L" " + utcToLocalTimeString(file.template getLastWriteTime<RIGHT_SIDE>());
}

//-----------------------------------------------------------------------------

void categorizeSymlinkByTime(SymlinkPair& symlink, int fileTimeTolerance, unsigned int optTimeShiftHours)
{
    //categorize symlinks that exist on both sides
    switch (compareFileTime(symlink.getLastWriteTime<LEFT_SIDE>(),
                            symlink.getLastWriteTime<RIGHT_SIDE>(), fileTimeTolerance, optTimeShiftHours))
    {
        case TimeResult::EQUAL:
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp

            if (symlink.getItemName<LEFT_SIDE>() == symlink.getItemName<RIGHT_SIDE>())
                symlink.setCategory<FILE_EQUAL>();
            else
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            break;

        case TimeResult::LEFT_NEWER:
            symlink.setCategory<FILE_LEFT_NEWER>();
            break;

        case TimeResult::RIGHT_NEWER:
            symlink.setCategory<FILE_RIGHT_NEWER>();
            break;

        case TimeResult::LEFT_INVALID:
            symlink.setCategoryConflict(getConflictInvalidDate(AFS::getDisplayPath(symlink.getAbstractPath<LEFT_SIDE>()), symlink.getLastWriteTime<LEFT_SIDE>()));
            break;

        case TimeResult::RIGHT_INVALID:
            symlink.setCategoryConflict(getConflictInvalidDate(AFS::getDisplayPath(symlink.getAbstractPath<RIGHT_SIDE>()), symlink.getLastWriteTime<RIGHT_SIDE>()));
            break;
    }
}


std::shared_ptr<BaseFolderPair> ComparisonBuffer::compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    std::shared_ptr<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* symlink : uncategorizedLinks)
        categorizeSymlinkByTime(*symlink, fpConfig.fileTimeTolerance, fpConfig.optTimeShiftHours);

    //categorize files that exist on both sides
    for (FilePair* file : uncategorizedFiles)
    {
        switch (compareFileTime(file->getLastWriteTime<LEFT_SIDE>(),
                                file->getLastWriteTime<RIGHT_SIDE>(), fpConfig.fileTimeTolerance, fpConfig.optTimeShiftHours))
        {
            case TimeResult::EQUAL:
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp
                if (file->getFileSize<LEFT_SIDE>() == file->getFileSize<RIGHT_SIDE>())
                {
                    if (file->getItemName<LEFT_SIDE>() == file->getItemName<RIGHT_SIDE>())
                        file->setCategory<FILE_EQUAL>();
                    else
                        file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
                }
                else
                    file->setCategoryConflict(getConflictSameDateDiffSize(*file)); //same date, different filesize
                break;

            case TimeResult::LEFT_NEWER:
                file->setCategory<FILE_LEFT_NEWER>();
                break;

            case TimeResult::RIGHT_NEWER:
                file->setCategory<FILE_RIGHT_NEWER>();
                break;

            case TimeResult::LEFT_INVALID:
                file->setCategoryConflict(getConflictInvalidDate(AFS::getDisplayPath(file->getAbstractPath<LEFT_SIDE>()), file->getLastWriteTime<LEFT_SIDE>()));
                break;

            case TimeResult::RIGHT_INVALID:
                file->setCategoryConflict(getConflictInvalidDate(AFS::getDisplayPath(file->getAbstractPath<RIGHT_SIDE>()), file->getLastWriteTime<RIGHT_SIDE>()));
                break;
        }
    }
    return output;
}


void categorizeSymlinkByContent(SymlinkPair& symlink, int fileTimeTolerance, unsigned int optTimeShiftHours, ProcessCallback& callback)
{
    //categorize symlinks that exist on both sides
    Zstring targetPathRawL;
    Zstring targetPathRawR;
    Opt<std::wstring> errMsg = tryReportingError([&]
    {
        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<LEFT_SIDE>()))));

        targetPathRawL = AFS::getSymlinkContentBuffer(symlink.getAbstractPath<LEFT_SIDE>()); //throw FileError

        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<RIGHT_SIDE>()))));
        targetPathRawR = AFS::getSymlinkContentBuffer(symlink.getAbstractPath<RIGHT_SIDE>()); //throw FileError
    }, callback); //throw X?

    if (errMsg)
        symlink.setCategoryConflict(*errMsg);
    else
    {
        if (targetPathRawL == targetPathRawR
#ifdef ZEN_WIN //type of symbolic link is relevant for Windows only
            &&
            AFS::folderExists(symlink.getAbstractPath<LEFT_SIDE >()) == //check if dir-symlink
            AFS::folderExists(symlink.getAbstractPath<RIGHT_SIDE>())    //
#endif
           )
        {
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp

            //symlinks have same "content"
            if (symlink.getItemName<LEFT_SIDE>() != symlink.getItemName<RIGHT_SIDE>())
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            else if (!sameFileTime(symlink.getLastWriteTime<LEFT_SIDE>(),
                                   symlink.getLastWriteTime<RIGHT_SIDE>(), fileTimeTolerance, optTimeShiftHours))
                symlink.setCategoryDiffMetadata(getDescrDiffMetaDate(symlink));
            else
                symlink.setCategory<FILE_EQUAL>();
        }
        else
            symlink.setCategory<FILE_DIFFERENT_CONTENT>();
    }
}


std::list<std::shared_ptr<BaseFolderPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
{
    std::list<std::shared_ptr<BaseFolderPair>> output;
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
        for (FilePair* file : undefinedFiles)
            //pre-check: files have different content if they have a different filesize (must not be FILE_EQUAL: see InSyncFile)
            if (file->getFileSize<LEFT_SIDE>() != file->getFileSize<RIGHT_SIDE>())
                file->setCategory<FILE_DIFFERENT_CONTENT>();
            else
            {
                //perf: skip binary comparison for excluded rows (e.g. via time span and size filter)!
                //both soft and hard filter were already applied in ComparisonBuffer::performComparison()!
                if (!file->isActive())
                    file->setCategoryConflict(getConflictSkippedBinaryComparison(*file));
                else
                    filesToCompareBytewise.push_back(file);
            }

        //finish symlink categorization
        for (SymlinkPair* symlink : uncategorizedLinks)
            categorizeSymlinkByContent(*symlink, w.second.fileTimeTolerance, w.second.optTimeShiftHours, callback_);
    }

    //finish categorization...
    const size_t objectsTotal = filesToCompareBytewise.size();

    std::uint64_t bytesTotal = 0; //left and right filesizes are equal
    for (FilePair* file : filesToCompareBytewise)
        bytesTotal += file->getFileSize<LEFT_SIDE>();

    callback_.initNewPhase(static_cast<int>(objectsTotal), //may throw
                           bytesTotal,
                           ProcessCallback::PHASE_COMPARING_CONTENT);

    const std::wstring txtComparingContentOfFiles = _("Comparing content of files %x");

    //PERF_START;

    //compare files (that have same size) bytewise...
    for (FilePair* file : filesToCompareBytewise)
    {
        callback_.reportStatus(replaceCpy(txtComparingContentOfFiles, L"%x", fmtPath(file->getPairRelativePath())));

        //check files that exist in left and right model but have different content

        bool haveSameContent = false;
        Opt<std::wstring> errMsg = tryReportingError([&]
        {
            StatisticsReporter statReporter(1, file->getFileSize<LEFT_SIDE>(), callback_);

            auto onUpdateStatus = [&](std::int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            haveSameContent = filesHaveSameContent(file->getAbstractPath<LEFT_SIDE>(),
                                                   file->getAbstractPath<RIGHT_SIDE>(), onUpdateStatus); //throw FileError
            statReporter.reportDelta(1, 0);

            statReporter.reportFinished();
        }, callback_); //throw X?

        if (errMsg)
            file->setCategoryConflict(*errMsg);
        else
        {
            if (haveSameContent)
            {
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::syncTo() in file_hierarchy.cpp
                if (file->getItemName<LEFT_SIDE>() != file->getItemName<RIGHT_SIDE>())
                    file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
                else if (!sameFileTime(file->getLastWriteTime<LEFT_SIDE>(),
                                       file->getLastWriteTime<RIGHT_SIDE>(), file->base().getFileTimeTolerance(), file->base().getTimeShift()))
                    file->setCategoryDiffMetadata(getDescrDiffMetaDate(*file));
                else
                    file->setCategory<FILE_EQUAL>();
            }
            else
                file->setCategory<FILE_DIFFERENT_CONTENT>();
        }
    }
    return output;
}

//-----------------------------------------------------------------------------------------------

class MergeSides
{
public:
    MergeSides(const std::map<Zstring, std::wstring, LessFilePath>& failedItemReads,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedSymlinksOut) :
        failedItemReads_(failedItemReads),
        undefinedFiles(undefinedFilesOut),
        undefinedSymlinks(undefinedSymlinksOut) {}

    void execute(const FolderContainer& lhs, const FolderContainer& rhs, HierarchyObject& output)
    {
        auto it = failedItemReads_.find(Zstring()); //empty path if read-error for whole base directory

        mergeTwoSides(lhs, rhs,
                      it != failedItemReads_.end() ? &it->second : nullptr,
                      output);
    }

private:
    void mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const std::wstring* errorMsg, HierarchyObject& output);

    template <SelectedSide side>
    void fillOneSide(const FolderContainer& folderCont, const std::wstring* errorMsg, HierarchyObject& output);

    const std::wstring* checkFailedRead(FileSystemObject& fsObj, const std::wstring* errorMsg);

    const std::map<Zstring, std::wstring, LessFilePath>& failedItemReads_; //base-relative paths or empty if read-error for whole base directory
    std::vector<FilePair*>& undefinedFiles;
    std::vector<SymlinkPair*>& undefinedSymlinks;
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
void MergeSides::fillOneSide(const FolderContainer& folderCont, const std::wstring* errorMsg, HierarchyObject& output)
{
    for (const auto& file : folderCont.files)
    {
        FilePair& newItem = output.addSubFile<side>(file.first, file.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& symlink : folderCont.symlinks)
    {
        SymlinkPair& newItem = output.addSubLink<side>(symlink.first, symlink.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& dir : folderCont.folders)
    {
        FolderPair& newFolder = output.addSubFolder<side>(dir.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        fillOneSide<side>(dir.second, errorMsgNew, newFolder); //recurse
    }
}


//improve merge-perf by over 70% + more natural default sequence
template <class MapType, class ProcessLeftOnly, class ProcessRightOnly, class ProcessBoth> inline
void linearMerge(const MapType& mapLeft, const MapType& mapRight, ProcessLeftOnly lo, ProcessRightOnly ro, ProcessBoth bo)
{
    const auto lessKey = typename MapType::key_compare();

    auto itL = mapLeft .begin();
    auto itR = mapRight.begin();

    auto finishLeft  = [&] { std::for_each(itL, mapLeft .end(), lo); };
    auto finishRight = [&] { std::for_each(itR, mapRight.end(), ro); };

    if (itL == mapLeft .end()) return finishRight();
    if (itR == mapRight.end()) return finishLeft ();

    for (;;)
        if (lessKey(itL->first, itR->first))
        {
            lo(*itL);
            if (++itL == mapLeft.end())
                return finishRight();
        }
        else if (lessKey(itR->first, itL->first))
        {
            ro(*itR);
            if (++itR == mapRight.end())
                return finishLeft();
        }
        else
        {
            bo(*itL, *itR);
            ++itL; //
            ++itR; //increment BOTH before checking for end of range!
            if (itL == mapLeft .end()) return finishRight();
            if (itR == mapRight.end()) return finishLeft ();
        }
}


void MergeSides::mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const std::wstring* errorMsg, HierarchyObject& output)
{
    typedef const FolderContainer::FileList::value_type FileData;

    linearMerge(lhs.files, rhs.files,
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
        static_assert(IsSameType<HierarchyObject::FileList, FixedList<FilePair>>::value, ""); //HierarchyObject::addSubFile() must NOT invalidate references used in "undefinedFiles"!
    });

    //-----------------------------------------------------------------------------------------------
    typedef const FolderContainer::SymlinkList::value_type SymlinkData;

    linearMerge(lhs.symlinks, rhs.symlinks,
    [&](const SymlinkData& symlinkLeft ) { SymlinkPair& newItem = output.addSubLink<LEFT_SIDE >(symlinkLeft .first, symlinkLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const SymlinkData& symlinkRight) { SymlinkPair& newItem = output.addSubLink<RIGHT_SIDE>(symlinkRight.first, symlinkRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const SymlinkData& symlinkLeft, const SymlinkData& symlinkRight) //both sides
    {
        SymlinkPair& newItem = output.addSubLink(symlinkLeft.first,
                                                 symlinkLeft.second,
                                                 SYMLINK_EQUAL, //dummy-value until categorization is finished later
                                                 symlinkRight.first,
                                                 symlinkRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedSymlinks.push_back(&newItem);
    });

    //-----------------------------------------------------------------------------------------------
    typedef const FolderContainer::FolderList::value_type FolderData;

    linearMerge(lhs.folders, rhs.folders,
                [&](const FolderData& dirLeft) //left only
    {
        FolderPair& newFolder = output.addSubFolder<LEFT_SIDE>(dirLeft.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<LEFT_SIDE>(dirLeft.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirRight) //right only
    {
        FolderPair& newFolder = output.addSubFolder<RIGHT_SIDE>(dirRight.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<RIGHT_SIDE>(dirRight.second, errorMsgNew, newFolder); //recurse
    },

    [&](const FolderData& dirLeft, const FolderData& dirRight) //both sides
    {
        FolderPair& newFolder = output.addSubFolder(dirLeft.first, dirRight.first, DIR_EQUAL);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);

        if (!errorMsgNew)
            if (dirLeft.first != dirRight.first)
                newFolder.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(newFolder));

        mergeTwoSides(dirLeft.second, dirRight.second, errorMsgNew, newFolder); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//mark excluded directories (see fillBuffer()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(HierarchyObject& hierObj, const HardFilter& filterProc)
{
    for (FolderPair& folder : hierObj.refSubFolders())
        stripExcludedDirectories(folder, filterProc);

    //remove superfluous directories:
    //   this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
    //   and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to zen::FixedList!
    static_assert(IsSameType<FixedList<FolderPair>, HierarchyObject::FolderList>::value, "");

    hierObj.refSubFolders().remove_if([&](FolderPair& folder)
    {
        const bool included = filterProc.passDirFilter(folder.getPairRelativePath(), nullptr); //childItemMightMatch is false, child items were already excluded during scanning

        if (!included) //falsify only! (e.g. might already be inactive due to read error!)
            folder.setActive(false);

        return !included && //don't check active status, but eval filter directly!
               folder.refSubFolders().empty() &&
               folder.refSubLinks  ().empty() &&
               folder.refSubFiles  ().empty();
    });
}


//create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
std::shared_ptr<BaseFolderPair> ComparisonBuffer::performComparison(const ResolvedFolderPair& fp,
                                                                    const FolderPairCfg& fpCfg,
                                                                    std::vector<FilePair*>& undefinedFiles,
                                                                    std::vector<SymlinkPair*>& undefinedSymlinks) const
{
    callback_.reportStatus(_("Generating file list..."));
    callback_.forceUiRefresh();

    auto getDirValue = [&](const AbstractPath& folderPath) -> const DirectoryValue*
    {
        auto it = directoryBuffer.find(DirectoryKey(folderPath, fpCfg.filter.nameFilter, fpCfg.handleSymlinks));
        return it != directoryBuffer.end() ? &it->second : nullptr;
    };

    const DirectoryValue* bufValueLeft  = getDirValue(fp.folderPathLeft);
    const DirectoryValue* bufValueRight = getDirValue(fp.folderPathRight);

    std::map<Zstring, std::wstring, LessFilePath> failedReads; //base-relative paths or empty if read-error for whole base directory
    {
        //mix failedFolderReads with failedItemReads:
        //mark directory errors already at directory-level (instead for child items only) to show on GUI! See "MergeSides"
        //=> minor pessimization for "excludefilterFailedRead" which needlessly excludes parent folders, too
        if (bufValueLeft ) append(failedReads, bufValueLeft ->failedFolderReads);
        if (bufValueRight) append(failedReads, bufValueRight->failedFolderReads);

        if (bufValueLeft ) append(failedReads, bufValueLeft ->failedItemReads);
        if (bufValueRight) append(failedReads, bufValueRight->failedItemReads);
    }

    Zstring excludefilterFailedRead;
    if (failedReads.find(Zstring()) != failedReads.end()) //empty path if read-error for whole base directory
        excludefilterFailedRead += Zstr("*\n");
    else
        for (const auto& item : failedReads)
            excludefilterFailedRead += item.first + Zstr("\n"); //exclude item AND (potential) child items!

    std::shared_ptr<BaseFolderPair> output = std::make_shared<BaseFolderPair>(fp.folderPathLeft,
                                                                              bufValueLeft != nullptr, //dir existence must be checked only once: available iff buffer entry exists!
                                                                              fp.folderPathRight,
                                                                              bufValueRight != nullptr,
                                                                              fpCfg.filter.nameFilter->copyFilterAddingExclusion(excludefilterFailedRead),
                                                                              fpCfg.compareVar,
                                                                              fpCfg.fileTimeTolerance,
                                                                              fpCfg.optTimeShiftHours);

    //PERF_START;
    FolderContainer emptyFolderCont; //WTF!!! => using a temporary in the ternary conditional would implicitly call the FolderContainer copy-constructor!!!!!!
    MergeSides(failedReads, undefinedFiles, undefinedSymlinks).execute(bufValueLeft  ? bufValueLeft ->folderCont : emptyFolderCont,
                                                                       bufValueRight ? bufValueRight->folderCont : emptyFolderCont, *output);
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


FolderComparison zen::compare(xmlAccess::OptionalDialogs& warnings,
                              bool allowUserInteraction,
                              bool runWithBackgroundPriority,
                              bool createDirLocks,
                              std::unique_ptr<LockHolder>& dirLocks,
                              const std::vector<FolderPairCfg>& cfgList,
                              ProcessCallback& callback)
{
    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        try
        {
            backgroundPrio = std::make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
        }
        catch (const FileError& e) //not an error in this context
        {
            callback.reportInfo(e.toString()); //may throw!
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
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

    const ResolvedBaseFolders& resInfo = initializeBaseFolders(cfgList, allowUserInteraction, callback);

    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != cfgList.size())
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    checkForIncompleteInput(resInfo.resolvedPairs, warnings.warningInputFieldEmpty,  callback);
    checkFolderDependency  (resInfo.resolvedPairs, warnings.warningDependentFolders, callback);

    //list of directories that are *expected* to be existent (and need to be scanned)!

    //-------------------end of basic checks------------------------------------------

    auto basefolderExisting = [&](const AbstractPath& folderPath) { return resInfo.existingBaseFolders.find(folderPath) != resInfo.existingBaseFolders.end(); };

    std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> totalWorkLoad;
    for (size_t i = 0; i < cfgList.size(); ++i)
        totalWorkLoad.emplace_back(resInfo.resolvedPairs[i], cfgList[i]);

    //lock (existing) directories before comparison
    if (createDirLocks)
    {
        std::set<Zstring, LessFilePath> dirPathsExisting;
        for (const AbstractPath& folderPath : resInfo.existingBaseFolders)
            if (Opt<Zstring> nativePath = AFS::getNativeItemPath(folderPath)) //restrict directory locking to native paths until further
                dirPathsExisting.insert(*nativePath);

        dirLocks = std::make_unique<LockHolder>(dirPathsExisting, warnings.warningDirectoryLockFailed, callback);
    }

    try
    {
        //------------------- fill directory buffer ---------------------------------------------------
        std::set<DirectoryKey> dirsToRead;

        for (const auto& w : totalWorkLoad)
        {
            if (basefolderExisting(w.first.folderPathLeft)) //only traverse *currently existing* directories: at this point user is aware that non-ex + empty string are seen as empty folder!
                dirsToRead.emplace(w.first.folderPathLeft,  w.second.filter.nameFilter, w.second.handleSymlinks);
            if (basefolderExisting(w.first.folderPathRight))
                dirsToRead.emplace(w.first.folderPathRight, w.second.filter.nameFilter, w.second.handleSymlinks);
        }

        FolderComparison output;

        //reduce peak memory by restricting lifetime of ComparisonBuffer to have ended when loading potentially huge InSyncFolder instance in redetermineSyncDirection()
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
            std::list<std::shared_ptr<BaseFolderPair>> outputByContent = cmpBuff.compareByContent(workLoadByContent);

            //write output in expected order
            for (const auto& w : totalWorkLoad)
                switch (w.second.compareVar)
                {
                    case CMP_BY_TIME_SIZE:
                        output.push_back(cmpBuff.compareByTimeSize(w.first, w.second));
                        break;
                    case CMP_BY_CONTENT:
                        assert(!outputByContent.empty());
                        if (!outputByContent.empty())
                        {
                            output.push_back(outputByContent.front());
                            outputByContent.pop_front();
                        }
                        break;
                }
        }

        assert(output.size() == cfgList.size());

        //--------- set initial sync-direction --------------------------------------------------

        for (auto j = begin(output); j != end(output); ++j)
        {
            const FolderPairCfg& fpCfg = cfgList[j - output.begin()];

            callback.reportStatus(_("Calculating sync directions..."));
            callback.forceUiRefresh();

            zen::redetermineSyncDirection(fpCfg.directionCfg, *j,
            [&](const std::wstring& warning) { callback.reportWarning(warning, warnings.warningDatabaseError); },
            [&](std::int64_t bytesDelta) { callback.requestUiRefresh(); });//throw X
        }

        return output;
    }
    catch (const std::bad_alloc& e)
    {
        callback.reportFatalError(_("Out of memory.") + L" " + utfCvrtTo<std::wstring>(e.what()));
        //we need to maintain the "output.size() == cfgList.size()" contract in ALL cases! => abort
        callback.abortProcessNow(); //throw X
        throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
    }
}
