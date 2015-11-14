// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILE_HIERARCHY_H_257235289645296
#define FILE_HIERARCHY_H_257235289645296

#include <map>
#include <cstddef> //required by GCC 4.8.1 to find ptrdiff_t
#include <string>
#include <memory>
#include <functional>
#include <unordered_set>
#include <zen/zstring.h>
#include <zen/fixed_list.h>
#include <zen/stl_tools.h>
#include "structures.h"
#include <zen/file_id_def.h>
#include "structures.h"
#include "lib/hard_filter.h"
#include "fs/abstract.h"


namespace zen
{
using AFS = AbstractFileSystem;

struct FileDescriptor
{
    FileDescriptor() {}
    FileDescriptor(std::int64_t lastWriteTimeRawIn,
                   std::uint64_t fileSizeIn,
                   const AFS::FileId& idIn,
                   bool isSymlink) :
        lastWriteTimeRaw(lastWriteTimeRawIn),
        fileSize(fileSizeIn),
        fileId(idIn),
        isFollowedSymlink(isSymlink) {}

    std::int64_t lastWriteTimeRaw = 0; //number of seconds since Jan. 1st 1970 UTC, same semantics like time_t (== signed long)
    std::uint64_t fileSize = 0;
    AFS::FileId fileId {}; // optional!
    bool isFollowedSymlink = false;
};


struct LinkDescriptor
{
    LinkDescriptor() {}
    explicit LinkDescriptor(std::int64_t lastWriteTimeRawIn) : lastWriteTimeRaw(lastWriteTimeRawIn) {}

    std::int64_t lastWriteTimeRaw = 0; //number of seconds since Jan. 1st 1970 UTC, same semantics like time_t (== signed long)
};


enum SelectedSide
{
    LEFT_SIDE,
    RIGHT_SIDE
};

template <SelectedSide side>
struct OtherSide;

template <>
struct OtherSide<LEFT_SIDE> { static const SelectedSide result = RIGHT_SIDE; };

template <>
struct OtherSide<RIGHT_SIDE> { static const SelectedSide result = LEFT_SIDE; };


template <SelectedSide side>
struct SelectParam;

template <>
struct SelectParam<LEFT_SIDE>
{
    template <class T>
    static T& ref(T& left, T& right) { return left; }
};

template <>
struct SelectParam<RIGHT_SIDE>
{
    template <class T>
    static T& ref(T& left, T& right) { return right; }
};

//------------------------------------------------------------------

struct FolderContainer
{
    //------------------------------------------------------------------
    typedef std::map<Zstring, FolderContainer, LessFilePath> FolderList;  //
    typedef std::map<Zstring, FileDescriptor,  LessFilePath> FileList;    //key: file name
    typedef std::map<Zstring, LinkDescriptor,  LessFilePath> SymlinkList; //
    //------------------------------------------------------------------

    FolderContainer() = default;
    FolderContainer           (const FolderContainer&) = delete; //catch accidental (and unnecessary) copying
    FolderContainer& operator=(const FolderContainer&) = delete; //

    FolderList  folders;
    FileList    files;
    SymlinkList symlinks; //non-followed symlinks

    //convenience
    FolderContainer& addSubFolder(const Zstring& itemName)
    {
        return folders[itemName]; //value default-construction is okay here
        //return dirs.emplace(itemName, FolderContainer()).first->second;
    }

    void addSubFile(const Zstring& itemName, const FileDescriptor& fileData)
    {
        auto rv = files.emplace(itemName, fileData);
        if (!rv.second) //update entry if already existing (e.g. during folder traverser "retry")
            rv.first->second = fileData;
    }

    void addSubLink(const Zstring& itemName, const LinkDescriptor& linkData)
    {
        auto rv = symlinks.emplace(itemName, linkData);
        if (!rv.second)
            rv.first->second = linkData;
    }
};

class BaseFolderPair;
class FolderPair;
class FilePair;
class SymlinkPair;
class FileSystemObject;

/*------------------------------------------------------------------
    inheritance diagram:

                  ObjectMgr
                     /|\
                      |
               FileSystemObject         HierarchyObject
                     /|\                      /|\
       _______________|_______________   ______|______
      |               |               | |             |
 SymlinkPair       FilePair        FolderPair   BaseFolderPair

------------------------------------------------------------------*/

class HierarchyObject
{
    friend class FolderPair;
    friend class FileSystemObject;

public:
    typedef FixedList<FilePair>    FileList; //MergeSides::execute() requires a structure that doesn't invalidate pointers after push_back()
    typedef FixedList<SymlinkPair> SymlinkList; //Note: deque<> has circular dependency in VCPP!
    typedef FixedList<FolderPair>  FolderList;

