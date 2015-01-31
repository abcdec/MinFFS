// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include <set>
#include "tree_view.h"
#include <wx/settings.h>
#include <wx/menu.h>
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/stl_tools.h>
#include <zen/format_unit.h>
#include <wx+/rtl.h>
#include <wx+/context_menu.h>
#include <wx+/image_resources.h>
#include "../lib/icon_buffer.h"

using namespace zen;


inline
void TreeView::compressNode(Container& cont) //remove single-element sub-trees -> gain clarity + usability (call *after* inclusion check!!!)
{
    if (cont.subDirs.empty() || //single files node or...
        (cont.firstFileId == nullptr && //single dir node...
         cont.subDirs.size() == 1  && //
         cont.subDirs[0].firstFileId == nullptr && //...that is empty
         cont.subDirs[0].subDirs.empty()))       //
    {
        cont.subDirs.clear();
        cont.firstFileId = nullptr;
    }
}


template <class Function> //(const FileSystemObject&) -> bool
void TreeView::extractVisibleSubtree(HierarchyObject& hierObj,  //in
                                     TreeView::Container& cont, //out
                                     Function pred)
{
    auto getBytes = [](const FilePair& fileObj) //MSVC screws up miserably if we put this lambda into std::for_each
    {
        ////give accumulated bytes the semantics of a sync preview!
        //if (fileObj.isActive())
        //    switch (fileObj.getSyncDir())
        //    {
        //        case SyncDirection::LEFT:
        //            return fileObj.getFileSize<RIGHT_SIDE>();
        //        case SyncDirection::RIGHT:
        //            return fileObj.getFileSize<LEFT_SIDE>();
        //        case SyncDirection::NONE:
        //            break;
        //    }

        //prefer file-browser semantics over sync preview (=> always show useful numbers, even for SyncDirection::NONE)
        //discussion: https://sourceforge.net/p/freefilesync/discussion/open-discussion/thread/ba6b6a33
        return std::max(fileObj.getFileSize<LEFT_SIDE>(), fileObj.getFileSize<RIGHT_SIDE>());
    };

    cont.firstFileId = nullptr;
    for (FilePair& fileObj : hierObj.refSubFiles())
        if (pred(fileObj))
        {
            cont.bytesNet += getBytes(fileObj);
            ++cont.itemCountNet;

            if (!cont.firstFileId)
                cont.firstFileId = fileObj.getId();
        }

    for (SymlinkPair& linkObj : hierObj.refSubLinks())
        if (pred(linkObj))
        {
            ++cont.itemCountNet;

            if (!cont.firstFileId)
                cont.firstFileId = linkObj.getId();
        }

    cont.bytesGross     += cont.bytesNet;
    cont.itemCountGross += cont.itemCountNet;

    cont.subDirs.reserve(hierObj.refSubDirs().size()); //avoid expensive reallocations!

    for (DirPair& subDirObj : hierObj.refSubDirs())
    {
        const bool included = pred(subDirObj);

        cont.subDirs.emplace_back(); //
        auto& subDirCont = cont.subDirs.back();
        TreeView::extractVisibleSubtree(subDirObj, subDirCont, pred);
        if (included)
            ++subDirCont.itemCountGross;

        cont.bytesGross     += subDirCont.bytesGross;
        cont.itemCountGross += subDirCont.itemCountGross;

        if (!included && !subDirCont.firstFileId && subDirCont.subDirs.empty())
            cont.subDirs.pop_back();
        else
        {
            subDirCont.objId = subDirObj.getId();
            compressNode(subDirCont);
        }
    }
}


namespace
{
//generate nice percentage numbers which precisely sum up to 100
void calcPercentage(std::vector<std::pair<std::uint64_t, int*>>& workList)
{
    const std::uint64_t total = std::accumulate(workList.begin(), workList.end(), std::uint64_t(),
    [](std::uint64_t sum, const std::pair<std::uint64_t, int*>& pair) { return sum + pair.first; });

    if (total == 0U) //this case doesn't work with the error minimizing algorithm below
    {
        for (auto& pair : workList)
            *pair.second = 0;
        return;
    }

    int remainingPercent = 100;
    for (auto& pair : workList)
    {
        *pair.second = static_cast<int>(pair.first * 100U / total); //round down
        remainingPercent -= *pair.second;
    }
    assert(remainingPercent >= 0);
    assert(remainingPercent < static_cast<int>(workList.size()));

    //distribute remaining percent so that overall error is minimized as much as possible:
    remainingPercent = std::min(remainingPercent, static_cast<int>(workList.size()));
    if (remainingPercent > 0)
    {
        std::nth_element(workList.begin(), workList.begin() + remainingPercent - 1, workList.end(),
                         [total](const std::pair<std::uint64_t, int*>& lhs, const std::pair<std::uint64_t, int*>& rhs)
        {
            return lhs.first * 100U % total > rhs.first * 100U % total;
        });

        std::for_each(workList.begin(), workList.begin() + remainingPercent, [&](std::pair<std::uint64_t, int*>& pair) { ++*pair.second; });
    }
}


Zstring getShortDisplayNameForFolderPair(const Zstring& dirLeftPf, const Zstring& dirRightPf) //post-fixed with separator
{
    assert(endsWith(dirLeftPf,  FILE_NAME_SEPARATOR) || dirLeftPf .empty());
    assert(endsWith(dirRightPf, FILE_NAME_SEPARATOR) || dirRightPf.empty());

    auto itL = dirLeftPf .end();
    auto itR = dirRightPf.end();

    for (;;)
    {
        auto itLPrev = find_last(dirLeftPf .begin(), itL, FILE_NAME_SEPARATOR);
        auto itRPrev = find_last(dirRightPf.begin(), itR, FILE_NAME_SEPARATOR);

        if (itLPrev == itL ||
            itRPrev == itR)
        {
            if (itLPrev == itL)
                itLPrev = dirLeftPf.begin();
            else
                ++itLPrev; //skip separator
            if (itRPrev == itR)
                itRPrev = dirRightPf.begin();
            else
                ++itRPrev;

            if (equal(itLPrev, itL, itRPrev, itR))
            {
                itL = itLPrev;
                itR = itRPrev;
            }
            break;
        }

        if (!equal(itLPrev, itL, itRPrev, itR))
            break;
        itL = itLPrev;
        itR = itRPrev;
    }

    Zstring commonPostfix(itL, dirLeftPf.end());
    if (startsWith(commonPostfix, FILE_NAME_SEPARATOR))
        commonPostfix = afterFirst(commonPostfix, FILE_NAME_SEPARATOR);
    if (endsWith(commonPostfix, FILE_NAME_SEPARATOR))
        commonPostfix.resize(commonPostfix.size() - 1);

    if (commonPostfix.empty())
    {
        auto getLastComponent = [](const Zstring& dirPf) { return afterLast(beforeLast(dirPf, FILE_NAME_SEPARATOR), FILE_NAME_SEPARATOR); }; //returns the whole string if term not found
        if (dirLeftPf.empty())
            return getLastComponent(dirRightPf);
        else if (dirRightPf.empty())
            return getLastComponent(dirLeftPf);
        else
            return getLastComponent(dirLeftPf) + utfCvrtTo<Zstring>(L" \u2212 ") + //= unicode minus
                   getLastComponent(dirRightPf);
    }
    return commonPostfix;
}
}


