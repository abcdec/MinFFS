// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SORTING_H_82574232452345
#define SORTING_H_82574232452345

#include <zen/type_tools.h>
#include "../file_hierarchy.h"

namespace zen
{
namespace
{
struct CompileTimeReminder : public FSObjectVisitor
{
    void visit(const FilePair&    fileObj) override {}
    void visit(const SymlinkPair& linkObj) override {}
    void visit(const DirPair&     dirObj ) override {}
} checkDymanicCasts; //just a compile-time reminder to manually check dynamic casts in this file when needed
}

inline
bool isDirectoryPair(const FileSystemObject& fsObj)
{
    return dynamic_cast<const DirPair*>(&fsObj) != nullptr;
}


template <bool ascending, SelectedSide side> inline
bool lessShortFileName(const FileSystemObject& a, const FileSystemObject& b)
{
    //sort order: first files/symlinks, then directories then empty rows

    //empty rows always last
    if (a.isEmpty<side>())
        return false;
    else if (b.isEmpty<side>())
        return true;

    //directories after files/symlinks:
    if (isDirectoryPair(a))
    {
        if (!isDirectoryPair(b))
            return false;
    }
    else if (isDirectoryPair(b))
        return true;

    //sort directories and files/symlinks by short name
    return makeSortDirection(LessFilename(), Int2Type<ascending>())(a.getItemName<side>(), b.getItemName<side>());
}

template <bool ascending, SelectedSide side> inline
bool lessFullPath(const FileSystemObject& a, const FileSystemObject& b)
{
    //empty rows always last
    if (a.isEmpty<side>())
        return false;
    else if (b.isEmpty<side>())
        return true;

    return makeSortDirection(LessFilename(), Int2Type<ascending>())(a.getFullPath<side>(), b.getFullPath<side>());
}


template <bool ascending>  inline //side currently unused!
bool lessRelativeFolder(const FileSystemObject& a, const FileSystemObject& b)
{
    const bool isDirectoryA = isDirectoryPair(a);
    const Zstring& relFolderA = isDirectoryA ?
                                a.getPairRelativePath() : //directory
                                beforeLast(a.getPairRelativePath(), FILE_NAME_SEPARATOR); //returns empty string if ch not found

    const bool isDirectoryB = isDirectoryPair(b);
    const Zstring& relFolderB = isDirectoryB ?
                                b.getPairRelativePath() : //directory
                                beforeLast(b.getPairRelativePath(), FILE_NAME_SEPARATOR); //returns empty string if ch not found

    //compare relative names without filepaths first
    const int rv = cmpFileName(relFolderA, relFolderB);
    if (rv != 0)
        return makeSortDirection(std::less<int>(), Int2Type<ascending>())(rv, 0);

    //compare the filepaths
    if (isDirectoryB) //directories shall appear before files
        return false;
    else if (isDirectoryA)
        return true;

    return LessFilename()(a.getPairShortName(), b.getPairShortName());
}


template <bool ascending, SelectedSide side> inline
bool lessFilesize(const FileSystemObject& a, const FileSystemObject& b)
{
    //empty rows always last
    if (a.isEmpty<side>())
        return false;
    else if (b.isEmpty<side>())
        return true;

    //directories second last
    if (isDirectoryPair(a))
        return false;
    else if (isDirectoryPair(b))
        return true;

    const FilePair* fileObjA = dynamic_cast<const FilePair*>(&a);
    const FilePair* fileObjB = dynamic_cast<const FilePair*>(&b);

    //then symlinks
    if (!fileObjA)
        return false;
    else if (!fileObjB)
        return true;

    //return list beginning with largest files first
    return makeSortDirection(std::less<std::uint64_t>(), Int2Type<ascending>())(fileObjA->getFileSize<side>(), fileObjB->getFileSize<side>());
}


template <bool ascending, SelectedSide side> inline
bool lessFiletime(const FileSystemObject& a, const FileSystemObject& b)
{
    if (a.isEmpty<side>())
        return false;  //empty rows always last
    else if (b.isEmpty<side>())
        return true;  //empty rows always last

    const FilePair* fileObjA = dynamic_cast<const FilePair*>(&a);
    const FilePair* fileObjB = dynamic_cast<const FilePair*>(&b);

    const SymlinkPair* linkObjA = dynamic_cast<const SymlinkPair*>(&a);
    const SymlinkPair* linkObjB = dynamic_cast<const SymlinkPair*>(&b);

    if (!fileObjA && !linkObjA)
        return false; //directories last
    else if (!fileObjB && !linkObjB)
        return true;  //directories last

    const std::int64_t dateA = fileObjA ? fileObjA->getLastWriteTime<side>() : linkObjA->getLastWriteTime<side>();
    const std::int64_t dateB = fileObjB ? fileObjB->getLastWriteTime<side>() : linkObjB->getLastWriteTime<side>();

    //return list beginning with newest files first
    return makeSortDirection(std::less<std::int64_t>(), Int2Type<ascending>())(dateA, dateB);
}


template <bool ascending, SelectedSide side> inline
bool lessExtension(const FileSystemObject& a, const FileSystemObject& b)
{
    if (a.isEmpty<side>())
        return false;  //empty rows always last
    else if (b.isEmpty<side>())
        return true;  //empty rows always last

    if (dynamic_cast<const DirPair*>(&a))
        return false; //directories last
    else if (dynamic_cast<const DirPair*>(&b))
        return true;  //directories last

    auto getExtension = [&](const FileSystemObject& fsObj) -> Zstring
    {
        const Zstring& shortName = fsObj.getItemName<side>();
        const size_t pos = shortName.rfind(Zchar('.'));
        return pos == Zstring::npos ? Zstring() : Zstring(shortName.begin() + pos + 1, shortName.end());
    };

    return makeSortDirection(LessFilename(), Int2Type<ascending>())(getExtension(a), getExtension(b));
}


template <bool ascending> inline
bool lessCmpResult(const FileSystemObject& a, const FileSystemObject& b)
{
    //presort result: equal shall appear at end of list
    if (a.getCategory() == FILE_EQUAL)
        return false;
    if (b.getCategory() == FILE_EQUAL)
        return true;

    return makeSortDirection(std::less<CompareFilesResult>(), Int2Type<ascending>())(a.getCategory(), b.getCategory());
}


template <bool ascending> inline
bool lessSyncDirection(const FileSystemObject& a, const FileSystemObject& b)
{
    return makeSortDirection(std::less<SyncOperation>(), Int2Type<ascending>())(a.getSyncOperation(), b.getSyncOperation());
}
}

#endif //SORTING_H_82574232452345
