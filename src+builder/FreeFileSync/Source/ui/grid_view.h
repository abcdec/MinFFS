// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef GRIDVIEW_H_INCLUDED
#define GRIDVIEW_H_INCLUDED

#include <set>
#include <unordered_map>
#include "column_attr.h"
#include "../file_hierarchy.h"


namespace zen
{
//grid view of FolderComparison
class GridView
{
public:
    GridView() : folderPairCount(0) {}

    //direct data access via row number
    const FileSystemObject* getObject(size_t row) const;  //returns nullptr if object is not found; complexity: constant!
    /**/
    FileSystemObject* getObject(size_t row);        //
    size_t rowsOnView() const { return viewRef  .size(); } //only visible elements
    size_t rowsTotal () const { return sortedRef.size(); } //total rows available

    //get references to FileSystemObject: no nullptr-check needed! Everything's bound.
    std::vector<FileSystemObject*> getAllFileRef(const std::set<size_t>& rows);

    struct StatusCmpResult
    {
        StatusCmpResult();

        bool existsExcluded;
        bool existsEqual;
        bool existsConflict;

        bool existsLeftOnly;
        bool existsRightOnly;
        bool existsLeftNewer;
        bool existsRightNewer;
        bool existsDifferent;

        unsigned int filesOnLeftView;
        unsigned int foldersOnLeftView;
        unsigned int filesOnRightView;
        unsigned int foldersOnRightView;

        std::uint64_t filesizeLeftView;
        std::uint64_t filesizeRightView;
    };

    //comparison results view
    StatusCmpResult updateCmpResult(bool showExcluded,
                                    bool leftOnlyFilesActive,
                                    bool rightOnlyFilesActive,
                                    bool leftNewerFilesActive,
                                    bool rightNewerFilesActive,
                                    bool differentFilesActive,
                                    bool equalFilesActive,
                                    bool conflictFilesActive);

    struct StatusSyncPreview
    {
        StatusSyncPreview();

        bool existsExcluded;
        bool existsEqual;
        bool existsConflict;

        bool existsSyncCreateLeft;
        bool existsSyncCreateRight;
        bool existsSyncDeleteLeft;
        bool existsSyncDeleteRight;
        bool existsSyncDirLeft;
        bool existsSyncDirRight;
        bool existsSyncDirNone;

        unsigned int filesOnLeftView;
        unsigned int foldersOnLeftView;
        unsigned int filesOnRightView;
        unsigned int foldersOnRightView;

        std::uint64_t filesizeLeftView;
        std::uint64_t filesizeRightView;
    };

    //synchronization preview
    StatusSyncPreview updateSyncPreview(bool showExcluded,
                                        bool syncCreateLeftActive,
                                        bool syncCreateRightActive,
                                        bool syncDeleteLeftActive,
                                        bool syncDeleteRightActive,
                                        bool syncDirOverwLeftActive,
                                        bool syncDirOverwRightActive,
                                        bool syncDirNoneActive,
                                        bool syncEqualActive,
                                        bool conflictFilesActive);

    void setData(FolderComparison& newData);
    void removeInvalidRows(); //remove references to rows that have been deleted meanwhile: call after manual deletion and synchronization!

    //sorting...
    bool static getDefaultSortDirection(zen::ColumnTypeRim type); //true: ascending; false: descending

    void sortView(zen::ColumnTypeRim type, bool onLeft, bool ascending); //always call this method for sorting, never sort externally!

    struct SortInfo
    {
        SortInfo(zen::ColumnTypeRim type, bool onLeft, bool ascending) : type_(type), onLeft_(onLeft), ascending_(ascending) {}
        zen::ColumnTypeRim type_;
        bool onLeft_;
        bool ascending_;
    };
    const SortInfo* getSortInfo() const { return currentSort.get(); } //return nullptr if currently not sorted

    ptrdiff_t findRowDirect(FileSystemObject::ObjectIdConst objId) const; // find an object's row position on view list directly, return < 0 if not found
    ptrdiff_t findRowFirstChild(const HierarchyObject* hierObj)    const; // find first child of DirPair or BaseDirPair *on sorted sub view*
    //"hierObj" may be invalid, it is NOT dereferenced, return < 0 if not found

    size_t getFolderPairCount() const { return folderPairCount; } //count non-empty pairs to distinguish single/multiple folder pair cases

private:
    struct RefIndex
    {
        RefIndex(size_t folderInd, FileSystemObject::ObjectId id) :
            folderIndex(folderInd),
            objId(id) {}
        size_t folderIndex; //because of alignment there's no benefit in using "unsigned int" in 64-bit code here!
        FileSystemObject::ObjectId objId;
    };

    template <class Predicate> void updateView(Predicate pred);


    std::unordered_map<FileSystemObject::ObjectIdConst, size_t> rowPositions; //find row positions on sortedRef directly
    std::unordered_map<const void*, size_t> rowPositionsFirstChild; //find first child on sortedRef of a hierarchy object
    //void* instead of HierarchyObject*: these are weak pointers and should *never be dereferenced*!

    std::vector<FileSystemObject::ObjectId> viewRef;  //partial view on sortedRef
    /*             /|\
                    | (update...)
                    |                         */
    std::vector<RefIndex> sortedRef; //flat view of weak pointers on folderCmp; may be sorted
    /*             /|\
                    | (setData...)
                    |                         */
    //std::shared_ptr<FolderComparison> folderCmp; //actual comparison data: owned by GridView!
    size_t folderPairCount; //number of non-empty folder pairs


    class SerializeHierarchy;

    //sorting classes
    template <bool ascending, SelectedSide side>
    struct LessFullPath;

    template <bool ascending>
    struct LessRelativeFolder;

    template <bool ascending, SelectedSide side>
    struct LessShortFileName;

    template <bool ascending, SelectedSide side>
    struct LessFilesize;

    template <bool ascending, SelectedSide side>
    struct LessFiletime;

    template <bool ascending, SelectedSide side>
    struct LessExtension;

    template <bool ascending>
    struct LessCmpResult;

    template <bool ascending>
    struct LessSyncDirection;

    std::unique_ptr<SortInfo> currentSort;
};







//##################### implementation #########################################

inline
const FileSystemObject* GridView::getObject(size_t row) const
{
    return row < viewRef.size() ?
           FileSystemObject::retrieve(viewRef[row]) : nullptr;
}

inline
FileSystemObject* GridView::getObject(size_t row)
{
    //code re-use of const method: see Meyers Effective C++
    return const_cast<FileSystemObject*>(static_cast<const GridView&>(*this).getObject(row));
}
}


#endif // GRIDVIEW_H_INCLUDED