template <bool ascending>
struct TreeView::LessShortName
{
    bool operator()(const TreeLine& lhs, const TreeLine& rhs) const
    {
        //files last (irrespective of sort direction)
        if (lhs.type_ == TreeView::TYPE_FILES)
            return false;
        else if (rhs.type_ == TreeView::TYPE_FILES)
            return true;

        if (lhs.type_ != rhs.type_)       //
            return lhs.type_ < rhs.type_; //shouldn't happen! root nodes not mixed with files or directories

        switch (lhs.type_)
        {
            case TreeView::TYPE_ROOT:
                return makeSortDirection(LessFilename(), Int2Type<ascending>())(static_cast<const RootNodeImpl*>(lhs.node_)->displayName,
                                                                                static_cast<const RootNodeImpl*>(rhs.node_)->displayName);

            case TreeView::TYPE_DIRECTORY:
            {
                const auto* dirObjL = dynamic_cast<const DirPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(lhs.node_)->objId));
                const auto* dirObjR = dynamic_cast<const DirPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(rhs.node_)->objId));

                if (!dirObjL)  //might be pathologic, but it's covered
                    return false;
                else if (!dirObjR)
                    return true;

                return makeSortDirection(LessFilename(), Int2Type<ascending>())(dirObjL->getPairShortName(), dirObjR->getPairShortName());
            }

            case TreeView::TYPE_FILES:
                break;
        }
        assert(false);
        return false; //:= all equal
    }
};


template <bool ascending>
void TreeView::sortSingleLevel(std::vector<TreeLine>& items, ColumnTypeNavi columnType)
{
    auto getBytes = [](const TreeLine& line) -> std::uint64_t
    {
        switch (line.type_)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                return line.node_->bytesGross;
            case TreeView::TYPE_FILES:
                return line.node_->bytesNet;
        }
        assert(false);
        return 0U;
    };

    auto getCount = [](const TreeLine& line) -> int
    {
        switch (line.type_)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                return line.node_->itemCountGross;

            case TreeView::TYPE_FILES:
                return line.node_->itemCountNet;
        }
        assert(false);
        return 0;
    };

    const auto lessBytes = [&](const TreeLine& lhs, const TreeLine& rhs) { return getBytes(lhs) < getBytes(rhs); };
    const auto lessCount = [&](const TreeLine& lhs, const TreeLine& rhs) { return getCount(lhs) < getCount(rhs); };

    switch (columnType)
    {
        case COL_TYPE_NAVI_BYTES:
            std::sort(items.begin(), items.end(), makeSortDirection(lessBytes, Int2Type<ascending>()));
            break;

        case COL_TYPE_NAVI_DIRECTORY:
            std::sort(items.begin(), items.end(), LessShortName<ascending>());
            break;

        case COL_TYPE_NAVI_ITEM_COUNT:
            std::sort(items.begin(), items.end(), makeSortDirection(lessCount, Int2Type<ascending>()));
            break;
    }
}


void TreeView::getChildren(const Container& cont, unsigned int level, std::vector<TreeLine>& output)
{
    output.clear();
    output.reserve(cont.subDirs.size() + 1); //keep pointers in "workList" valid
    std::vector<std::pair<std::uint64_t, int*>> workList;

    for (const DirNodeImpl& subDir : cont.subDirs)
    {
        output.emplace_back(level, 0, &subDir, TreeView::TYPE_DIRECTORY);
        workList.emplace_back(subDir.bytesGross, &output.back().percent_);
    }

    if (cont.firstFileId)
    {
        output.emplace_back(level, 0, &cont, TreeView::TYPE_FILES);
        workList.emplace_back(cont.bytesNet, &output.back().percent_);
    }
    calcPercentage(workList);

    if (sortAscending)
        sortSingleLevel<true>(output, sortColumn);
    else
        sortSingleLevel<false>(output, sortColumn);
}


