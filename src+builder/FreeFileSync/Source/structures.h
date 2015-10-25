// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FREEFILESYNC_H_INCLUDED
#define FREEFILESYNC_H_INCLUDED

#include <vector>
#include <memory>
#include <zen/zstring.h>

namespace zen
{
enum CompareVariant
{
    CMP_BY_TIME_SIZE,
    CMP_BY_CONTENT
};

std::wstring getVariantName(CompareVariant var);

enum SymLinkHandling
{
    SYMLINK_EXCLUDE,
    SYMLINK_DIRECT,
    SYMLINK_FOLLOW
};


enum class SyncDirection : unsigned char //we need to save space for use in FileSystemObject!
{
    LEFT,
    RIGHT,
    NONE
};


enum CompareFilesResult
{
    FILE_EQUAL,
    FILE_LEFT_SIDE_ONLY,
    FILE_RIGHT_SIDE_ONLY,
    FILE_LEFT_NEWER,  //CMP_BY_TIME_SIZE only!
    FILE_RIGHT_NEWER, //
    FILE_DIFFERENT_CONTENT, //CMP_BY_CONTENT only!
    FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case, modification time
    FILE_CONFLICT
};
//attention make sure these /|\  \|/ three enums match!!!
enum CompareDirResult
{
    DIR_EQUAL              = FILE_EQUAL,
    DIR_LEFT_SIDE_ONLY     = FILE_LEFT_SIDE_ONLY,
    DIR_RIGHT_SIDE_ONLY    = FILE_RIGHT_SIDE_ONLY,
    DIR_DIFFERENT_METADATA = FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case
    DIR_CONFLICT           = FILE_CONFLICT
};

enum CompareSymlinkResult
{
    SYMLINK_EQUAL           = FILE_EQUAL,
    SYMLINK_LEFT_SIDE_ONLY  = FILE_LEFT_SIDE_ONLY,
    SYMLINK_RIGHT_SIDE_ONLY = FILE_RIGHT_SIDE_ONLY,
    SYMLINK_LEFT_NEWER      = FILE_LEFT_NEWER,
    SYMLINK_RIGHT_NEWER     = FILE_RIGHT_NEWER,
    SYMLINK_DIFFERENT_CONTENT  = FILE_DIFFERENT_CONTENT,
    SYMLINK_DIFFERENT_METADATA = FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case
    SYMLINK_CONFLICT        = FILE_CONFLICT
};


std::wstring getSymbol(CompareFilesResult cmpRes);


enum SyncOperation
{
    SO_CREATE_NEW_LEFT,
    SO_CREATE_NEW_RIGHT,
    SO_DELETE_LEFT,
    SO_DELETE_RIGHT,

    SO_MOVE_LEFT_SOURCE, //SO_DELETE_LEFT    - optimization!
    SO_MOVE_LEFT_TARGET, //SO_CREATE_NEW_LEFT

    SO_MOVE_RIGHT_SOURCE, //SO_DELETE_RIGHT    - optimization!
    SO_MOVE_RIGHT_TARGET, //SO_CREATE_NEW_RIGHT

    SO_OVERWRITE_LEFT,
    SO_OVERWRITE_RIGHT,
    SO_COPY_METADATA_TO_LEFT,  //objects are already equal: transfer metadata only - optimization
    SO_COPY_METADATA_TO_RIGHT, //

    SO_DO_NOTHING, //nothing will be synced: both sides differ
    SO_EQUAL,      //nothing will be synced: both sides are equal
    SO_UNRESOLVED_CONFLICT
};

std::wstring getSymbol(SyncOperation op); //method used for exporting .csv file only!


struct DirectionSet
{
    DirectionSet() :
        exLeftSideOnly (SyncDirection::RIGHT),
        exRightSideOnly(SyncDirection::LEFT),
        leftNewer      (SyncDirection::RIGHT),
        rightNewer     (SyncDirection::LEFT),
        different      (SyncDirection::NONE),
        conflict       (SyncDirection::NONE) {}

    SyncDirection exLeftSideOnly;
    SyncDirection exRightSideOnly;
    SyncDirection leftNewer;  //CMP_BY_TIME_SIZE only!
    SyncDirection rightNewer; //
    SyncDirection different; //CMP_BY_CONTENT only!
    SyncDirection conflict;
};

DirectionSet getTwoWayUpdateSet();

inline
bool operator==(const DirectionSet& lhs, const DirectionSet& rhs)
{
    return lhs.exLeftSideOnly  == rhs.exLeftSideOnly  &&
           lhs.exRightSideOnly == rhs.exRightSideOnly &&
           lhs.leftNewer       == rhs.leftNewer       &&
           lhs.rightNewer      == rhs.rightNewer      &&
           lhs.different       == rhs.different       &&
           lhs.conflict        == rhs.conflict;
}

struct DirectionConfig //technical representation of sync-config
{
    enum Variant
    {
        TWOWAY, //use sync-database to determine directions
        MIRROR,    //predefined
        UPDATE,    //
        CUSTOM     //use custom directions
    };

