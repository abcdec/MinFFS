// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "file_hierarchy.h"
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/file_error.h>

using namespace zen;


void HierarchyObject::removeEmptyRec()
{
    bool emptyExisting = false;
    auto isEmpty = [&](const FileSystemObject& fsObj) -> bool
    {
        const bool objEmpty = fsObj.isEmpty();
        if (objEmpty)
            emptyExisting = true;
        return objEmpty;
    };

    refSubFiles().remove_if(isEmpty);
    refSubLinks().remove_if(isEmpty);
    refSubDirs ().remove_if(isEmpty);

    if (emptyExisting) //notify if actual deletion happened
        notifySyncCfgChanged(); //mustn't call this in ~FileSystemObject(), since parent, usually a DirPair, is already partially destroyed and existing as a pure HierarchyObject!

    for (DirPair& subDir : refSubDirs())
        subDir.removeEmptyRec(); //recurse
}

namespace
{
SyncOperation getIsolatedSyncOperation(CompareFilesResult cmpResult,
                                       bool selectedForSynchronization,
                                       SyncDirection syncDir,
                                       bool hasDirConflict) //perf: std::wstring was wasteful here
{
    assert(!hasDirConflict || syncDir == SyncDirection::NONE);

    if (!selectedForSynchronization)
        return cmpResult == FILE_EQUAL ?
               SO_EQUAL :
               SO_DO_NOTHING;

    switch (cmpResult)
    {
        case FILE_LEFT_SIDE_ONLY:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_DELETE_LEFT; //delete files on left
                case SyncDirection::RIGHT:
                    return SO_CREATE_NEW_RIGHT; //copy files to right
                case SyncDirection::NONE:
                    return hasDirConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_RIGHT_SIDE_ONLY:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_CREATE_NEW_LEFT; //copy files to left
                case SyncDirection::RIGHT:
                    return SO_DELETE_RIGHT; //delete files on right
                case SyncDirection::NONE:
                    return hasDirConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_LEFT_NEWER:
        case FILE_RIGHT_NEWER:
        case FILE_DIFFERENT:
        case FILE_CONFLICT:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_OVERWRITE_LEFT; //copy from right to left
                case SyncDirection::RIGHT:
                    return SO_OVERWRITE_RIGHT; //copy from left to right
                case SyncDirection::NONE:
                    return hasDirConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_DIFFERENT_METADATA:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_COPY_METADATA_TO_LEFT;
                case SyncDirection::RIGHT:
                    return SO_COPY_METADATA_TO_RIGHT;
                case SyncDirection::NONE:
                    return hasDirConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_EQUAL:
            assert(syncDir == SyncDirection::NONE);
            return SO_EQUAL;
    }

    assert(false);
    return SO_DO_NOTHING; //dummy
}


template <class Predicate> inline
bool hasDirectChild(const HierarchyObject& hierObj, Predicate p)
{
    return std::any_of(hierObj.refSubFiles().begin(), hierObj.refSubFiles().end(), p) ||
           std::any_of(hierObj.refSubLinks().begin(), hierObj.refSubLinks().end(), p) ||
           std::any_of(hierObj.refSubDirs(). begin(), hierObj.refSubDirs(). end(), p);
}
}


SyncOperation FileSystemObject::testSyncOperation(SyncDirection testSyncDir) const //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
{
    return getIsolatedSyncOperation(getCategory(), true, testSyncDir, false);
}


SyncOperation FileSystemObject::getSyncOperation() const
{
    return getIsolatedSyncOperation(getCategory(), selectedForSynchronization, getSyncDir(), syncDirConflict.get() != nullptr);
    //no *not* make a virtual call to testSyncOperation()! See FilePair::testSyncOperation()! <- better not implement one in terms of the other!!!
}


//SyncOperation DirPair::testSyncOperation() const -> no recursion: we do NOT want to consider child elements when testing!