void TreeView::applySubView(std::vector<RootNodeImpl>&& newView)
{
    //preserve current node expansion status
    auto getHierAlias = [](const TreeView::TreeLine& tl) -> const HierarchyObject*
    {
        switch (tl.type_)
        {
            case TreeView::TYPE_ROOT:
                return static_cast<const RootNodeImpl*>(tl.node_)->baseDirObj.get();

            case TreeView::TYPE_DIRECTORY:
                if (auto dirObj = dynamic_cast<const DirPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(tl.node_)->objId)))
                    return dirObj;
                break;

            case TreeView::TYPE_FILES:
                break; //none!!!
        }
        return nullptr;
    };

    std::unordered_set<const HierarchyObject*> expandedNodes;
    if (!flatTree.empty())
    {
        auto it = flatTree.begin();
        for (auto iterNext = flatTree.begin() + 1; iterNext != flatTree.end(); ++iterNext, ++it)
            if (it->level_ < iterNext->level_)
                if (auto hierObj = getHierAlias(*it))
                    expandedNodes.insert(hierObj);
    }

    //update view on full data
    folderCmpView.swap(newView); //newView may be an alias for folderCmpView! see sorting!

    //set default flat tree
    flatTree.clear();

    if (folderCmp.size() == 1) //single folder pair case (empty pairs were already removed!) do NOT use folderCmpView for this check!
    {
        if (!folderCmpView.empty()) //it may really be!
            getChildren(folderCmpView[0], 0, flatTree); //do not show root
    }
    else
    {
        //following is almost identical with TreeView::getChildren(): however we *cannot* reuse code here;
        //this were only possible if we replaced "std::vector<RootNodeImpl>" with "Container"!

        flatTree.reserve(folderCmpView.size()); //keep pointers in "workList" valid
        std::vector<std::pair<std::uint64_t, int*>> workList;

        for (const RootNodeImpl& root : folderCmpView)
        {
            flatTree.emplace_back(0, 0, &root, TreeView::TYPE_ROOT);
            workList.emplace_back(root.bytesGross, &flatTree.back().percent_);
        }

        calcPercentage(workList);

        if (sortAscending)
            sortSingleLevel<true>(flatTree, sortColumn);
        else
            sortSingleLevel<false>(flatTree, sortColumn);
    }

    //restore node expansion status
    for (size_t row = 0; row < flatTree.size(); ++row) //flatTree size changes during loop!
    {
        const TreeLine& line = flatTree[row];

        if (auto hierObj = getHierAlias(line))
            if (expandedNodes.find(hierObj) != expandedNodes.end())
            {
                std::vector<TreeLine> newLines;
                getChildren(*line.node_, line.level_ + 1, newLines);

                flatTree.insert(flatTree.begin() + row + 1, newLines.begin(), newLines.end());
            }
    }
}


template <class Predicate>
void TreeView::updateView(Predicate pred)
{
    //update view on full data
    std::vector<RootNodeImpl> newView;
    newView.reserve(folderCmp.size()); //avoid expensive reallocations!

    for (const std::shared_ptr<BaseDirPair>& baseObj : folderCmp)
    {
        newView.emplace_back();
        RootNodeImpl& root = newView.back();
        this->extractVisibleSubtree(*baseObj, root, pred); //"this->" is bogus for a static method, but GCC screws this one up

        //warning: the following lines are almost 1:1 copy from extractVisibleSubtree:
        //however we *cannot* reuse code here; this were only possible if we replaced "std::vector<RootNodeImpl>" with "Container"!
        if (!root.firstFileId && root.subDirs.empty())
            newView.pop_back();
        else
        {
            root.baseDirObj = baseObj;
            root.displayName = getShortDisplayNameForFolderPair(baseObj->getBaseDirPf<LEFT_SIDE >(),
                                                                baseObj->getBaseDirPf<RIGHT_SIDE>());

            this->compressNode(root); //"this->" required by two-pass lookup as enforced by GCC 4.7
        }
    }

    lastViewFilterPred = pred;
    applySubView(std::move(newView));
}


void TreeView::setSortDirection(ColumnTypeNavi colType, bool ascending) //apply permanently!
{
    sortColumn    = colType;
    sortAscending = ascending;

    //reapply current view
    applySubView(std::move(folderCmpView));
}


bool TreeView::getDefaultSortDirection(ColumnTypeNavi colType)
{
    switch (colType)
    {
        case COL_TYPE_NAVI_BYTES:
            return false;
        case COL_TYPE_NAVI_DIRECTORY:
            return true;
        case COL_TYPE_NAVI_ITEM_COUNT:
            return false;
    }
    assert(false);
    return true;
}


TreeView::NodeStatus TreeView::getStatus(size_t row) const
{
    if (row < flatTree.size())
    {
        if (row + 1 < flatTree.size() && flatTree[row + 1].level_ > flatTree[row].level_)
            return TreeView::STATUS_EXPANDED;

        //it's either reduced or empty
        switch (flatTree[row].type_)
        {
            case TreeView::TYPE_DIRECTORY:
            case TreeView::TYPE_ROOT:
                return flatTree[row].node_->firstFileId || !flatTree[row].node_->subDirs.empty() ? TreeView::STATUS_REDUCED : TreeView::STATUS_EMPTY;

            case TreeView::TYPE_FILES:
                return TreeView::STATUS_EMPTY;
        }
    }
    return TreeView::STATUS_EMPTY;
}


void TreeView::expandNode(size_t row)
{
    if (getStatus(row) != TreeView::STATUS_REDUCED)
    {
        assert(false);
        return;
    }

    if (row < flatTree.size())
    {
        std::vector<TreeLine> newLines;

        switch (flatTree[row].type_)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                getChildren(*flatTree[row].node_, flatTree[row].level_ + 1, newLines);
                break;
            case TreeView::TYPE_FILES:
                break;
        }
        flatTree.insert(flatTree.begin() + row + 1, newLines.begin(), newLines.end());
    }
}


void TreeView::reduceNode(size_t row)
{
    if (row < flatTree.size())
    {
        const unsigned int parentLevel = flatTree[row].level_;

        bool done = false;
        flatTree.erase(std::remove_if(flatTree.begin() + row + 1, flatTree.end(),
                                      [&](const TreeLine& line) -> bool
        {
            if (done)
                return false;
            if (line.level_ > parentLevel)
                return true;
            else
            {
                done = true;
                return false;
            }
        }), flatTree.end());
    }
}


ptrdiff_t TreeView::getParent(size_t row) const
{
    if (row < flatTree.size())
    {
        const auto level = flatTree[row].level_;

        while (row-- > 0)
            if (flatTree[row].level_ < level)
                return row;
    }
    return -1;
}