    DirectionConfig() : var(TWOWAY), detectMovedFiles(false) {}

    Variant var;
    DirectionSet custom; //custom sync directions
    bool detectMovedFiles; //dependent from Variant: e.g. always active for DirectionConfig::TWOWAY! => use functions below for evaluation!
};

inline
bool operator==(const DirectionConfig& lhs, const DirectionConfig& rhs)
{
    return lhs.var              == rhs.var &&
           lhs.custom           == rhs.custom &&
           lhs.detectMovedFiles == rhs.detectMovedFiles;
    //adapt effectivelyEqual() on changes, too!
}

bool detectMovedFilesSelectable(const DirectionConfig& cfg);
bool detectMovedFilesEnabled   (const DirectionConfig& cfg);

DirectionSet extractDirections(const DirectionConfig& cfg); //get sync directions: DON'T call for DirectionConfig::TWOWAY!

std::wstring getVariantName(DirectionConfig::Variant var);

inline
bool effectivelyEqual(const DirectionConfig& lhs, const DirectionConfig& rhs)
{
    return (lhs.var == DirectionConfig::TWOWAY) == (rhs.var == DirectionConfig::TWOWAY) && //either both two-way or none
           (lhs.var == DirectionConfig::TWOWAY || extractDirections(lhs) == extractDirections(rhs)) &&
           detectMovedFilesEnabled(lhs) == detectMovedFilesEnabled(rhs);
}


struct CompConfig
{
    CompConfig() :
        compareVar(CMP_BY_TIME_SIZE),
        handleSymlinks(SYMLINK_EXCLUDE),
        optTimeShiftHours(0) {}

    CompareVariant compareVar;
    SymLinkHandling handleSymlinks;
    unsigned int optTimeShiftHours; //if != 0: treat modification times with this offset as equal
};

inline
bool operator==(const CompConfig& lhs, const CompConfig& rhs)
{
    return lhs.compareVar        == rhs.compareVar &&
           lhs.handleSymlinks    == rhs.handleSymlinks &&
           lhs.optTimeShiftHours == rhs.optTimeShiftHours;
}

inline
bool effectivelyEqual(const CompConfig& lhs, const CompConfig& rhs) { return lhs == rhs; } //no change in behavior


enum DeletionPolicy
{
    DELETE_PERMANENTLY,
    DELETE_TO_RECYCLER,
    DELETE_TO_VERSIONING
};

enum VersioningStyle
{
    VER_STYLE_REPLACE,
    VER_STYLE_ADD_TIMESTAMP,
};

struct SyncConfig
{
    SyncConfig() :
        handleDeletion(DELETE_TO_RECYCLER),
        versioningStyle(VER_STYLE_REPLACE) {}

    //sync direction settings
    DirectionConfig directionCfg;

    DeletionPolicy handleDeletion; //use Recycle, delete permanently or move to user-defined location
    //versioning options
    VersioningStyle versioningStyle;
    Zstring versioningDirectory;
    //int versionCountLimit; //max versions per file (DELETE_TO_VERSIONING); < 0 := no limit
};


inline
bool operator==(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return lhs.directionCfg        == rhs.directionCfg   &&
           lhs.handleDeletion      == rhs.handleDeletion &&
           lhs.versioningStyle     == rhs.versioningStyle &&
           lhs.versioningDirectory == rhs.versioningDirectory;
    //adapt effectivelyEqual() on changes, too!
}


inline
bool effectivelyEqual(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return effectivelyEqual(lhs.directionCfg, rhs.directionCfg) &&
           lhs.handleDeletion == rhs.handleDeletion &&
           (lhs.handleDeletion != DELETE_TO_VERSIONING || //only compare deletion directory if required!
            (lhs.versioningStyle   == rhs.versioningStyle &&
             lhs.versioningDirectory == rhs.versioningDirectory));
}


enum UnitSize
{
    USIZE_NONE,
    USIZE_BYTE,
    USIZE_KB,
    USIZE_MB
};

enum UnitTime
{
    UTIME_NONE,
    UTIME_TODAY,
    //    UTIME_THIS_WEEK,
    UTIME_THIS_MONTH,
    UTIME_THIS_YEAR,
    UTIME_LAST_X_DAYS
};

struct FilterConfig
{
    FilterConfig(const Zstring& include  = Zstr("*"),
                 const Zstring& exclude  = Zstring(),
                 size_t   timeSpanIn     = 0,
                 UnitTime unitTimeSpanIn = UTIME_NONE,
                 size_t   sizeMinIn      = 0,
                 UnitSize unitSizeMinIn  = USIZE_NONE,
                 size_t   sizeMaxIn      = 0,
                 UnitSize unitSizeMaxIn  = USIZE_NONE) :
        includeFilter(include),
        excludeFilter(exclude),
        timeSpan     (timeSpanIn),
        unitTimeSpan (unitTimeSpanIn),
        sizeMin      (sizeMinIn),
        unitSizeMin  (unitSizeMinIn),
        sizeMax      (sizeMaxIn),
        unitSizeMax  (unitSizeMaxIn) {}