    FolderPair& addSubFolder(const Zstring& itemNameLeft,
                             const Zstring& itemNameRight,
                             CompareDirResult defaultCmpResult);

    template <SelectedSide side>
    FolderPair& addSubFolder(const Zstring& itemName); //dir exists on one side only


    FilePair& addSubFile(const Zstring&        itemNameLeft,
                         const FileDescriptor& left,          //file exists on both sides
                         CompareFilesResult    defaultCmpResult,
                         const Zstring&        itemNameRight,
                         const FileDescriptor& right);

    template <SelectedSide side>
    FilePair& addSubFile(const Zstring&          itemName, //file exists on one side only
                         const FileDescriptor&   descr);

    SymlinkPair& addSubLink(const Zstring&        itemNameLeft,
                            const LinkDescriptor& left,  //link exists on both sides
                            CompareSymlinkResult  defaultCmpResult,
                            const Zstring&        itemNameRight,
                            const LinkDescriptor& right);

    template <SelectedSide side>
    SymlinkPair& addSubLink(const Zstring&        itemName, //link exists on one side only
                            const LinkDescriptor& descr);

    const FileList& refSubFiles() const { return subFiles; }
    /**/  FileList& refSubFiles()       { return subFiles; }

    const SymlinkList& refSubLinks() const { return subLinks; }
    /**/  SymlinkList& refSubLinks()       { return subLinks; }

    const FolderList& refSubFolders() const { return subFolders; }
    /**/  FolderList& refSubFolders()       { return subFolders; }

    BaseFolderPair& getBase() { return base_; }

    const Zstring& getPairRelativePathPf() const { return pairRelPathPf; } //postfixed or empty!

protected:
    HierarchyObject(const Zstring& relPathPf,
                    BaseFolderPair& baseFolder) :
        pairRelPathPf(relPathPf),
        base_(baseFolder) {}

    virtual ~HierarchyObject() {} //don't need polymorphic deletion, but we have a vtable anyway

    virtual void flip();

    void removeEmptyRec();

private:
    virtual void notifySyncCfgChanged() {}

    HierarchyObject           (const HierarchyObject&) = delete; //this class is referenced by it's child elements => make it non-copyable/movable!
    HierarchyObject& operator=(const HierarchyObject&) = delete;

    FileList    subFiles;   //contained file maps
    SymlinkList subLinks;   //contained symbolic link maps
    FolderList  subFolders; //contained directory maps

    Zstring pairRelPathPf; //postfixed or empty
    BaseFolderPair& base_;
};

//------------------------------------------------------------------

class BaseFolderPair : public HierarchyObject //synchronization base directory
{
public:
    BaseFolderPair(const AbstractPath& folderPathLeft,
                   bool dirExistsLeft,
                   const AbstractPath& folderPathRight,
                   bool dirExistsRight,
                   const HardFilter::FilterRef& filter,
                   CompareVariant cmpVar,
                   int fileTimeTolerance,
                   unsigned int optTimeShiftHours) :
        HierarchyObject(Zstring(), *this),
        filter_(filter), cmpVar_(cmpVar), fileTimeTolerance_(fileTimeTolerance), optTimeShiftHours_(optTimeShiftHours),
        dirExistsLeft_ (dirExistsLeft),
        dirExistsRight_(dirExistsRight),
        folderPathLeft_(folderPathLeft),
        folderPathRight_(folderPathRight) {}

    template <SelectedSide side> const AbstractPath& getAbstractPath() const;

    static void removeEmpty(BaseFolderPair& baseFolder) { baseFolder.removeEmptyRec(); } //physically remove all invalid entries (where both sides are empty) recursively

    template <SelectedSide side> bool isExisting() const; //status of directory existence at the time of comparison!
    template <SelectedSide side> void setExisting(bool value); //update after creating the directory in FFS

    //get settings which were used while creating BaseFolderPair
    const HardFilter&   getFilter() const { return *filter_; }
    CompareVariant getCompVariant() const { return cmpVar_; }
    int  getFileTimeTolerance() const { return fileTimeTolerance_; }
    unsigned int getTimeShift() const { return optTimeShiftHours_; }