void TreeView::updateCmpResult(bool showExcluded,
                               bool leftOnlyFilesActive,
                               bool rightOnlyFilesActive,
                               bool leftNewerFilesActive,
                               bool rightNewerFilesActive,
                               bool differentFilesActive,
                               bool equalFilesActive,
                               bool conflictFilesActive)
{
    updateView([showExcluded, //make sure the predicate can be stored safely!
                leftOnlyFilesActive,
                rightOnlyFilesActive,
                leftNewerFilesActive,
                rightNewerFilesActive,
                differentFilesActive,
                equalFilesActive,
                conflictFilesActive](const FileSystemObject& fsObj) -> bool
    {
        if (!fsObj.isActive() && !showExcluded)
            return false;

        switch (fsObj.getCategory())
        {
            case FILE_LEFT_SIDE_ONLY:
                return leftOnlyFilesActive;
            case FILE_RIGHT_SIDE_ONLY:
                return rightOnlyFilesActive;
            case FILE_LEFT_NEWER:
                return leftNewerFilesActive;
            case FILE_RIGHT_NEWER:
                return rightNewerFilesActive;
            case FILE_DIFFERENT_CONTENT:
                return differentFilesActive;
            case FILE_EQUAL:
            case FILE_DIFFERENT_METADATA: //= sub-category of equal
                return equalFilesActive;
            case FILE_CONFLICT:
                return conflictFilesActive;
        }
        assert(false);
        return true;
    });
}


void TreeView::updateSyncPreview(bool showExcluded,
                                 bool syncCreateLeftActive,
                                 bool syncCreateRightActive,
                                 bool syncDeleteLeftActive,
                                 bool syncDeleteRightActive,
                                 bool syncDirOverwLeftActive,
                                 bool syncDirOverwRightActive,
                                 bool syncDirNoneActive,
                                 bool syncEqualActive,
                                 bool conflictFilesActive)
{
    updateView([showExcluded, //make sure the predicate can be stored safely!
                syncCreateLeftActive,
                syncCreateRightActive,
                syncDeleteLeftActive,
                syncDeleteRightActive,
                syncDirOverwLeftActive,
                syncDirOverwRightActive,
                syncDirNoneActive,
                syncEqualActive,
                conflictFilesActive](const FileSystemObject& fsObj) -> bool
    {
        if (!fsObj.isActive() && !showExcluded)
            return false;

        switch (fsObj.getSyncOperation())
        {
            case SO_CREATE_NEW_LEFT:
                return syncCreateLeftActive;
            case SO_CREATE_NEW_RIGHT:
                return syncCreateRightActive;
            case SO_DELETE_LEFT:
                return syncDeleteLeftActive;
            case SO_DELETE_RIGHT:
                return syncDeleteRightActive;
            case SO_OVERWRITE_RIGHT:
            case SO_COPY_METADATA_TO_RIGHT:
            case SO_MOVE_RIGHT_SOURCE:
            case SO_MOVE_RIGHT_TARGET:
                return syncDirOverwRightActive;
            case SO_OVERWRITE_LEFT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_MOVE_LEFT_SOURCE:
            case SO_MOVE_LEFT_TARGET:
                return syncDirOverwLeftActive;
            case SO_DO_NOTHING:
                return syncDirNoneActive;
            case SO_EQUAL:
                return syncEqualActive;
            case SO_UNRESOLVED_CONFLICT:
                return conflictFilesActive;
        }
        assert(false);
        return true;
    });
}


void TreeView::setData(FolderComparison& newData)
{
    std::vector<TreeLine    >().swap(flatTree);      //free mem
    std::vector<RootNodeImpl>().swap(folderCmpView); //
    folderCmp = newData;

    //remove truly empty folder pairs as early as this: we want to distinguish single/multiple folder pair cases by looking at "folderCmp"
    vector_remove_if(folderCmp, [](const std::shared_ptr<BaseDirPair>& baseObj)
    {
        return baseObj->getBaseDirPf<LEFT_SIDE >().empty() &&
               baseObj->getBaseDirPf<RIGHT_SIDE>().empty();
    });
}


std::unique_ptr<TreeView::Node> TreeView::getLine(size_t row) const
{
    if (row < flatTree.size())
    {
        const auto level  = flatTree[row].level_;
        const int percent = flatTree[row].percent_;

        switch (flatTree[row].type_)
        {
            case TreeView::TYPE_ROOT:
            {
                const auto& root = *static_cast<const TreeView::RootNodeImpl*>(flatTree[row].node_);
                return zen::make_unique<TreeView::RootNode>(percent, root.bytesGross, root.itemCountGross, getStatus(row), *root.baseDirObj, root.displayName);
            }
            break;

            case TreeView::TYPE_DIRECTORY:
            {
                const auto* dir = static_cast<const TreeView::DirNodeImpl*>(flatTree[row].node_);
                if (auto dirObj = dynamic_cast<DirPair*>(FileSystemObject::retrieve(dir->objId)))
                    return zen::make_unique<TreeView::DirNode>(percent, dir->bytesGross, dir->itemCountGross, level, getStatus(row), *dirObj);
            }
            break;

            case TreeView::TYPE_FILES:
            {
                const auto* parentDir = flatTree[row].node_;
                if (auto firstFile = FileSystemObject::retrieve(parentDir->firstFileId))
                {
                    std::vector<FileSystemObject*> filesAndLinks;
                    HierarchyObject& parent = firstFile->parent();

                    //lazy evaluation: recheck "lastViewFilterPred" again rather than buffer and bloat "lastViewFilterPred"
                    for (FileSystemObject& fsObj : parent.refSubFiles())
                        if (lastViewFilterPred(fsObj))
                            filesAndLinks.push_back(&fsObj);

                    for (FileSystemObject& fsObj : parent.refSubLinks())
                        if (lastViewFilterPred(fsObj))
                            filesAndLinks.push_back(&fsObj);

                    return zen::make_unique<TreeView::FilesNode>(percent, parentDir->bytesNet, parentDir->itemCountNet, level, filesAndLinks);
                }
            }
            break;
        }
    }
    return nullptr;
}

//##########################################################################################################

