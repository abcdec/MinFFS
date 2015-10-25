// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILEHIERARCHY_H_INCLUDED
#define FILEHIERARCHY_H_INCLUDED

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

namespace zen
{
struct FileDescriptor
{
    FileDescriptor() : lastWriteTimeRaw(), fileSize(), fileIdx(), devId(), isFollowedSymlink() {}
    FileDescriptor(std::int64_t lastWriteTimeRawIn,
                   std::uint64_t fileSizeIn,
                   const FileId& idIn,
                   bool isSymlink) :
        lastWriteTimeRaw(lastWriteTimeRawIn),
        fileSize(fileSizeIn),
        fileIdx(idIn.second),
        devId(idIn.first),
        isFollowedSymlink(isSymlink) {}

    std::int64_t lastWriteTimeRaw; //number of seconds since Jan. 1st 1970 UTC, same semantics like time_t (== signed long)
    std::uint64_t fileSize;
    FileIndex fileIdx; // == file id: optional! (however, always set on Linux, and *generally* available on Windows)
    DeviceId  devId;   //split into file id into components to avoid padding overhead of a std::pair!
    bool isFollowedSymlink;
};

inline
FileId getFileId(const FileDescriptor& fd) { return FileId(fd.devId, fd.fileIdx); }

struct LinkDescriptor
{
    LinkDescriptor() : lastWriteTimeRaw() {}
    explicit LinkDescriptor(std::int64_t lastWriteTimeRawIn) : lastWriteTimeRaw(lastWriteTimeRawIn) {}

    std::int64_t lastWriteTimeRaw; //number of seconds since Jan. 1st 1970 UTC, same semantics like time_t (== signed long)
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


class BaseDirPair;
class DirPair;
class FilePair;
class SymlinkPair;
class FileSystemObject;

//------------------------------------------------------------------

struct DirContainer
{
    //------------------------------------------------------------------
    typedef std::map<Zstring, DirContainer,   LessFilename> DirList;  //
    typedef std::map<Zstring, FileDescriptor, LessFilename> FileList; //key: file name
    typedef std::map<Zstring, LinkDescriptor, LessFilename> LinkList; //
    //------------------------------------------------------------------

    DirContainer() = default;
    DirContainer           (const DirContainer&) = delete; //catch accidental (and unnecessary) copying
    DirContainer& operator=(const DirContainer&) = delete; //

    DirList  dirs;
    FileList files;
    LinkList links; //non-followed symlinks

    //convenience
    DirContainer& addSubDir(const Zstring& shortName)
    {
        return dirs[shortName]; //value default-construction is okay here
        //return dirs.emplace(shortName, DirContainer()).first->second;
    }

    void addSubFile(const Zstring& shortName, const FileDescriptor& fileData)
    {
        auto rv = files.emplace(shortName, fileData);
        if (!rv.second) //update entry if already existing (e.g. during folder traverser "retry")
            rv.first->second = fileData;
    }

    void addSubLink(const Zstring& shortName, const LinkDescriptor& linkData)
    {
        auto rv = links.emplace(shortName, linkData);
        if (!rv.second)
            rv.first->second = linkData;
    }
};

/*------------------------------------------------------------------
    inheritance diagram:

                  ObjectMgr
                     /|\
                      |
               FileSystemObject         HierarchyObject
                     /|\                      /|\
       _______________|_______________   ______|______
      |               |               | |             |
 SymlinkPair       FilePair         DirPair      BaseDirPair

------------------------------------------------------------------*/

class HierarchyObject
{
    friend class DirPair;
    friend class FileSystemObject;

public:
    typedef FixedList<FilePair>    SubFileVec; //MergeSides::execute() requires a structure that doesn't invalidate pointers after push_back()
    typedef FixedList<SymlinkPair> SubLinkVec; //Note: deque<> has circular dependency in VCPP!
    typedef FixedList<DirPair>     SubDirVec;

    DirPair& addSubDir(const Zstring& shortNameLeft,
                       const Zstring& shortNameRight,
                       CompareDirResult defaultCmpResult);

    template <SelectedSide side>
    DirPair& addSubDir(const Zstring& shortName); //dir exists on one side only


    FilePair& addSubFile(const Zstring&        shortNameLeft,
                         const FileDescriptor& left,          //file exists on both sides
                         CompareFilesResult    defaultCmpResult,
                         const Zstring&        shortNameRight,
                         const FileDescriptor& right);