    void flip() override;

private:
    const HardFilter::FilterRef filter_; //filter used while scanning directory: represents sub-view of actual files!
    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const unsigned int optTimeShiftHours_;

    bool dirExistsLeft_;
    bool dirExistsRight_;

    AbstractPath folderPathLeft_;
    AbstractPath folderPathRight_;
};


template <> inline
const AbstractPath& BaseFolderPair::getAbstractPath<LEFT_SIDE>() const { return folderPathLeft_; }

template <> inline
const AbstractPath& BaseFolderPair::getAbstractPath<RIGHT_SIDE>() const { return folderPathRight_; }


//get rid of shared_ptr indirection
template <class IterTy, //underlying iterator type
          class U>      //target object type
class DerefIter : public std::iterator<std::bidirectional_iterator_tag, U>
{
public:
    DerefIter() {}
    DerefIter(IterTy it) : it_(it) {}
    DerefIter(const DerefIter& other) : it_(other.it_) {}
    DerefIter& operator++() { ++it_; return *this; }
    DerefIter& operator--() { --it_; return *this; }
    inline friend DerefIter operator++(DerefIter& it, int) { DerefIter tmp(it); ++it; return tmp; }
    inline friend DerefIter operator--(DerefIter& it, int) { DerefIter tmp(it); --it; return tmp; }
    inline friend ptrdiff_t operator-(const DerefIter& lhs, const DerefIter& rhs) { return lhs.it_ - rhs.it_; }
    inline friend bool operator==(const DerefIter& lhs, const DerefIter& rhs) { return lhs.it_ == rhs.it_; }
    inline friend bool operator!=(const DerefIter& lhs, const DerefIter& rhs) { return !(lhs == rhs); }
    U& operator* () { return  **it_; }
    U* operator->() { return &** it_; }
private:
    IterTy it_;
};

typedef std::vector<std::shared_ptr<BaseFolderPair>> FolderComparison; //make sure pointers to sub-elements remain valid
//don't change this back to std::vector<BaseFolderPair> too easily: comparison uses push_back to add entries which may result in a full copy!

DerefIter<typename FolderComparison::iterator,             BaseFolderPair> inline begin(      FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::iterator,             BaseFolderPair> inline end  (      FolderComparison& vect) { return vect.end  (); }
DerefIter<typename FolderComparison::const_iterator, const BaseFolderPair> inline begin(const FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::const_iterator, const BaseFolderPair> inline end  (const FolderComparison& vect) { return vect.end  (); }

//------------------------------------------------------------------
struct FSObjectVisitor
{
    virtual ~FSObjectVisitor() {}
    virtual void visit(const FilePair&    file   ) = 0;
    virtual void visit(const SymlinkPair& symlink) = 0;
    virtual void visit(const FolderPair&  folder ) = 0;
};

//inherit from this class to allow safe random access by id instead of unsafe raw pointer
//allow for similar semantics like std::weak_ptr without having to use std::shared_ptr
template <class T>
class ObjectMgr
{
public:
    typedef       ObjectMgr* ObjectId;
    typedef const ObjectMgr* ObjectIdConst;

    ObjectIdConst  getId() const { return this; }
    /**/  ObjectId getId()       { return this; }

    static const T* retrieve(ObjectIdConst id) //returns nullptr if object is not valid anymore
    {
        auto it = activeObjects().find(id);
        return static_cast<const T*>(it == activeObjects().end() ? nullptr : *it);
    }
    static T* retrieve(ObjectId id) { return const_cast<T*>(retrieve(static_cast<ObjectIdConst>(id))); }

protected:
    ObjectMgr () { activeObjects().insert(this); }
    ~ObjectMgr() { activeObjects().erase (this); }

private:
    ObjectMgr           (const ObjectMgr& rhs) = delete;
    ObjectMgr& operator=(const ObjectMgr& rhs) = delete; //it's not well-defined what copying an objects means regarding object-identity in this context

    static std::unordered_set<const ObjectMgr*>& activeObjects() { static std::unordered_set<const ObjectMgr*> inst; return inst; } //external linkage (even in header file!)
};

//------------------------------------------------------------------

class FileSystemObject : public ObjectMgr<FileSystemObject>
{
public:
    virtual void accept(FSObjectVisitor& visitor) const = 0;

    Zstring getPairItemName    () const; //like getItemName() but also returns value if either side is empty
    Zstring getPairRelativePath() const; //like getRelativePath() but also returns value if either side is empty
    template <SelectedSide side>           bool isEmpty()         const;
    template <SelectedSide side> const Zstring& getItemName()     const; //case sensitive!
    template <SelectedSide side>       Zstring  getRelativePath() const; //get path relative to base sync dir without FILE_NAME_SEPARATOR prefix

public:
    template <SelectedSide side> AbstractPath getAbstractPath() const; //precondition: !isEmpty<side>()

    //comparison result
    CompareFilesResult getCategory() const { return cmpResult; }
    std::wstring getCatExtraDescription() const; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA

    //sync settings
    SyncDirection getSyncDir() const { return syncDir_; }
    void setSyncDir(SyncDirection newDir);
    void setSyncDirConflict(const std::wstring& description); //set syncDir = SyncDirection::NONE + fill conflict description

    bool isActive() const;
    void setActive(bool active);

    //sync operation
    virtual SyncOperation testSyncOperation(SyncDirection testSyncDir) const; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    virtual SyncOperation getSyncOperation() const;
    std::wstring getSyncOpConflict() const; //return conflict when determining sync direction or (still unresolved) conflict during categorization

    template <SelectedSide side> void removeObject();    //removes file or directory (recursively!) without physically removing the element: used by manual deletion

    bool isEmpty() const; //true, if both sides are empty

    const HierarchyObject& parent() const { return parent_; }
    /**/  HierarchyObject& parent()       { return parent_; }
    const BaseFolderPair& base() const  { return parent_.getBase(); }
    /**/  BaseFolderPair& base()        { return parent_.getBase(); }

    //for use during init in "CompareProcess" only:
    template <CompareFilesResult res> void setCategory();
    void setCategoryConflict    (const std::wstring& description);
    void setCategoryDiffMetadata(const std::wstring& description);

protected:
    FileSystemObject(const Zstring& itemNameLeft,
                     const Zstring& itemNameRight,
                     HierarchyObject& parentObj,
                     CompareFilesResult defaultCmpResult) :
        cmpResult(defaultCmpResult),
        itemNameLeft_(itemNameLeft),
        itemNameRight_(itemNameRight),
        //itemNameRight_(itemNameRight == itemNameLeft ? itemNameLeft : itemNameRight), -> strangely doesn't seem to shrink peak memory consumption at all!
        parent_(parentObj)
    {
        parent_.notifySyncCfgChanged();
    }

    virtual ~FileSystemObject() {} //don't need polymorphic deletion, but we have a vtable anyway
    //mustn't call parent here, it is already partially destroyed and nothing more than a pure HierarchyObject!

    virtual void flip();
    virtual void notifySyncCfgChanged() { parent().notifySyncCfgChanged(); /*propagate!*/ }

    void setSynced(const Zstring& itemName);

private:
    FileSystemObject           (const FileSystemObject&) = delete;
    FileSystemObject& operator=(const FileSystemObject&) = delete;

    virtual void removeObjectL() = 0;
    virtual void removeObjectR() = 0;

    //categorization
    std::unique_ptr<std::wstring> cmpResultDescr; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA
    CompareFilesResult cmpResult; //although this uses 4 bytes there is currently *no* space wasted in class layout!

    bool selectedForSynchronization = true;

    //Note: we model *four* states with following two variables => "syncDirectionConflict is empty or syncDir == NONE" is a class invariant!!!
    SyncDirection syncDir_ = SyncDirection::NONE; //1 byte: optimize memory layout!
    std::unique_ptr<std::wstring> syncDirectionConflict; //non-empty if we have a conflict setting sync-direction
    //get rid of std::wstring small string optimization (consumes 32/48 byte on VS2010 x86/x64!)

    Zstring itemNameLeft_;  //slightly redundant under linux, but on windows the "same" filepaths can differ in case
    Zstring itemNameRight_; //use as indicator: an empty name means: not existing!

    HierarchyObject& parent_;
};

//------------------------------------------------------------------

class FolderPair : public FileSystemObject, public HierarchyObject
{
    friend class HierarchyObject;

public:
    void accept(FSObjectVisitor& visitor) const override;

    CompareDirResult getDirCategory() const; //returns actually used subset of CompareFilesResult

    FolderPair(const Zstring& itemNameLeft,  //use empty itemName if "not existing"
               const Zstring& itemNameRight, //
               HierarchyObject& parentObj,
               CompareDirResult defaultCmpResult) :
        FileSystemObject(itemNameLeft, itemNameRight, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        HierarchyObject(getPairRelativePath() + FILE_NAME_SEPARATOR, parentObj.getBase())  {}

    SyncOperation getSyncOperation() const override;

    void setSyncedTo(const Zstring& itemName); //call after sync, sets DIR_EQUAL

private:
    void flip         () override;
    void removeObjectL() override;
    void removeObjectR() override;
    void notifySyncCfgChanged() override { haveBufferedSyncOp = false; FileSystemObject::notifySyncCfgChanged(); HierarchyObject::notifySyncCfgChanged(); }

    mutable SyncOperation syncOpBuffered = SO_DO_NOTHING; //determining sync-op for directory may be expensive as it depends on child-objects -> buffer it
    mutable bool haveBufferedSyncOp      = false;         //
};

//------------------------------------------------------------------

class FilePair : public FileSystemObject
{
    friend class HierarchyObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    FilePair(const Zstring&        itemNameLeft, //use empty string if "not existing"
             const FileDescriptor& left,
             CompareFilesResult    defaultCmpResult,
             const Zstring&        itemNameRight, //
             const FileDescriptor& right,
             HierarchyObject& parentObj) :
        FileSystemObject(itemNameLeft, itemNameRight, parentObj, defaultCmpResult),
        dataLeft(left),
        dataRight(right) {}

    template <SelectedSide side> std::int64_t getLastWriteTime() const;
    template <SelectedSide side> std::uint64_t     getFileSize() const;
    template <SelectedSide side> AFS::FileId       getFileId  () const;
    template <SelectedSide side> bool        isFollowedSymlink() const;

    void setMoveRef(ObjectId refId) { moveFileRef = refId; } //reference to corresponding renamed file
    ObjectId getMoveRef() const { return moveFileRef; } //may be nullptr

    CompareFilesResult getFileCategory() const;

    SyncOperation testSyncOperation(SyncDirection testSyncDir) const override; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    SyncOperation getSyncOperation() const override;

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& itemName, //call after sync, sets FILE_EQUAL
                     std::uint64_t fileSize,
                     std::int64_t lastWriteTimeTrg,
                     std::int64_t lastWriteTimeSrc,
                     const AFS::FileId& fileIdTrg,
                     const AFS::FileId& fileIdSrc,
                     bool isSymlinkTrg,
                     bool isSymlinkSrc);

private:
    SyncOperation applyMoveOptimization(SyncOperation op) const;

    void flip         () override;
    void removeObjectL() override { dataLeft  = FileDescriptor(); }
    void removeObjectR() override { dataRight = FileDescriptor(); }

    FileDescriptor dataLeft;
    FileDescriptor dataRight;

    ObjectId moveFileRef = nullptr; //optional, filled by redetermineSyncDirection()
};

//------------------------------------------------------------------

class SymlinkPair : public FileSystemObject //this class models a TRUE symbolic link, i.e. one that is NEVER dereferenced: deref-links should be directly placed in class File/FolderPair
{
    friend class HierarchyObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    template <SelectedSide side> std::int64_t getLastWriteTime() const; //write time of the link, NOT target!

    CompareSymlinkResult getLinkCategory()   const; //returns actually used subset of CompareFilesResult

    SymlinkPair(const Zstring&         itemNameLeft, //use empty string if "not existing"
                const LinkDescriptor&  left,
                CompareSymlinkResult   defaultCmpResult,
                const Zstring&         itemNameRight, //use empty string if "not existing"
                const LinkDescriptor&  right,
                HierarchyObject& parentObj) :
        FileSystemObject(itemNameLeft, itemNameRight, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        dataLeft(left),
        dataRight(right) {}

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& itemName, //call after sync, sets SYMLINK_EQUAL
                     std::int64_t lastWriteTimeTrg,
                     std::int64_t lastWriteTimeSrc);

private:
    void flip()          override;
    void removeObjectL() override { dataLeft  = LinkDescriptor(); }
    void removeObjectR() override { dataRight = LinkDescriptor(); }

    LinkDescriptor dataLeft;
    LinkDescriptor dataRight;
};

//------------------------------------------------------------------

//generic type descriptions (usecase CSV legend, sync config)
std::wstring getCategoryDescription(CompareFilesResult cmpRes);
std::wstring getSyncOpDescription  (SyncOperation op);

//item-specific type descriptions
std::wstring getCategoryDescription(const FileSystemObject& fsObj);
std::wstring getSyncOpDescription  (const FileSystemObject& fsObj);

//------------------------------------------------------------------






















//--------------------- implementation ------------------------------------------

//inline virtual... admittedly its use may be limited
inline void FilePair   ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void FolderPair ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void SymlinkPair::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }


inline
CompareFilesResult FilePair::getFileCategory() const
{
    return getCategory();
}


inline
CompareDirResult FolderPair::getDirCategory() const
{
    return static_cast<CompareDirResult>(getCategory());
}


inline
std::wstring FileSystemObject::getCatExtraDescription() const
{
    assert(getCategory() == FILE_CONFLICT || getCategory() == FILE_DIFFERENT_METADATA);
    if (cmpResultDescr) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return *cmpResultDescr;
    return std::wstring();
}


inline
void FileSystemObject::setSyncDir(SyncDirection newDir)
{
    syncDir_ = newDir;
    syncDirectionConflict.reset();

    notifySyncCfgChanged();
}


inline
void FileSystemObject::setSyncDirConflict(const std::wstring& description)
{
    syncDir_ = SyncDirection::NONE;
    syncDirectionConflict = std::make_unique<std::wstring>(description);

    notifySyncCfgChanged();
}


inline
std::wstring FileSystemObject::getSyncOpConflict() const
{
    assert(getSyncOperation() == SO_UNRESOLVED_CONFLICT);
    if (syncDirectionConflict) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return *syncDirectionConflict;
    return std::wstring();
}


inline
bool FileSystemObject::isActive() const
{
    return selectedForSynchronization;
}


inline
void FileSystemObject::setActive(bool active)
{
    selectedForSynchronization = active;
    notifySyncCfgChanged();
}


template <SelectedSide side> inline
bool FileSystemObject::isEmpty() const
{
    return SelectParam<side>::ref(itemNameLeft_, itemNameRight_).empty();
}


inline
bool FileSystemObject::isEmpty() const
{
    return isEmpty<LEFT_SIDE>() && isEmpty<RIGHT_SIDE>();
}


template <SelectedSide side> inline
const Zstring& FileSystemObject::getItemName() const
{
    return SelectParam<side>::ref(itemNameLeft_, itemNameRight_); //empty if not existing
}


template <SelectedSide side> inline
Zstring FileSystemObject::getRelativePath() const
{
    if (isEmpty<side>()) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return Zstring();
    return parent_.getPairRelativePathPf() + getItemName<side>();
}


inline
Zstring FileSystemObject::getPairRelativePath() const
{
    return parent_.getPairRelativePathPf() + getPairItemName();
}


inline
Zstring FileSystemObject::getPairItemName() const
{
    return isEmpty<LEFT_SIDE>() ? getItemName<RIGHT_SIDE>() : getItemName<LEFT_SIDE>();
}


template <SelectedSide side> inline
AbstractPath FileSystemObject::getAbstractPath() const
{
    assert(!isEmpty<side>());
    const Zstring& itemName = isEmpty<side>() ? getItemName<OtherSide<side>::result>() : getItemName<side>();
    return AFS::appendRelPath(base().getAbstractPath<side>(), parent_.getPairRelativePathPf() + itemName);
}


template <> inline
void FileSystemObject::removeObject<LEFT_SIDE>()
{
    cmpResult = isEmpty<RIGHT_SIDE>() ? FILE_EQUAL : FILE_RIGHT_SIDE_ONLY;
    itemNameLeft_.clear();
    removeObjectL();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
}


template <> inline
void FileSystemObject::removeObject<RIGHT_SIDE>()
{
    cmpResult = isEmpty<LEFT_SIDE>() ? FILE_EQUAL : FILE_LEFT_SIDE_ONLY;
    itemNameRight_.clear();
    removeObjectR();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
}


inline
void FileSystemObject::setSynced(const Zstring& itemName)
{
    assert(!isEmpty());
    itemNameRight_ = itemNameLeft_ = itemName;
    cmpResult = FILE_EQUAL;
    setSyncDir(SyncDirection::NONE);
}


template <CompareFilesResult res> inline
void FileSystemObject::setCategory()
{
    cmpResult = res;
}
template <> void FileSystemObject::setCategory<FILE_CONFLICT>();           //
template <> void FileSystemObject::setCategory<FILE_DIFFERENT_METADATA>(); //deny use => not defined!
template <> void FileSystemObject::setCategory<FILE_LEFT_SIDE_ONLY>();     //
template <> void FileSystemObject::setCategory<FILE_RIGHT_SIDE_ONLY>();    //

inline
void FileSystemObject::setCategoryConflict(const std::wstring& description)
{
    cmpResult = FILE_CONFLICT;
    cmpResultDescr = std::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::setCategoryDiffMetadata(const std::wstring& description)
{
    cmpResult = FILE_DIFFERENT_METADATA;
    cmpResultDescr = std::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::flip()
{
    std::swap(itemNameLeft_, itemNameRight_);

    switch (cmpResult)
    {
        case FILE_LEFT_SIDE_ONLY:
            cmpResult = FILE_RIGHT_SIDE_ONLY;
            break;
        case FILE_RIGHT_SIDE_ONLY:
            cmpResult = FILE_LEFT_SIDE_ONLY;
            break;
        case FILE_LEFT_NEWER:
            cmpResult = FILE_RIGHT_NEWER;
            break;
        case FILE_RIGHT_NEWER:
            cmpResult = FILE_LEFT_NEWER;
            break;
        case FILE_DIFFERENT_CONTENT:
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA:
        case FILE_CONFLICT:
            break;
    }

    notifySyncCfgChanged();
}


inline
void HierarchyObject::flip()
{
    for (FilePair& file : refSubFiles())
        file.flip();
    for (SymlinkPair& link : refSubLinks())
        link.flip();
    for (FolderPair& folder : refSubFolders())
        folder.flip();
}


inline
FolderPair& HierarchyObject::addSubFolder(const Zstring& itemNameLeft,
                                          const Zstring& itemNameRight,
                                          CompareDirResult defaultCmpResult)
{
    subFolders.emplace_back(itemNameLeft, itemNameRight, *this, defaultCmpResult);
    return subFolders.back();
}


template <> inline
FolderPair& HierarchyObject::addSubFolder<LEFT_SIDE>(const Zstring& itemName)
{
    subFolders.emplace_back(itemName, Zstring(), *this, DIR_LEFT_SIDE_ONLY);
    return subFolders.back();
}


template <> inline
FolderPair& HierarchyObject::addSubFolder<RIGHT_SIDE>(const Zstring& itemName)
{
    subFolders.emplace_back(Zstring(), itemName, *this, DIR_RIGHT_SIDE_ONLY);
    return subFolders.back();
}


inline
FilePair& HierarchyObject::addSubFile(const Zstring&        itemNameLeft,
                                      const FileDescriptor& left,          //file exists on both sides
                                      CompareFilesResult    defaultCmpResult,
                                      const Zstring&        itemNameRight,
                                      const FileDescriptor& right)
{
    subFiles.emplace_back(itemNameLeft, left, defaultCmpResult, itemNameRight, right, *this);
    return subFiles.back();
}


template <> inline
FilePair& HierarchyObject::addSubFile<LEFT_SIDE>(const Zstring& itemName, const FileDescriptor& descr)
{
    subFiles.emplace_back(itemName, descr, FILE_LEFT_SIDE_ONLY, Zstring(), FileDescriptor(), *this);
    return subFiles.back();
}


template <> inline
FilePair& HierarchyObject::addSubFile<RIGHT_SIDE>(const Zstring& itemName, const FileDescriptor& descr)
{
    subFiles.emplace_back(Zstring(), FileDescriptor(), FILE_RIGHT_SIDE_ONLY, itemName, descr, *this);
    return subFiles.back();
}


inline
SymlinkPair& HierarchyObject::addSubLink(const Zstring&        itemNameLeft,
                                         const LinkDescriptor& left,  //link exists on both sides
                                         CompareSymlinkResult  defaultCmpResult,
                                         const Zstring&        itemNameRight,
                                         const LinkDescriptor& right)
{
    subLinks.emplace_back(itemNameLeft, left, defaultCmpResult, itemNameRight, right, *this);
    return subLinks.back();
}


template <> inline
SymlinkPair& HierarchyObject::addSubLink<LEFT_SIDE>(const Zstring& itemName, const LinkDescriptor& descr)
{
    subLinks.emplace_back(itemName, descr, SYMLINK_LEFT_SIDE_ONLY, Zstring(), LinkDescriptor(), *this);
    return subLinks.back();
}


template <> inline
SymlinkPair& HierarchyObject::addSubLink<RIGHT_SIDE>(const Zstring& itemName, const LinkDescriptor& descr)
{
    subLinks.emplace_back(Zstring(), LinkDescriptor(), SYMLINK_RIGHT_SIDE_ONLY, itemName, descr, *this);
    return subLinks.back();
}


inline
void BaseFolderPair::flip()
{
    HierarchyObject::flip();
    std::swap(dirExistsLeft_, dirExistsRight_);
    std::swap(folderPathLeft_, folderPathRight_);
}


inline
void FolderPair::flip()
{
    HierarchyObject ::flip(); //call base class versions
    FileSystemObject::flip(); //
}


inline
void FolderPair::removeObjectL()
{
    for (FilePair& file : refSubFiles())
        file.removeObject<LEFT_SIDE>();
    for (SymlinkPair& link : refSubLinks())
        link.removeObject<LEFT_SIDE>();
    for (FolderPair& folder : refSubFolders())
        folder.removeObject<LEFT_SIDE>();
}


inline
void FolderPair::removeObjectR()
{
    for (FilePair& file : refSubFiles())
        file.removeObject<RIGHT_SIDE>();
    for (SymlinkPair& link : refSubLinks())
        link.removeObject<RIGHT_SIDE>();
    for (FolderPair& folder : refSubFolders())
        folder.removeObject<RIGHT_SIDE>();
}


template <SelectedSide side> inline
bool BaseFolderPair::isExisting() const
{
    return SelectParam<side>::ref(dirExistsLeft_, dirExistsRight_);
}


template <SelectedSide side> inline
void BaseFolderPair::setExisting(bool value)
{
    SelectParam<side>::ref(dirExistsLeft_, dirExistsRight_) = value;
}


inline
void FilePair::flip()
{
    FileSystemObject::flip(); //call base class version
    std::swap(dataLeft, dataRight);
}


template <SelectedSide side> inline
std::int64_t FilePair::getLastWriteTime() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).lastWriteTimeRaw;
}


template <SelectedSide side> inline
std::uint64_t FilePair::getFileSize() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).fileSize;
}