namespace
{
const wxColour COLOR_LEVEL0(0xcc, 0xcc, 0xff);
const wxColour COLOR_LEVEL1(0xcc, 0xff, 0xcc);
const wxColour COLOR_LEVEL2(0xff, 0xff, 0x99);

const wxColour COLOR_LEVEL3(0xcc, 0xcc, 0xcc);
const wxColour COLOR_LEVEL4(0xff, 0xcc, 0xff);
const wxColour COLOR_LEVEL5(0x99, 0xff, 0xcc);

const wxColour COLOR_LEVEL6(0xcc, 0xcc, 0x99);
const wxColour COLOR_LEVEL7(0xff, 0xcc, 0xcc);
const wxColour COLOR_LEVEL8(0xcc, 0xff, 0x99);

const wxColour COLOR_LEVEL9 (0xff, 0xff, 0xcc);
const wxColour COLOR_LEVEL10(0xcc, 0xff, 0xff);
const wxColour COLOR_LEVEL11(0xff, 0xcc, 0x99);

const wxColour COLOR_PERCENTAGE_BORDER    (198, 198, 198);
const wxColour COLOR_PERCENTAGE_BACKGROUND(0xf8, 0xf8, 0xf8);

//const wxColor COLOR_TREE_SELECTION_GRADIENT_FROM = wxColor( 89, 255,  99); //green: HSV: 88, 255, 172
//const wxColor COLOR_TREE_SELECTION_GRADIENT_TO   = wxColor(225, 255, 227); //       HSV: 88, 255, 240
const wxColor COLOR_TREE_SELECTION_GRADIENT_FROM = getColorSelectionGradientFrom();
const wxColor COLOR_TREE_SELECTION_GRADIENT_TO   = getColorSelectionGradientTo  ();

const int iconSizeSmall = IconBuffer::getSize(IconBuffer::SIZE_SMALL);

class GridDataNavi : private wxEvtHandler, public GridData
{
public:
    GridDataNavi(Grid& grid, const std::shared_ptr<TreeView>& treeDataView) : treeDataView_(treeDataView),
        fileIcon(IconBuffer::genericFileIcon(IconBuffer::SIZE_SMALL)),
        dirIcon (IconBuffer::genericDirIcon (IconBuffer::SIZE_SMALL)),
        rootBmp(getResourceImage(L"rootFolder").ConvertToImage().Scale(iconSizeSmall, iconSizeSmall, wxIMAGE_QUALITY_HIGH)),
        widthNodeIcon(iconSizeSmall),
        widthLevelStep(widthNodeIcon),
        widthNodeStatus(getResourceImage(L"nodeExpanded").GetWidth()),
        grid_(grid),
        showPercentBar(true)
    {
        grid.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(GridDataNavi::onKeyDown), nullptr, this);
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOWN,       GridClickEventHandler(GridDataNavi::onMouseLeft          ), nullptr, this);
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE,     GridClickEventHandler(GridDataNavi::onMouseLeftDouble    ), nullptr, this);
        grid.Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridClickEventHandler(GridDataNavi::onGridLabelContext), nullptr, this );
        grid.Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridClickEventHandler(GridDataNavi::onGridLabelLeftClick ), nullptr, this );
    }

    void setShowPercentage(bool value) { showPercentBar = value; grid_.Refresh(); }
    bool getShowPercentage() const { return showPercentBar; }