    /*
    Semantics of HardFilter:
    1. using it creates a NEW folder hierarchy! -> must be considered by <Automatic>-mode! (fortunately it turns out, doing nothing already has perfect semantics :)
    2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!
    */
    Zstring includeFilter;
    Zstring excludeFilter;

    /*
    Semantics of SoftFilter:
    1. It potentially may match only one side => it MUST NOT be applied while traversing a single folder to avoid mismatches
    2. => it is applied after traversing and just marks rows, (NO deletions after comparison are allowed)
    3. => equivalent to a user temporarily (de-)selecting rows -> not relevant for <Automatic>-mode! ;)
    */
    size_t timeSpan;
    UnitTime unitTimeSpan;

    size_t sizeMin;
    UnitSize unitSizeMin;

    size_t sizeMax;
    UnitSize unitSizeMax;
};

inline
bool operator==(const FilterConfig& lhs, const FilterConfig& rhs)
{
    return lhs.includeFilter == rhs.includeFilter &&
           lhs.excludeFilter == rhs.excludeFilter &&
           lhs.timeSpan      == rhs.timeSpan      &&
           lhs.unitTimeSpan  == rhs.unitTimeSpan  &&
           lhs.sizeMin       == rhs.sizeMin       &&
           lhs.unitSizeMin   == rhs.unitSizeMin   &&
           lhs.sizeMax       == rhs.sizeMax       &&
           lhs.unitSizeMax   == rhs.unitSizeMax;
}

void resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                  size_t sizeMin,  UnitSize unitSizeMin,
                  size_t sizeMax,  UnitSize unitSizeMax,
                  std::int64_t&  timeFrom,   //unit: UTC time, seconds
                  std::uint64_t& sizeMinBy,  //unit: bytes
                  std::uint64_t& sizeMaxBy); //unit: bytes


struct FolderPairEnh //enhanced folder pairs with (optional) alternate configuration
{
    FolderPairEnh() {}

    FolderPairEnh(const Zstring& phraseLeft,
                  const Zstring& phraseRight,
                  const std::shared_ptr<const CompConfig>& cmpConfig,
                  const std::shared_ptr<const SyncConfig>& syncConfig,
                  const FilterConfig& filter) :
        dirpathPhraseLeft (phraseLeft),
        dirpathPhraseRight(phraseRight),
        altCmpConfig(cmpConfig),
        altSyncConfig(syncConfig),
        localFilter(filter) {}

    Zstring dirpathPhraseLeft;  //unresolved directory names as entered by user!
    Zstring dirpathPhraseRight; //

    std::shared_ptr<const CompConfig> altCmpConfig;  //optional
    std::shared_ptr<const SyncConfig> altSyncConfig; //
    FilterConfig localFilter;
};


inline
bool operator==(const FolderPairEnh& lhs, const FolderPairEnh& rhs)
{
    return lhs.dirpathPhraseLeft  == rhs.dirpathPhraseLeft  &&
           lhs.dirpathPhraseRight == rhs.dirpathPhraseRight &&

           (lhs.altCmpConfig.get() && rhs.altCmpConfig.get() ?
            *lhs.altCmpConfig == *rhs.altCmpConfig :
            lhs.altCmpConfig.get() == rhs.altCmpConfig.get()) &&

           (lhs.altSyncConfig.get() && rhs.altSyncConfig.get() ?
            *lhs.altSyncConfig == *rhs.altSyncConfig :
            lhs.altSyncConfig.get() == rhs.altSyncConfig.get()) &&

           lhs.localFilter == rhs.localFilter;
}


struct MainConfiguration
{
    CompConfig   cmpConfig;    //global compare settings:         may be overwritten by folder pair settings
    SyncConfig   syncCfg;      //global synchronisation settings: may be overwritten by folder pair settings
    FilterConfig globalFilter; //global filter settings:          combined with folder pair settings

    FolderPairEnh firstPair; //there needs to be at least one pair!
    std::vector<FolderPairEnh> additionalPairs;

    Zstring onCompletion; //user-defined command line

    std::wstring getCompVariantName() const;
    std::wstring getSyncVariantName() const;
};


inline
bool operator==(const MainConfiguration& lhs, const MainConfiguration& rhs)
{
    return lhs.cmpConfig        == rhs.cmpConfig       &&
           lhs.syncCfg          == rhs.syncCfg         &&
           lhs.globalFilter     == rhs.globalFilter    &&
           lhs.firstPair        == rhs.firstPair       &&
           lhs.additionalPairs  == rhs.additionalPairs &&
           lhs.onCompletion     == rhs.onCompletion;
}


//facilitate drag & drop config merge:
MainConfiguration merge(const std::vector<MainConfiguration>& mainCfgs);
}

#endif // FREEFILESYNC_H_INCLUDED