SyncOperation DirPair::getSyncOperation() const
{
    if (!syncOpUpToDate)
    {
        syncOpUpToDate = true;
        //redetermine...

        //suggested operation *not* considering child elements
        syncOpBuffered = FileSystemObject::getSyncOperation();

        //action for child elements may occassionally have to overwrite parent task:
        switch (syncOpBuffered)
        {
            case SO_OVERWRITE_LEFT:
            case SO_OVERWRITE_RIGHT:
            case SO_MOVE_LEFT_SOURCE:
            case SO_MOVE_LEFT_TARGET:
            case SO_MOVE_RIGHT_SOURCE:
            case SO_MOVE_RIGHT_TARGET:
                assert(false);
            case SO_CREATE_NEW_LEFT:
            case SO_CREATE_NEW_RIGHT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_COPY_METADATA_TO_RIGHT:
            case SO_EQUAL:
                break; //take over suggestion, no problem for child-elements
            case SO_DELETE_LEFT:
            case SO_DELETE_RIGHT:
            case SO_DO_NOTHING:
            case SO_UNRESOLVED_CONFLICT:
                if (isEmpty<LEFT_SIDE>())
                {
                    //1. if at least one child-element is to be created, make sure parent folder is created also
                    //note: this automatically fulfills "create parent folders even if excluded";
                    //see http://sourceforge.net/tracker/index.php?func=detail&aid=2628943&group_id=234430&atid=1093080
                    if (hasDirectChild(*this,
                                       [](const FileSystemObject& fsObj) -> bool
                {
                    const SyncOperation op = fsObj.getSyncOperation();
                        return  op == SO_CREATE_NEW_LEFT ||
                        op == SO_MOVE_LEFT_TARGET;
                    }))
                    syncOpBuffered = SO_CREATE_NEW_LEFT;
                    //2. cancel parent deletion if only a single child is not also scheduled for deletion
                    else if (syncOpBuffered == SO_DELETE_RIGHT &&
                             hasDirectChild(*this,
                                            [](const FileSystemObject& fsObj) -> bool
                {
                    if (fsObj.isEmpty())
                            return false; //fsObj may already be empty because it once contained a "move source"
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op != SO_DELETE_RIGHT &&
                        op != SO_MOVE_RIGHT_SOURCE;
                    }))
                    syncOpBuffered = SO_DO_NOTHING;
                }
                else if (isEmpty<RIGHT_SIDE>())
                {
                    if (hasDirectChild(*this,
                                       [](const FileSystemObject& fsObj) -> bool
                {
                    const SyncOperation op = fsObj.getSyncOperation();
                        return  op == SO_CREATE_NEW_RIGHT ||
                        op == SO_MOVE_RIGHT_TARGET;
                    }))
                    syncOpBuffered = SO_CREATE_NEW_RIGHT;
                    else if (syncOpBuffered == SO_DELETE_LEFT &&
                             hasDirectChild(*this,
                                            [](const FileSystemObject& fsObj) -> bool
                {
                    if (fsObj.isEmpty())
                            return false;
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op != SO_DELETE_LEFT &&
                        op != SO_MOVE_LEFT_SOURCE;
                    }))
                    syncOpBuffered = SO_DO_NOTHING;
                }
                break;
        }
    }
    return syncOpBuffered;
}


inline //it's private!
SyncOperation FilePair::applyMoveOptimization(SyncOperation op) const
{
    /*
        check whether we can optimize "create + delete" via "move":
        note: as long as we consider "create + delete" cases only, detection of renamed files, should be fine even for "binary" comparison variant!
    */
    if (moveFileRef)
        if (auto refFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(moveFileRef))) //we expect a "FilePair", but only need a "FileSystemObject"
        {
            SyncOperation opRef = refFile->FileSystemObject::getSyncOperation(); //do *not* make a virtual call!

            if (op    == SO_CREATE_NEW_LEFT &&
                opRef == SO_DELETE_LEFT)
                op = SO_MOVE_LEFT_TARGET;
            else if (op    == SO_DELETE_LEFT &&
                     opRef == SO_CREATE_NEW_LEFT)
                op = SO_MOVE_LEFT_SOURCE;
            else if (op    == SO_CREATE_NEW_RIGHT &&
                     opRef == SO_DELETE_RIGHT)
                op = SO_MOVE_RIGHT_TARGET;
            else if (op    == SO_DELETE_RIGHT &&
                     opRef == SO_CREATE_NEW_RIGHT)
                op = SO_MOVE_RIGHT_SOURCE;
        }
    return op;
}


SyncOperation FilePair::testSyncOperation(SyncDirection testSyncDir) const
{
    return applyMoveOptimization(FileSystemObject::testSyncOperation(testSyncDir));
}


SyncOperation FilePair::getSyncOperation() const
{
    return applyMoveOptimization(FileSystemObject::getSyncOperation());
}


std::wstring zen::getCategoryDescription(CompareFilesResult cmpRes)
{
    switch (cmpRes)
    {
        case FILE_LEFT_SIDE_ONLY:
            return _("Item exists on left side only");
        case FILE_RIGHT_SIDE_ONLY:
            return _("Item exists on right side only");
        case FILE_LEFT_NEWER:
            return _("Left side is newer");
        case FILE_RIGHT_NEWER:
            return _("Right side is newer");
        case FILE_DIFFERENT:
            return _("Items have different content");
        case FILE_EQUAL:
            return _("Both sides are equal");
        case FILE_DIFFERENT_METADATA:
            return _("Items differ in attributes only");
        case FILE_CONFLICT:
            return _("Conflict/item cannot be categorized");
    }
    assert(false);
    return std::wstring();
}