template <SelectedSide side> inline
AFS::FileId FilePair::getFileId() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).fileId;
}


template <SelectedSide side> inline
bool FilePair::isFollowedSymlink() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).isFollowedSymlink;
}


template <SelectedSide sideTrg> inline
void FilePair::setSyncedTo(const Zstring& itemName,
                           std::uint64_t fileSize,
                           std::int64_t lastWriteTimeTrg,
                           std::int64_t lastWriteTimeSrc,
                           const AFS::FileId& fileIdTrg,
                           const AFS::FileId& fileIdSrc,
                           bool isSymlinkTrg,
                           bool isSymlinkSrc)
{
    //FILE_EQUAL is only allowed for same short name and file size: enforced by this method!
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(dataLeft, dataRight) = FileDescriptor(lastWriteTimeTrg, fileSize, fileIdTrg, isSymlinkTrg);
    SelectParam<sideSrc>::ref(dataLeft, dataRight) = FileDescriptor(lastWriteTimeSrc, fileSize, fileIdSrc, isSymlinkSrc);

    moveFileRef = nullptr;
    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


template <SelectedSide sideTrg> inline
void SymlinkPair::setSyncedTo(const Zstring& itemName,
                              std::int64_t lastWriteTimeTrg,
                              std::int64_t lastWriteTimeSrc)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(dataLeft, dataRight) = LinkDescriptor(lastWriteTimeTrg);
    SelectParam<sideSrc>::ref(dataLeft, dataRight) = LinkDescriptor(lastWriteTimeSrc);

    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


inline
void FolderPair::setSyncedTo(const Zstring& itemName)
{
    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


template <SelectedSide side> inline
std::int64_t SymlinkPair::getLastWriteTime() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).lastWriteTimeRaw;
}


inline
CompareSymlinkResult SymlinkPair::getLinkCategory() const
{
    return static_cast<CompareSymlinkResult>(getCategory());
}


inline
void SymlinkPair::flip()
{
    FileSystemObject::flip(); //call base class versions
    std::swap(dataLeft, dataRight);
}
}

#endif //FILE_HIERARCHY_H_257235289645296