    template <SelectedSide side>
    FilePair& addSubFile(const Zstring&          shortName, //file exists on one side only
                         const FileDescriptor&   descr);

    SymlinkPair& addSubLink(const Zstring&        shortNameLeft,
                            const LinkDescriptor& left,  //link exists on both sides
                            CompareSymlinkResult  defaultCmpResult,
                            const Zstring&        shortNameRight,
                            const LinkDescriptor& right);

    template <SelectedSide side>
    SymlinkPair& addSubLink(const Zstring&        shortName, //link exists on one side only
                            const LinkDescriptor& descr);

    const SubFileVec& refSubFiles() const { return subFiles; }
    /**/  SubFileVec& refSubFiles()       { return subFiles; }

    const SubLinkVec& refSubLinks() const { return subLinks; }
    /**/  SubLinkVec& refSubLinks()       { return subLinks; }

    const SubDirVec& refSubDirs() const { return subDirs; }
    /**/  SubDirVec& refSubDirs()       { return subDirs; }

    BaseDirPair& getRoot() { return root_; }

    const Zstring& getPairRelativePathPf() const { return pairRelPathPf; } //postfixed or empty!

protected:
    HierarchyObject(const Zstring& relPathPf,
                    BaseDirPair& baseDirObj) :
        pairRelPathPf(relPathPf),
        root_(baseDirObj) {}

    ~HierarchyObject() {} //don't need polymorphic deletion

    virtual void flip();

    void removeEmptyRec();

private:
    virtual void notifySyncCfgChanged() {};

    HierarchyObject           (const HierarchyObject&) = delete; //this class is referenced by it's child elements => make it non-copyable/movable!
    HierarchyObject& operator=(const HierarchyObject&) = delete;

    SubFileVec subFiles; //contained file maps
    SubLinkVec subLinks; //contained symbolic link maps
    SubDirVec  subDirs;  //contained directory maps

    Zstring pairRelPathPf; //postfixed or empty
    BaseDirPair& root_;
};

//------------------------------------------------------------------

class BaseDirPair : public HierarchyObject //synchronization base directory
{
public:
    BaseDirPair(const Zstring& dirPostfixedLeft,
                bool dirExistsLeft,
                const Zstring& dirPostfixedRight,
                bool dirExistsRight,
                const HardFilter::FilterRef& filter,
                CompareVariant cmpVar,
                int fileTimeTolerance,
                unsigned int optTimeShiftHours) :
#ifdef _MSC_VER
#pragma warning(suppress: 4355) //"The this pointer is valid only within nonstatic member functions. It cannot be used in the initializer list for a base class."
#endif
        HierarchyObject(Zstring(), *this),
        filter_(filter), cmpVar_(cmpVar), fileTimeTolerance_(fileTimeTolerance), optTimeShiftHours_(optTimeShiftHours),
        baseDirPfL     (dirPostfixedLeft ),
        baseDirPfR     (dirPostfixedRight),
        dirExistsLeft_ (dirExistsLeft    ),
        dirExistsRight_(dirExistsRight) {}

    template <SelectedSide side> const Zstring& getBaseDirPf() const; //base sync directory postfixed with FILE_NAME_SEPARATOR (or empty!)
    static void removeEmpty(BaseDirPair& baseDir) { baseDir.removeEmptyRec(); }; //physically remove all invalid entries (where both sides are empty) recursively

    template <SelectedSide side> bool isExisting() const; //status of directory existence at the time of comparison!
    template <SelectedSide side> void setExisting(bool value); //update after creating the directory in FFS

    //get settings which were used while creating BaseDirPair
    const HardFilter&   getFilter() const { return *filter_; }
    CompareVariant getCompVariant() const { return cmpVar_; }
    int getFileTimeTolerance() const { return fileTimeTolerance_; }
    unsigned int getTimeShift() const { return optTimeShiftHours_; }

    void flip() override;

private:
    const HardFilter::FilterRef filter_; //filter used while scanning directory: represents sub-view of actual files!
    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const unsigned int optTimeShiftHours_;

    Zstring baseDirPfL; //base sync dir postfixed
    Zstring baseDirPfR; //