private:
    size_t getRowCount() const override { return treeDataView_ ? treeDataView_->linesTotal() : 0; }

    wxString getToolTip(size_t row, ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeNavi>(colType))
        {
            case COL_TYPE_NAVI_BYTES:
            case COL_TYPE_NAVI_ITEM_COUNT:
                break;

            case COL_TYPE_NAVI_DIRECTORY:
                if (treeDataView_)
                    if (std::unique_ptr<TreeView::Node> node = treeDataView_->getLine(row))
                        if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                        {
                            const wxString& dirLeft  = utfCvrtTo<wxString>(root->baseDirObj_.getBaseDirPf<LEFT_SIDE >());
                            const wxString& dirRight = utfCvrtTo<wxString>(root->baseDirObj_.getBaseDirPf<RIGHT_SIDE>());
                            if (dirLeft.empty())
                                return dirRight;
                            else if (dirRight.empty())
                                return dirLeft;
                            return dirLeft + L" \u2212 \n" + dirRight; //\u2212 = unicode minus
                        }
                break;
        }
        return wxString();
    }

    wxString getValue(size_t row, ColumnType colType) const override
    {
        if (treeDataView_)
        {
            if (std::unique_ptr<TreeView::Node> node = treeDataView_->getLine(row))
                switch (static_cast<ColumnTypeNavi>(colType))
                {
                    case COL_TYPE_NAVI_BYTES:
                        return filesizeToShortString(node->bytes_);

                    case COL_TYPE_NAVI_DIRECTORY:
                        if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                            return utfCvrtTo<wxString>(root->displayName_);
                        else if (const TreeView::DirNode* dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                            return utfCvrtTo<wxString>(dir->dirObj_.getPairShortName());
                        else if (dynamic_cast<const TreeView::FilesNode*>(node.get()))
                            return _("Files");
                        break;

                    case COL_TYPE_NAVI_ITEM_COUNT:
                        return toGuiString(node->itemCount_);
                }
        }
        return wxString();
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        wxRect rectInside = drawColumnLabelBorder(dc, rect);
        drawColumnLabelBackground(dc, rectInside, highlighted);

        rectInside.x     += COLUMN_GAP_LEFT;
        rectInside.width -= COLUMN_GAP_LEFT;
        drawColumnLabelText(dc, rectInside, getColumnLabel(colType));

        if (treeDataView_) //draw sort marker
        {
            auto sortInfo = treeDataView_->getSortDirection();
            if (colType == static_cast<ColumnType>(sortInfo.first))
            {
                const wxBitmap& marker = getResourceImage(sortInfo.second ? L"sortAscending" : L"sortDescending");
                wxPoint markerBegin = rectInside.GetTopLeft() + wxPoint((rectInside.width - marker.GetWidth()) / 2, 0);
                dc.DrawBitmap(marker, markerBegin, true); //respect 2-pixel gap
            }
        }
    }

    static const int GAP_SIZE = 2;

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, COLOR_TREE_SELECTION_GRADIENT_FROM, COLOR_TREE_SELECTION_GRADIENT_TO, wxEAST);
            //ignore focus
            else
                clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected) override
    {
        //wxRect rectTmp= drawCellBorder(dc, rect);
        wxRect rectTmp = rect;

        //  Partitioning:
        //   ________________________________________________________________________________
        //  | space | gap | percentage bar | 2 x gap | node status | gap |icon | gap | rest |
        //   --------------------------------------------------------------------------------
        // -> synchronize renderCell() <-> getBestSize() <-> onMouseLeft()

        if (static_cast<ColumnTypeNavi>(colType) == COL_TYPE_NAVI_DIRECTORY && treeDataView_)
        {
            if (std::unique_ptr<TreeView::Node> node = treeDataView_->getLine(row))
            {
                ////clear first secion:
                //clearArea(dc, wxRect(rect.GetTopLeft(), wxSize(
                //                         node->level_ * widthLevelStep + GAP_SIZE + //width
                //                         (showPercentBar ? widthPercentBar + 2 * GAP_SIZE : 0) + //
                //                         widthNodeStatus + GAP_SIZE + widthNodeIcon + GAP_SIZE, //
                //                         rect.height)), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

                //consume space
                rectTmp.x     += static_cast<int>(node->level_) * widthLevelStep;
                rectTmp.width -= static_cast<int>(node->level_) * widthLevelStep;

                rectTmp.x     += GAP_SIZE;
                rectTmp.width -= GAP_SIZE;

                if (rectTmp.width > 0)
                {
                    //percentage bar
                    if (showPercentBar)
                    {
                        const wxColour brushCol = [&]() -> wxColour
                        {
                            switch (node->level_ % 12)
                            {
                                case 0:
                                    return COLOR_LEVEL0;
                                case 1:
                                    return COLOR_LEVEL1;
                                case 2:
                                    return COLOR_LEVEL2;
                                case 3:
                                    return COLOR_LEVEL3;
                                case 4:
                                    return COLOR_LEVEL4;
                                case 5:
                                    return COLOR_LEVEL5;
                                case 6:
                                    return COLOR_LEVEL6;
                                case 7:
                                    return COLOR_LEVEL7;
                                case 8:
                                    return COLOR_LEVEL8;
                                case 9:
                                    return COLOR_LEVEL9;
                                case 10:
                                    return COLOR_LEVEL10;
                                default:
                                    return COLOR_LEVEL11;
                            }
                        }();

                        const wxRect areaPerc(rectTmp.x, rectTmp.y + 2, widthPercentBar, rectTmp.height - 4);
                        {
                            //clear background
                            wxDCPenChanger   dummy (dc, COLOR_PERCENTAGE_BORDER);
                            wxDCBrushChanger dummy2(dc, COLOR_PERCENTAGE_BACKGROUND);
                            dc.DrawRectangle(areaPerc);

                            //inner area
                            dc.SetPen  (brushCol);
                            dc.SetBrush(brushCol);

                            wxRect areaPercTmp = areaPerc;
                            areaPercTmp.Deflate(1); //do not include border
                            areaPercTmp.width = numeric::round(areaPercTmp.width * node->percent_ / 100.0);
                            dc.DrawRectangle(areaPercTmp);
                        }

                        wxDCTextColourChanger dummy3(dc, *wxBLACK); //accessibility: always set both foreground AND background colors!
                        dc.DrawLabel(numberTo<wxString>(node->percent_) + L"%", areaPerc, wxALIGN_CENTER);

                        rectTmp.x     += widthPercentBar + 2 * GAP_SIZE;
                        rectTmp.width -= widthPercentBar + 2 * GAP_SIZE;
                    }
                    if (rectTmp.width > 0)
                    {
                        //node status
                        auto drawStatus = [&](const wchar_t* image)
                        {
                            const wxBitmap& bmp = getResourceImage(image);

                            wxRect rectStat(rectTmp.GetTopLeft(), wxSize(bmp.GetWidth(), bmp.GetHeight()));
                            rectStat.y += (rectTmp.height - rectStat.height) / 2;

                            //clearArea(dc, rectStat, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
                            clearArea(dc, rectStat, *wxWHITE); //accessibility: always set both foreground AND background colors!
                            drawBitmapRtlMirror(dc, bmp, rectStat, wxALIGN_CENTER, buffer);
                        };

                        switch (node->status_)
                        {
                            case TreeView::STATUS_EXPANDED:
                                drawStatus(L"nodeExpanded");
                                break;
                            case TreeView::STATUS_REDUCED:
                                drawStatus(L"nodeReduced");
                                break;
                            case TreeView::STATUS_EMPTY:
                                break;
                        }

                        rectTmp.x     += widthNodeStatus + GAP_SIZE;
                        rectTmp.width -= widthNodeStatus + GAP_SIZE;
                        if (rectTmp.width > 0)
                        {
                            wxBitmap nodeIcon;
                            bool isActive = true;
                            //icon
                            if (dynamic_cast<const TreeView::RootNode*>(node.get()))
                                nodeIcon = rootBmp;
                            else if (auto dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                            {
                                nodeIcon = dirIcon;
                                isActive = dir->dirObj_.isActive();
                            }
                            else if (dynamic_cast<const TreeView::FilesNode*>(node.get()))
                                nodeIcon = fileIcon;

                            if (isActive)
                                drawBitmapRtlNoMirror(dc, nodeIcon, rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, buffer);

                            else
                                drawBitmapRtlNoMirror(dc, wxBitmap(nodeIcon.ConvertToImage().ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3)), //treat all channels equally!
                                                      rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, buffer);

                            rectTmp.x     += widthNodeIcon + GAP_SIZE;
                            rectTmp.width -= widthNodeIcon + GAP_SIZE;

                            if (rectTmp.width > 0)
                                drawCellText(dc, rectTmp, getValue(row, colType), isActive, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                        }
                    }
                }
            }
        }
        else
        {
            int alignment = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL;

            //have file size and item count right-justified (but don't change for RTL languages)
            if ((static_cast<ColumnTypeNavi>(colType) == COL_TYPE_NAVI_BYTES ||
                 static_cast<ColumnTypeNavi>(colType) == COL_TYPE_NAVI_ITEM_COUNT) && grid_.GetLayoutDirection() != wxLayout_RightToLeft)
            {
                rectTmp.width -= 2 * GAP_SIZE;
                alignment = wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL;
            }
            else //left-justified
            {
                rectTmp.x     += 2 * GAP_SIZE;
                rectTmp.width -= 2 * GAP_SIZE;
            }

            drawCellText(dc, rectTmp, getValue(row, colType), true, alignment);
        }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize() <-> onMouseLeft()

        if (static_cast<ColumnTypeNavi>(colType) == COL_TYPE_NAVI_DIRECTORY && treeDataView_)
        {
            if (std::unique_ptr<TreeView::Node> node = treeDataView_->getLine(row))
                return node->level_ * widthLevelStep + GAP_SIZE + (showPercentBar ? widthPercentBar + 2 * GAP_SIZE : 0) + widthNodeStatus + GAP_SIZE
                       + widthNodeIcon + GAP_SIZE + dc.GetTextExtent(getValue(row, colType)).GetWidth() +
                       GAP_SIZE; //additional gap from right
            else
                return 0;
        }
        else
            return 2 * GAP_SIZE + dc.GetTextExtent(getValue(row, colType)).GetWidth() +
                   2 * GAP_SIZE; //include gap from right!
    }

    wxString getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeNavi>(colType))
        {
            case COL_TYPE_NAVI_BYTES:
                return _("Size");
            case COL_TYPE_NAVI_DIRECTORY:
                return _("Name");
            case COL_TYPE_NAVI_ITEM_COUNT:
                return _("Items");
        }
        return wxEmptyString;
    }

    void onMouseLeft(GridClickEvent& event)
    {
        if (treeDataView_)
        {
            bool clickOnNodeStatus = false;
            if (static_cast<ColumnTypeNavi>(event.colType_) == COL_TYPE_NAVI_DIRECTORY)
                if (std::unique_ptr<TreeView::Node> node = treeDataView_->getLine(event.row_))
                {
                    const int absX = grid_.CalcUnscrolledPosition(event.GetPosition()).x;
                    const wxRect cellArea = grid_.getCellArea(event.row_, event.colType_);
                    if (cellArea.width > 0 && cellArea.height > 0)
                    {
                        const int tolerance = 1;
                        const int xNodeStatusFirst = -tolerance + cellArea.x + static_cast<int>(node->level_) * widthLevelStep + GAP_SIZE + (showPercentBar ? widthPercentBar + 2 * GAP_SIZE : 0);
                        const int xNodeStatusLast  = (xNodeStatusFirst + tolerance) + widthNodeStatus + tolerance;
                        // -> synchronize renderCell() <-> getBestSize() <-> onMouseLeft()

                        if (xNodeStatusFirst <= absX && absX < xNodeStatusLast)
                            clickOnNodeStatus = true;
                    }
                }
            //--------------------------------------------------------------------------------------------------

            if (clickOnNodeStatus)
                switch (treeDataView_->getStatus(event.row_))
                {
                    case TreeView::STATUS_EXPANDED:
                        return reduceNode(event.row_);
                    case TreeView::STATUS_REDUCED:
                        return expandNode(event.row_);
                    case TreeView::STATUS_EMPTY:
                        break;
                }
        }
        event.Skip();
    }

    void onMouseLeftDouble(GridClickEvent& event)
    {
        if (treeDataView_)
            switch (treeDataView_->getStatus(event.row_))
            {
                case TreeView::STATUS_EXPANDED:
                    return reduceNode(event.row_);
                case TreeView::STATUS_REDUCED:
                    return expandNode(event.row_);
                case TreeView::STATUS_EMPTY:
                    break;
            }
        event.Skip();
    }

    void onKeyDown(wxKeyEvent& event)
    {
        int keyCode = event.GetKeyCode();
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
        {
            if (keyCode == WXK_LEFT)
                keyCode = WXK_RIGHT;
            else if (keyCode == WXK_RIGHT)
                keyCode = WXK_LEFT;
            else if (keyCode == WXK_NUMPAD_LEFT)
                keyCode = WXK_NUMPAD_RIGHT;
            else if (keyCode == WXK_NUMPAD_RIGHT)
                keyCode = WXK_NUMPAD_LEFT;
        }

        const size_t rowCount = grid_.getRowCount();
        if (rowCount == 0) return;

        size_t row = grid_.getGridCursor();
        if (event.ShiftDown())
            ;
        else if (event.ControlDown())
            ;
        else
            switch (keyCode)
            {
                case WXK_LEFT:
                case WXK_NUMPAD_LEFT:
                case WXK_NUMPAD_SUBTRACT: //http://msdn.microsoft.com/en-us/library/ms971323.aspx#atg_keyboardshortcuts_windows_shortcut_keys
                    if (treeDataView_)
                        switch (treeDataView_->getStatus(row))
                        {
                            case TreeView::STATUS_EXPANDED:
                                return reduceNode(row);
                            case TreeView::STATUS_REDUCED:
                            case TreeView::STATUS_EMPTY:

                                const int parentRow = treeDataView_->getParent(row);
                                if (parentRow >= 0)
                                    grid_.setGridCursor(parentRow);
                                break;
                        }
                    return; //swallow event

                case WXK_RIGHT:
                case WXK_NUMPAD_RIGHT:
                case WXK_NUMPAD_ADD:
                    if (treeDataView_)
                        switch (treeDataView_->getStatus(row))
                        {
                            case TreeView::STATUS_EXPANDED:
                                grid_.setGridCursor(std::min(rowCount - 1, row + 1));
                                break;
                            case TreeView::STATUS_REDUCED:
                                return expandNode(row);
                            case TreeView::STATUS_EMPTY:
                                break;
                        }
                    return; //swallow event
            }

        event.Skip();
    }

    void onGridLabelContext(GridClickEvent& event)
    {
        ContextMenu menu;

        //--------------------------------------------------------------------------------------------------------
        menu.addCheckBox(_("Percentage"), [this] { setShowPercentage(!getShowPercentage()); }, getShowPercentage());
        //--------------------------------------------------------------------------------------------------------
        auto toggleColumn = [&](const Grid::ColumnAttribute& ca)
        {
            auto colAttr = grid_.getColumnConfig();

            for (auto it = colAttr.begin(); it != colAttr.end(); ++it)
                if (it->type_ == ca.type_)
                {
                    it->visible_ = !ca.visible_;
                    grid_.setColumnConfig(colAttr);
                    return;
                }
        };

        for (const Grid::ColumnAttribute& ca : grid_.getColumnConfig())
        {
            menu.addCheckBox(getColumnLabel(ca.type_), [ca, toggleColumn]() { toggleColumn(ca); },
            ca.visible_, ca.type_ != static_cast<ColumnType>(COL_TYPE_NAVI_DIRECTORY)); //do not allow user to hide file name column!
        }
        //--------------------------------------------------------------------------------------------------------
        menu.addSeparator();

        auto setDefaultColumns = [&]
        {
            setShowPercentage(defaultValueShowPercentage);
            grid_.setColumnConfig(treeview::convertConfig(getDefaultColumnAttributesNavi()));
        };
        menu.addItem(_("&Default"), setDefaultColumns); //'&' -> reuse text from "default" buttons elsewhere

        menu.popup(grid_);

        //event.Skip();
    }

    void onGridLabelLeftClick(GridClickEvent& event)
    {
        if (treeDataView_)
        {
            const auto colTypeNavi = static_cast<ColumnTypeNavi>(event.colType_);
            bool sortAscending = TreeView::getDefaultSortDirection(colTypeNavi);

            const auto sortInfo = treeDataView_->getSortDirection();
            if (sortInfo.first == colTypeNavi)
                sortAscending = !sortInfo.second;

            treeDataView_->setSortDirection(colTypeNavi, sortAscending);
            grid_.clearSelection(ALLOW_GRID_EVENT);
            grid_.Refresh();
        }
    }

    void expandNode(size_t row)
    {
        treeDataView_->expandNode(row);
        grid_.Refresh(); //implicitly clears selection (changed row count after expand)
        grid_.setGridCursor(row);
        //grid_.autoSizeColumns(); -> doesn't look as good as expected
    }

    void reduceNode(size_t row)
    {
        treeDataView_->reduceNode(row);
        grid_.Refresh();
        grid_.setGridCursor(row);
    }

    std::shared_ptr<TreeView> treeDataView_;
    const wxBitmap fileIcon;
    const wxBitmap dirIcon;
    const wxBitmap rootBmp;
    std::unique_ptr<wxBitmap> buffer; //avoid costs of recreating this temporal variable
    const int widthNodeIcon;
    const int widthLevelStep;
    const int widthNodeStatus;
    static const int widthPercentBar = 60;
    Grid& grid_;
    bool showPercentBar;
};
}