std::wstring zen::getCategoryDescription(const FileSystemObject& fsObj)
{
    const CompareFilesResult cmpRes = fsObj.getCategory();
    if (cmpRes == FILE_CONFLICT ||
        cmpRes == FILE_DIFFERENT_METADATA)
        return fsObj.getCatExtraDescription();

    return getCategoryDescription(cmpRes);
}


std::wstring zen::getSyncOpDescription(SyncOperation op)
{
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
            return _("Copy new item to left");
        case SO_CREATE_NEW_RIGHT:
            return _("Copy new item to right");
        case SO_DELETE_LEFT:
            return _("Delete left item");
        case SO_DELETE_RIGHT:
            return _("Delete right item");
        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
            return _("Move file on left"); //move only supported for files
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_RIGHT_TARGET:
            return _("Move file on right");
        case SO_OVERWRITE_LEFT:
            return _("Update left item");
        case SO_OVERWRITE_RIGHT:
            return _("Update right item");
        case SO_DO_NOTHING:
            return _("Do nothing");
        case SO_EQUAL:
            return _("Both sides are equal");
        case SO_COPY_METADATA_TO_LEFT:
            return _("Update attributes on left");
        case SO_COPY_METADATA_TO_RIGHT:
            return _("Update attributes on right");
        case SO_UNRESOLVED_CONFLICT: //not used on GUI, but in .csv
            return _("Conflict/item cannot be categorized");
    }
    assert(false);
    return std::wstring();
}


std::wstring zen::getSyncOpDescription(const FileSystemObject& fsObj)
{
    const SyncOperation op = fsObj.getSyncOperation();
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_DO_NOTHING:
        case SO_EQUAL:
            return getSyncOpDescription(op); //use generic description

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            //harmonize with synchronization.cpp::SynchronizeFolderPair::synchronizeFileInt, ect!!
        {
            Zstring shortNameOld = fsObj.getItemName<RIGHT_SIDE>();
            Zstring shortNameNew = fsObj.getItemName<LEFT_SIDE >();
            if (op == SO_COPY_METADATA_TO_LEFT)
                std::swap(shortNameOld, shortNameNew);

            if (shortNameOld != shortNameNew) //detected change in case
                return getSyncOpDescription(op) + L"\n" +
                       fmtFileName(shortNameOld) + L" ->\n" + //show short name only
                       fmtFileName(shortNameNew);
        }
            //fallback:
        return getSyncOpDescription(op);

        case SO_MOVE_LEFT_SOURCE:
        case SO_MOVE_LEFT_TARGET:
        case SO_MOVE_RIGHT_SOURCE:
        case SO_MOVE_RIGHT_TARGET:
            if (const FilePair* sourceFile = dynamic_cast<const FilePair*>(&fsObj))
                if (const FilePair* targetFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(sourceFile->getMoveRef())))
                {
                    const bool onLeft   = op == SO_MOVE_LEFT_SOURCE || op == SO_MOVE_LEFT_TARGET;
                    const bool isSource = op == SO_MOVE_LEFT_SOURCE || op == SO_MOVE_RIGHT_SOURCE;

                    if (!isSource)
                        std::swap(sourceFile, targetFile);

                    auto getRelName = [&](const FileSystemObject& fso, bool leftSide) { return leftSide ? fso.getRelativePath<LEFT_SIDE>() : fso.getRelativePath<RIGHT_SIDE>(); };

                    const Zstring relSource = getRelName(*sourceFile,  onLeft);
                    const Zstring relTarget = getRelName(*targetFile, !onLeft);

                    return getSyncOpDescription(op) + L"\n" +
                           (EqualFilename()(beforeLast(relSource, FILE_NAME_SEPARATOR), beforeLast(relTarget, FILE_NAME_SEPARATOR)) ? //returns empty string if ch not found
                            //detected pure "rename"
                            fmtFileName(afterLast(relSource, FILE_NAME_SEPARATOR)) + L" ->\n" + //show short name only
                            fmtFileName(afterLast(relTarget, FILE_NAME_SEPARATOR)) :
                            //"move" or "move + rename"
                            fmtFileName(relSource) + L" ->\n" +
                            fmtFileName(relTarget));
                    //attention: ::SetWindowText() doesn't handle tab characters correctly in combination with certain file names, so don't use them
                }
            break;

        case SO_UNRESOLVED_CONFLICT:
            return fsObj.getSyncOpConflict();
    }

    assert(false);
    return std::wstring();
}