    bool dirExistsLeft_;
    bool dirExistsRight_;
};


template <> inline
const Zstring& BaseDirPair::getBaseDirPf<LEFT_SIDE>() const { return baseDirPfL; }

template <> inline
const Zstring& BaseDirPair::getBaseDirPf<RIGHT_SIDE>() const { return baseDirPfR; }


//get rid of shared_ptr indirection
template <class IterTy,     //underlying iterator type
          class U>           //target object type
class DerefIter : public std::iterator<std::bidirectional_iterator_tag, U>
{
public:
    DerefIter() {}
    DerefIter(IterTy it) : iter(it) {}
    DerefIter(const DerefIter& other) : iter(other.iter) {}
    DerefIter& operator++() { ++iter; return *this; }
    DerefIter& operator--() { --iter; return *this; }
    DerefIter operator++(int) { DerefIter tmp(*this); operator++(); return tmp; }
    DerefIter operator--(int) { DerefIter tmp(*this); operator--(); return tmp; }
    inline friend ptrdiff_t operator-(const DerefIter& lhs, const DerefIter& rhs) { return lhs.iter - rhs.iter; }
    inline friend bool operator==(const DerefIter& lhs, const DerefIter& rhs) { return lhs.iter == rhs.iter; }
    inline friend bool operator!=(const DerefIter& lhs, const DerefIter& rhs) { return !(lhs == rhs); }
    U& operator* () { return  **iter; }
    U* operator->() { return &** iter; }
private:
    IterTy iter;
};

typedef std::vector<std::shared_ptr<BaseDirPair>> FolderComparison; //make sure pointers to sub-elements remain valid
//don't change this back to std::vector<BaseDirPair> too easily: comparison uses push_back to add entries which may result in a full copy!

DerefIter<typename FolderComparison::iterator, BaseDirPair> inline begin(FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::iterator, BaseDirPair> inline end  (FolderComparison& vect) { return vect.end  (); }
DerefIter<typename FolderComparison::const_iterator, const BaseDirPair> inline begin(const FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::const_iterator, const BaseDirPair> inline end  (const FolderComparison& vect) { return vect.end  (); }

//------------------------------------------------------------------
struct FSObjectVisitor
{
    virtual ~FSObjectVisitor() {}
    virtual void visit(const FilePair&    fileObj) = 0;
    virtual void visit(const SymlinkPair& linkObj) = 0;
    virtual void visit(const DirPair&      dirObj) = 0;
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

    Zstring getPairShortName   () const; //like getItemName() but also returns value if either side is empty
    Zstring getPairRelativePath() const; //like getRelativePath() but also returns value if either side is empty
    template <SelectedSide side>           bool isEmpty()         const;
    template <SelectedSide side> const Zstring& getItemName()     const; //case sensitive!
    template <SelectedSide side>       Zstring  getRelativePath() const; //get name relative to base sync dir without FILE_NAME_SEPARATOR prefix
    template <SelectedSide side> const Zstring& getBaseDirPf()    const; //base sync directory postfixed with FILE_NAME_SEPARATOR
    template <SelectedSide side>       Zstring  getFullPath()     const; //getFullPath() == getBaseDirPf() + getRelativePath()

    //comparison result
    CompareFilesResult getCategory() const { return cmpResult; }
    std::wstring getCatExtraDescription() const; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA

    //sync settings
    SyncDirection getSyncDir() const;
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
    const BaseDirPair& root() const  { return parent_.getRoot(); }
    /**/  BaseDirPair& root()        { return parent_.getRoot(); }

    //for use during init in "CompareProcess" only:
    template <CompareFilesResult res> void setCategory();
    void setCategoryConflict    (const std::wstring& description);
    void setCategoryDiffMetadata(const std::wstring& description);

protected:
    FileSystemObject(const Zstring& shortNameLeft,
                     const Zstring& shortNameRight,
                     HierarchyObject& parentObj,
                     CompareFilesResult defaultCmpResult) :
        cmpResult(defaultCmpResult),
        selectedForSynchronization(true),
        syncDir_(SyncDirection::NONE),
        shortNameLeft_(shortNameLeft),
        shortNameRight_(shortNameRight),
        //shortNameRight_(shortNameRight == shortNameLeft ? shortNameLeft : shortNameRight), -> strangely doesn't seem to shrink peak memory consumption at all!
        parent_(parentObj)
    {
        parent_.notifySyncCfgChanged();
    }

    ~FileSystemObject() {} //don't need polymorphic deletion
    //mustn't call parent here, it is already partially destroyed and nothing more than a pure HierarchyObject!

    virtual void flip();
    virtual void notifySyncCfgChanged() { parent().notifySyncCfgChanged(); /*propagate!*/ }

    void setSynced(const Zstring& shortName);

private:
    FileSystemObject           (const FileSystemObject&) = delete;
    FileSystemObject& operator=(const FileSystemObject&) = delete;

    virtual void removeObjectL() = 0;
    virtual void removeObjectR() = 0;

    //categorization
    std::unique_ptr<std::wstring> cmpResultDescr; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA
    CompareFilesResult cmpResult; //although this uses 4 bytes there is currently *no* space wasted in class layout!

    bool selectedForSynchronization;

    //Note: we model *four* states with following two variables => "syncDirectionConflict is empty or syncDir == NONE" is a class invariant!!!
    SyncDirection syncDir_; //1 byte: optimize memory layout!
    std::unique_ptr<std::wstring> syncDirectionConflict; //non-empty if we have a conflict setting sync-direction
    //get rid of std::wstring small string optimization (consumes 32/48 byte on VS2010 x86/x64!)

    Zstring shortNameLeft_;  //slightly redundant under linux, but on windows the "same" filepaths can differ in case
    Zstring shortNameRight_; //use as indicator: an empty name means: not existing!

    HierarchyObject& parent_;
};

//------------------------------------------------------------------

class DirPair : public FileSystemObject, public HierarchyObject
{
    friend class HierarchyObject;

public:
    void accept(FSObjectVisitor& visitor) const override;

    CompareDirResult getDirCategory() const; //returns actually used subset of CompareFilesResult

    DirPair(const Zstring& shortNameLeft,  //use empty shortname if "not existing"
            const Zstring& shortNameRight, //
            HierarchyObject& parentObj,
            CompareDirResult defaultCmpResult) :
        FileSystemObject(shortNameLeft, shortNameRight, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        HierarchyObject(getPairRelativePath() + FILE_NAME_SEPARATOR, parentObj.getRoot()),
        syncOpBuffered(SO_DO_NOTHING),
        haveBufferedSyncOp(false) {}

    SyncOperation getSyncOperation() const override;

    void setSyncedTo(const Zstring& shortName); //call after sync, sets DIR_EQUAL

private:
    void flip         () override;
    void removeObjectL() override;
    void removeObjectR() override;
    void notifySyncCfgChanged() override { haveBufferedSyncOp = false; FileSystemObject::notifySyncCfgChanged(); HierarchyObject::notifySyncCfgChanged(); }

    mutable SyncOperation syncOpBuffered; //determining sync-op for directory may be expensive as it depends on child-objects -> buffer it
    mutable bool haveBufferedSyncOp;      //
};

//------------------------------------------------------------------

class FilePair : public FileSystemObject
{
    friend class HierarchyObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    FilePair(const Zstring&        shortNameLeft, //use empty string if "not existing"
             const FileDescriptor& left,
             CompareFilesResult    defaultCmpResult,
             const Zstring&        shortNameRight, //
             const FileDescriptor& right,
             HierarchyObject& parentObj) :
        FileSystemObject(shortNameLeft, shortNameRight, parentObj, defaultCmpResult),
        dataLeft(left),
        dataRight(right),
        moveFileRef(nullptr) {}

    template <SelectedSide side> std::int64_t getLastWriteTime() const;
    template <SelectedSide side> std::uint64_t     getFileSize() const;
    template <SelectedSide side> FileId getFileId        () const;
    template <SelectedSide side> bool   isFollowedSymlink() const;

    void setMoveRef(ObjectId refId) { moveFileRef = refId; } //reference to corresponding renamed file
    ObjectId getMoveRef() const { return moveFileRef; } //may be nullptr

    CompareFilesResult getFileCategory() const;

    SyncOperation testSyncOperation(SyncDirection testSyncDir) const override; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    SyncOperation getSyncOperation() const override;

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& shortName, //call after sync, sets FILE_EQUAL
                     std::uint64_t fileSize,
                     std::int64_t lastWriteTimeTrg,
                     std::int64_t lastWriteTimeSrc,
                     const FileId& fileIdTrg,
                     const FileId& fileIdSrc,
                     bool isSymlinkTrg,
                     bool isSymlinkSrc);

private:
    SyncOperation applyMoveOptimization(SyncOperation op) const;

    void flip         () override;
    void removeObjectL() override;
    void removeObjectR() override;

    FileDescriptor dataLeft;
    FileDescriptor dataRight;

    ObjectId moveFileRef; //optional, filled by redetermineSyncDirection()
};

//------------------------------------------------------------------

class SymlinkPair : public FileSystemObject //this class models a TRUE symbolic link, i.e. one that is NEVER dereferenced: deref-links should be directly placed in class File/DirPair
{
    friend class HierarchyObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    template <SelectedSide side> std::int64_t getLastWriteTime() const; //write time of the link, NOT target!

    CompareSymlinkResult getLinkCategory()   const; //returns actually used subset of CompareFilesResult

    SymlinkPair(const Zstring&         shortNameLeft, //use empty string if "not existing"
                const LinkDescriptor&  left,
                CompareSymlinkResult   defaultCmpResult,
                const Zstring&         shortNameRight, //use empty string if "not existing"
                const LinkDescriptor&  right,
                HierarchyObject& parentObj) :
        FileSystemObject(shortNameLeft, shortNameRight, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        dataLeft(left),
        dataRight(right) {}

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& shortName, //call after sync, sets SYMLINK_EQUAL
                     std::int64_t lastWriteTimeTrg,
                     std::int64_t lastWriteTimeSrc);

private:
    void flip()          override;
    void removeObjectL() override;
    void removeObjectR() override;

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
inline void DirPair    ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void SymlinkPair::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }


inline
CompareFilesResult FilePair::getFileCategory() const
{
    return getCategory();
}


inline
CompareDirResult DirPair::getDirCategory() const
{
    return static_cast<CompareDirResult>(getCategory());
}


inline
std::wstring FileSystemObject::getCatExtraDescription() const
{
    assert(getCategory() == FILE_CONFLICT || getCategory() == FILE_DIFFERENT_METADATA);
    if (cmpResultDescr) //avoid ternary-WTF!
        return *cmpResultDescr;
    return std::wstring();
}


inline
SyncDirection FileSystemObject::getSyncDir() const
{
    return syncDir_;
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
    syncDirectionConflict = zen::make_unique<std::wstring>(description);

    notifySyncCfgChanged();
}


inline
std::wstring FileSystemObject::getSyncOpConflict() const
{
    assert(getSyncOperation() == SO_UNRESOLVED_CONFLICT);
    if (syncDirectionConflict) //avoid ternary-WTF!
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
    return SelectParam<side>::ref(shortNameLeft_, shortNameRight_).empty();
}


inline
bool FileSystemObject::isEmpty() const
{
    return isEmpty<LEFT_SIDE>() && isEmpty<RIGHT_SIDE>();
}


template <SelectedSide side> inline
const Zstring& FileSystemObject::getItemName() const
{
    return SelectParam<side>::ref(shortNameLeft_, shortNameRight_); //empty if not existing
}


template <SelectedSide side> inline
Zstring FileSystemObject::getRelativePath() const
{
    if (isEmpty<side>()) //avoid ternary-WTF!
        return Zstring();
    return parent_.getPairRelativePathPf() + getItemName<side>();
}


inline
Zstring FileSystemObject::getPairRelativePath() const
{
    return parent_.getPairRelativePathPf() + getPairShortName();
}


inline
Zstring FileSystemObject::getPairShortName() const
{
    return isEmpty<LEFT_SIDE>() ? getItemName<RIGHT_SIDE>() : getItemName<LEFT_SIDE>();
}


template <SelectedSide side> inline
Zstring FileSystemObject::getFullPath() const
{
    if (isEmpty<side>()) //avoid ternary-WTF!
        return Zstring();
    return getBaseDirPf<side>() + parent_.getPairRelativePathPf() + getItemName<side>();
}


template <SelectedSide side> inline
const Zstring& FileSystemObject::getBaseDirPf() const
{
    return root().getBaseDirPf<side>();
}


template <> inline
void FileSystemObject::removeObject<LEFT_SIDE>()
{
    cmpResult = isEmpty<RIGHT_SIDE>() ? FILE_EQUAL : FILE_RIGHT_SIDE_ONLY;
    shortNameLeft_.clear();
    removeObjectL();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
}


template <> inline
void FileSystemObject::removeObject<RIGHT_SIDE>()
{
    cmpResult = isEmpty<LEFT_SIDE>() ? FILE_EQUAL : FILE_LEFT_SIDE_ONLY;
    shortNameRight_.clear();
    removeObjectR();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
}


inline
void FileSystemObject::setSynced(const Zstring& shortName)
{
    assert(!isEmpty());
    shortNameRight_ = shortNameLeft_ = shortName;
    cmpResult = FILE_EQUAL;
    setSyncDir(SyncDirection::NONE);
}


template <CompareFilesResult res> inline
void FileSystemObject::setCategory()
{
    cmpResult = res;
}
template <> void FileSystemObject::setCategory<FILE_CONFLICT>();           //
template <> void FileSystemObject::setCategory<FILE_DIFFERENT_METADATA>(); //not defined!
template <> void FileSystemObject::setCategory<FILE_LEFT_SIDE_ONLY>();     //
template <> void FileSystemObject::setCategory<FILE_RIGHT_SIDE_ONLY>();    //

inline
void FileSystemObject::setCategoryConflict(const std::wstring& description)
{
    cmpResult = FILE_CONFLICT;
    cmpResultDescr = zen::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::setCategoryDiffMetadata(const std::wstring& description)
{
    cmpResult = FILE_DIFFERENT_METADATA;
    cmpResultDescr = zen::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::flip()
{
    std::swap(shortNameLeft_, shortNameRight_);

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
    for (FilePair& fileObj : refSubFiles())
        fileObj.flip();
    for (SymlinkPair& linkObj : refSubLinks())
        linkObj.flip();
    for (DirPair& dirObj : refSubDirs())
        dirObj.flip();
}


inline
DirPair& HierarchyObject::addSubDir(const Zstring& shortNameLeft,
                                    const Zstring& shortNameRight,
                                    CompareDirResult defaultCmpResult)
{
    subDirs.emplace_back(shortNameLeft, shortNameRight, *this, defaultCmpResult);
    return subDirs.back();
}


template <> inline
DirPair& HierarchyObject::addSubDir<LEFT_SIDE>(const Zstring& shortName)
{
    subDirs.emplace_back(shortName, Zstring(), *this, DIR_LEFT_SIDE_ONLY);
    return subDirs.back();
}


template <> inline
DirPair& HierarchyObject::addSubDir<RIGHT_SIDE>(const Zstring& shortName)
{
    subDirs.emplace_back(Zstring(), shortName, *this, DIR_RIGHT_SIDE_ONLY);
    return subDirs.back();
}


inline
FilePair& HierarchyObject::addSubFile(const Zstring&        shortNameLeft,
                                      const FileDescriptor& left,          //file exists on both sides
                                      CompareFilesResult    defaultCmpResult,
                                      const Zstring&        shortNameRight,
                                      const FileDescriptor& right)
{
    subFiles.emplace_back(shortNameLeft, left, defaultCmpResult, shortNameRight, right, *this);
    return subFiles.back();
}


template <> inline
FilePair& HierarchyObject::addSubFile<LEFT_SIDE>(const Zstring& shortName, const FileDescriptor& descr)
{
    subFiles.emplace_back(shortName, descr, FILE_LEFT_SIDE_ONLY, Zstring(), FileDescriptor(), *this);
    return subFiles.back();
}


template <> inline
FilePair& HierarchyObject::addSubFile<RIGHT_SIDE>(const Zstring& shortName, const FileDescriptor& descr)
{
    subFiles.emplace_back(Zstring(), FileDescriptor(), FILE_RIGHT_SIDE_ONLY, shortName, descr, *this);
    return subFiles.back();
}


inline
SymlinkPair& HierarchyObject::addSubLink(const Zstring&        shortNameLeft,
                                         const LinkDescriptor& left,  //link exists on both sides
                                         CompareSymlinkResult  defaultCmpResult,
                                         const Zstring&        shortNameRight,
                                         const LinkDescriptor& right)
{
    subLinks.emplace_back(shortNameLeft, left, defaultCmpResult, shortNameRight, right, *this);
    return subLinks.back();
}


template <> inline
SymlinkPair& HierarchyObject::addSubLink<LEFT_SIDE>(const Zstring& shortName, const LinkDescriptor& descr)
{
    subLinks.emplace_back(shortName, descr, SYMLINK_LEFT_SIDE_ONLY, Zstring(), LinkDescriptor(), *this);
    return subLinks.back();
}


template <> inline
SymlinkPair& HierarchyObject::addSubLink<RIGHT_SIDE>(const Zstring& shortName, const LinkDescriptor& descr)
{
    subLinks.emplace_back(Zstring(), LinkDescriptor(), SYMLINK_RIGHT_SIDE_ONLY, shortName, descr, *this);
    return subLinks.back();
}


inline
void BaseDirPair::flip()
{
    HierarchyObject::flip();
    std::swap(baseDirPfL, baseDirPfR);
    std::swap(dirExistsLeft_, dirExistsRight_);
}


inline
void DirPair::flip()
{
    HierarchyObject ::flip(); //call base class versions
    FileSystemObject::flip(); //
}


inline
void DirPair::removeObjectL()
{
    for (FilePair& fileObj : refSubFiles())
        fileObj.removeObject<LEFT_SIDE>();
    for (SymlinkPair& linkObj : refSubLinks())
        linkObj.removeObject<LEFT_SIDE>();
    for (DirPair& dirObj : refSubDirs())
        dirObj.removeObject<LEFT_SIDE>();
}


inline
void DirPair::removeObjectR()
{
    for (FilePair& fileObj : refSubFiles())
        fileObj.removeObject<RIGHT_SIDE>();
    for (SymlinkPair& linkObj : refSubLinks())
        linkObj.removeObject<RIGHT_SIDE>();
    for (DirPair& dirObj : refSubDirs())
        dirObj.removeObject<RIGHT_SIDE>();
}


template <SelectedSide side> inline
bool BaseDirPair::isExisting() const
{
    return SelectParam<side>::ref(dirExistsLeft_, dirExistsRight_);
}


template <SelectedSide side> inline
void BaseDirPair::setExisting(bool value)
{
    SelectParam<side>::ref(dirExistsLeft_, dirExistsRight_) = value;
}


inline
void FilePair::flip()
{
    FileSystemObject::flip(); //call base class version
    std::swap(dataLeft, dataRight);
}


inline
void FilePair::removeObjectL()
{
    dataLeft = FileDescriptor();
}


inline
void FilePair::removeObjectR()
{
    dataRight = FileDescriptor();
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
FileId FilePair::getFileId() const
{
    return FileId(SelectParam<side>::ref(dataLeft, dataRight).devId,
                  SelectParam<side>::ref(dataLeft, dataRight).fileIdx);
}


template <SelectedSide side> inline
bool FilePair::isFollowedSymlink() const
{
    return SelectParam<side>::ref(dataLeft, dataRight).isFollowedSymlink;
}


template <SelectedSide sideTrg> inline
void FilePair::setSyncedTo(const Zstring& shortName,
                           std::uint64_t fileSize,
                           std::int64_t lastWriteTimeTrg,
                           std::int64_t lastWriteTimeSrc,
                           const FileId& fileIdTrg,
                           const FileId& fileIdSrc,
                           bool isSymlinkTrg,
                           bool isSymlinkSrc)
{
    //FILE_EQUAL is only allowed for same short name and file size: enforced by this method!
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(dataLeft, dataRight) = FileDescriptor(lastWriteTimeTrg, fileSize, fileIdTrg, isSymlinkTrg);
    SelectParam<sideSrc>::ref(dataLeft, dataRight) = FileDescriptor(lastWriteTimeSrc, fileSize, fileIdSrc, isSymlinkSrc);

    moveFileRef = nullptr;
    FileSystemObject::setSynced(shortName); //set FileSystemObject specific part
}


template <SelectedSide sideTrg> inline
void SymlinkPair::setSyncedTo(const Zstring& shortName,
                              std::int64_t lastWriteTimeTrg,
                              std::int64_t lastWriteTimeSrc)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(dataLeft, dataRight) = LinkDescriptor(lastWriteTimeTrg);
    SelectParam<sideSrc>::ref(dataLeft, dataRight) = LinkDescriptor(lastWriteTimeSrc);

    FileSystemObject::setSynced(shortName); //set FileSystemObject specific part
}


inline
void DirPair::setSyncedTo(const Zstring& shortName)
{
    FileSystemObject::setSynced(shortName); //set FileSystemObject specific part
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


inline
void SymlinkPair::removeObjectL()
{
    dataLeft = LinkDescriptor();
}


inline
void SymlinkPair::removeObjectR()
{
    dataRight = LinkDescriptor();
}
}

#endif // FILEHIERARCHY_H_INCLUDED