void treeview::init(Grid& grid, const std::shared_ptr<TreeView>& treeDataView)
{
    grid.setDataProvider(std::make_shared<GridDataNavi>(grid, treeDataView));
    grid.showRowLabel(false);

    const int rowHeight = std::max(IconBuffer::getSize(IconBuffer::SIZE_SMALL), grid.getMainWin().GetCharHeight()) + 2; //allow 1 pixel space on top and bottom; dearly needed on OS X!
    grid.setRowHeight(rowHeight);
}


void treeview::setShowPercentage(Grid& grid, bool value)
{
    if (auto* prov = dynamic_cast<GridDataNavi*>(grid.getDataProvider()))
        prov->setShowPercentage(value);
    else
        assert(false);
}


bool treeview::getShowPercentage(const Grid& grid)
{
    if (auto* prov = dynamic_cast<const GridDataNavi*>(grid.getDataProvider()))
        return prov->getShowPercentage();
    assert(false);
    return true;
}


namespace
{
std::vector<ColumnAttributeNavi> makeConsistent(const std::vector<ColumnAttributeNavi>& attribs)
{
    std::set<ColumnTypeNavi> usedTypes;

    std::vector<ColumnAttributeNavi> output;
    //remove duplicates
    std::copy_if(attribs.begin(), attribs.end(), std::back_inserter(output),
    [&](const ColumnAttributeNavi& a) { return usedTypes.insert(a.type_).second; });

    //make sure each type is existing!
    const auto& defAttr = getDefaultColumnAttributesNavi();
    std::copy_if(defAttr.begin(), defAttr.end(), std::back_inserter(output),
    [&](const ColumnAttributeNavi& a) { return usedTypes.insert(a.type_).second; });

    return output;
}
}

std::vector<Grid::ColumnAttribute> treeview::convertConfig(const std::vector<ColumnAttributeNavi>& attribs)
{
    const auto& attribClean = makeConsistent(attribs);

    std::vector<Grid::ColumnAttribute> output;
    std::transform(attribClean.begin(), attribClean.end(), std::back_inserter(output),
    [&](const ColumnAttributeNavi& ca) { return Grid::ColumnAttribute(static_cast<ColumnType>(ca.type_), ca.offset_, ca.stretch_, ca.visible_); });

    return output;
}


std::vector<ColumnAttributeNavi> treeview::convertConfig(const std::vector<Grid::ColumnAttribute>& attribs)
{
    std::vector<ColumnAttributeNavi> output;

    std::transform(attribs.begin(), attribs.end(), std::back_inserter(output),
    [&](const Grid::ColumnAttribute& ca) { return ColumnAttributeNavi(static_cast<ColumnTypeNavi>(ca.type_), ca.offset_, ca.stretch_, ca.visible_); });

    return makeConsistent(output);
}
