// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "process_xml.h"
#include <utility>
#include <zenxml/xml.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/xml_io.h>
#include "ffs_paths.h"

using namespace zen;
using namespace xmlAccess; //functionally needed for correct overload resolution!!!
using namespace std::rel_ops;

namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const int XML_FORMAT_VER_GLOBAL    = 1;
const int XML_FORMAT_VER_FFS_GUI   = 4; //for FFS 6.8
const int XML_FORMAT_VER_FFS_BATCH = 4; //
//-------------------------------------------------------------------------------------------------------------------------------
}

XmlType getXmlTypeNoThrow(const XmlDoc& doc) //throw()
{
    if (doc.root().getNameAs<std::string>() == "FreeFileSync")
    {
        std::string type;
        if (doc.root().getAttribute("XmlType", type))
        {
            if (type == "GUI")
                return XML_TYPE_GUI;
            else if (type == "BATCH")
                return XML_TYPE_BATCH;
            else if (type == "GLOBAL")
                return XML_TYPE_GLOBAL;
        }
    }
    return XML_TYPE_OTHER;
}


XmlType xmlAccess::getXmlType(const Zstring& filepath) //throw FileError
{
    //do NOT use zen::loadStream as it will needlessly load even huge files!
    XmlDoc doc = loadXmlDocument(filepath); //throw FileError; quick exit if file is not an FFS XML
    return ::getXmlTypeNoThrow(doc);
}


void setXmlType(XmlDoc& doc, XmlType type) //throw()
{
    switch (type)
    {
        case XML_TYPE_GUI:
            doc.root().setAttribute("XmlType", "GUI");
            break;
        case XML_TYPE_BATCH:
            doc.root().setAttribute("XmlType", "BATCH");
            break;
        case XML_TYPE_GLOBAL:
            doc.root().setAttribute("XmlType", "GLOBAL");
            break;
        case XML_TYPE_OTHER:
            assert(false);
            break;
    }
}

//################################################################################################################

Zstring xmlAccess::getGlobalConfigFile()
{
    return zen::getConfigDir() + Zstr("GlobalSettings.xml");
}


xmlAccess::XmlGuiConfig xmlAccess::convertBatchToGui(const xmlAccess::XmlBatchConfig& batchCfg) //noexcept
{
    XmlGuiConfig output;
    output.mainCfg = batchCfg.mainCfg;

    switch (batchCfg.handleError)
    {
        case ON_ERROR_POPUP:
        case ON_ERROR_STOP:
            output.handleError = ON_GUIERROR_POPUP;
            break;
        case ON_ERROR_IGNORE:
            output.handleError = ON_GUIERROR_IGNORE;
            break;
    }
    return output;
}


xmlAccess::XmlBatchConfig xmlAccess::convertGuiToBatch(const xmlAccess::XmlGuiConfig& guiCfg, const XmlBatchConfig* referenceBatchCfg) //noexcept
{
    XmlBatchConfig output;

    //try to take over batch-specific settings from reference if available
    if (referenceBatchCfg)
        output = *referenceBatchCfg;
    else
        switch (guiCfg.handleError)
        {
            case ON_GUIERROR_POPUP:
                output.handleError = ON_ERROR_POPUP;
                break;
            case ON_GUIERROR_IGNORE:
                output.handleError = ON_ERROR_IGNORE;
                break;
        }

    output.mainCfg = guiCfg.mainCfg;
    return output;
}


namespace
{
std::vector<Zstring> splitFilterByLines(const Zstring& filterPhrase)
{
    if (filterPhrase.empty())
        return std::vector<Zstring>();
    return split(filterPhrase, Zstr('\n'));
}

Zstring mergeFilterLines(const std::vector<Zstring>& filterLines)
{
    if (filterLines.empty())
        return Zstring();
    Zstring out = filterLines[0];
    std::for_each(filterLines.begin() + 1, filterLines.end(), [&](const Zstring& line) { out += Zstr('\n'); out += line; });
    return out;
}
}


namespace zen
{
template <> inline
void writeText(const CompareVariant& value, std::string& output)
{
    switch (value)
    {
        case zen::CMP_BY_TIME_SIZE:
            output = "TimeAndSize";
            break;
        case zen::CMP_BY_CONTENT:
            output = "Content";
            break;
    }
}

template <> inline
bool readText(const std::string& input, CompareVariant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TimeAndSize")
        value = zen::CMP_BY_TIME_SIZE;
    else if (tmp == "Content")
        value = zen::CMP_BY_CONTENT;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SyncDirection& value, std::string& output)
{
    switch (value)
    {
        case SyncDirection::LEFT:
            output = "left";
            break;
        case SyncDirection::RIGHT:
            output = "right";
            break;
        case SyncDirection::NONE:
            output = "none";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SyncDirection& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "left")
        value = SyncDirection::LEFT;
    else if (tmp == "right")
        value = SyncDirection::RIGHT;
    else if (tmp == "none")
        value = SyncDirection::NONE;
    else
        return false;
    return true;
}


template <> inline
void writeText(const OnError& value, std::string& output)
{
    switch (value)
    {
        case ON_ERROR_IGNORE:
            output = "Ignore";
            break;
        case ON_ERROR_POPUP:
            output = "Popup";
            break;
        case ON_ERROR_STOP:
            output = "Stop";
            break;
    }
}

template <> inline
bool readText(const std::string& input, OnError& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Ignore")
        value = ON_ERROR_IGNORE;
    else if (tmp == "Popup")
        value = ON_ERROR_POPUP;
    else if (tmp == "Stop")
        value = ON_ERROR_STOP;
    else
        return false;
    return true;
}


template <> inline
void writeText(const OnGuiError& value, std::string& output)
{
    switch (value)
    {
        case ON_GUIERROR_IGNORE:
            output = "Ignore";
            break;
        case ON_GUIERROR_POPUP:
            output = "Popup";
            break;
    }
}

template <> inline
bool readText(const std::string& input, OnGuiError& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Ignore")
        value = ON_GUIERROR_IGNORE;
    else if (tmp == "Popup")
        value = ON_GUIERROR_POPUP;
    else
        return false;
    return true;
}


template <> inline
void writeText(const FileIconSize& value, std::string& output)
{
    switch (value)
    {
        case ICON_SIZE_SMALL:
            output = "Small";
            break;
        case ICON_SIZE_MEDIUM:
            output = "Medium";
            break;
        case ICON_SIZE_LARGE:
            output = "Large";
            break;
    }
}


template <> inline
bool readText(const std::string& input, FileIconSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Small")
        value = ICON_SIZE_SMALL;
    else if (tmp == "Medium")
        value = ICON_SIZE_MEDIUM;
    else if (tmp == "Large")
        value = ICON_SIZE_LARGE;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DeletionPolicy& value, std::string& output)
{
    switch (value)
    {
        case DELETE_PERMANENTLY:
            output = "Permanent";
            break;
        case DELETE_TO_RECYCLER:
            output = "RecycleBin";
            break;
        case DELETE_TO_VERSIONING:
            output = "Versioning";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DeletionPolicy& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Permanent")
        value = DELETE_PERMANENTLY;
    else if (tmp == "RecycleBin")
        value = DELETE_TO_RECYCLER;
    else if (tmp == "Versioning")
        value = DELETE_TO_VERSIONING;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SymLinkHandling& value, std::string& output)
{
    switch (value)
    {
        case SYMLINK_EXCLUDE:
            output = "Exclude";
            break;
        case SYMLINK_DIRECT:
            output = "Direct";
            break;
        case SYMLINK_FOLLOW:
            output = "Follow";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SymLinkHandling& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Exclude")
        value = SYMLINK_EXCLUDE;
    else if (tmp == "Direct")
        value = SYMLINK_DIRECT;
    else if (tmp == "Follow")
        value = SYMLINK_FOLLOW;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeRim& value, std::string& output)
{
    switch (value)
    {
        case COL_TYPE_BASE_DIRECTORY:
            output = "Base";
            break;
        case COL_TYPE_FULL_PATH:
            output = "Full";
            break;
        case COL_TYPE_REL_FOLDER:
            output = "Rel";
            break;
        case COL_TYPE_FILENAME:
            output = "Name";
            break;
        case COL_TYPE_SIZE:
            output = "Size";
            break;
        case COL_TYPE_DATE:
            output = "Date";
            break;
        case COL_TYPE_EXTENSION:
            output = "Ext";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeRim& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Base")
        value = COL_TYPE_BASE_DIRECTORY;
    else if (tmp == "Full")
        value = COL_TYPE_FULL_PATH;
    else if (tmp == "Rel")
        value = COL_TYPE_REL_FOLDER;
    else if (tmp == "Name")
        value = COL_TYPE_FILENAME;
    else if (tmp == "Size")
        value = COL_TYPE_SIZE;
    else if (tmp == "Date")
        value = COL_TYPE_DATE;
    else if (tmp == "Ext")
        value = COL_TYPE_EXTENSION;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeNavi& value, std::string& output)
{
    switch (value)
    {
        case COL_TYPE_NAVI_BYTES:
            output = "Bytes";
            break;
        case COL_TYPE_NAVI_DIRECTORY:
            output = "Tree";
            break;
        case COL_TYPE_NAVI_ITEM_COUNT:
            output = "Count";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeNavi& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Bytes")
        value = COL_TYPE_NAVI_BYTES;
    else if (tmp == "Tree")
        value = COL_TYPE_NAVI_DIRECTORY;
    else if (tmp == "Count")
        value = COL_TYPE_NAVI_ITEM_COUNT;
    else
        return false;
    return true;
}


template <> inline
void writeText(const UnitSize& value, std::string& output)
{
    switch (value)
    {
        case USIZE_NONE:
            output = "None";
            break;
        case USIZE_BYTE:
            output = "Byte";
            break;
        case USIZE_KB:
            output = "KB";
            break;
        case USIZE_MB:
            output = "MB";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = USIZE_NONE;
    else if (tmp == "Byte")
        value = USIZE_BYTE;
    else if (tmp == "KB")
        value = USIZE_KB;
    else if (tmp == "MB")
        value = USIZE_MB;
    else
        return false;
    return true;
}

template <> inline
void writeText(const UnitTime& value, std::string& output)
{
    switch (value)
    {
        case UTIME_NONE:
            output = "None";
            break;
        case UTIME_TODAY:
            output = "Today";
            break;
        case UTIME_THIS_MONTH:
            output = "Month";
            break;
        case UTIME_THIS_YEAR:
            output = "Year";
            break;
        case UTIME_LAST_X_DAYS:
            output = "x-days";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitTime& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = UTIME_NONE;
    else if (tmp == "Today")
        value = UTIME_TODAY;
    else if (tmp == "Month")
        value = UTIME_THIS_MONTH;
    else if (tmp == "Year")
        value = UTIME_THIS_YEAR;
    else if (tmp == "x-days")
        value = UTIME_LAST_X_DAYS;
    else
        return false;
    return true;
}

template <> inline
void writeText(const VersioningStyle& value, std::string& output)
{
    switch (value)
    {
        case VER_STYLE_REPLACE:
            output = "Replace";
            break;
        case VER_STYLE_ADD_TIMESTAMP:
            output = "TimeStamp";
            break;
    }
}

template <> inline
bool readText(const std::string& input, VersioningStyle& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Replace")
        value = VER_STYLE_REPLACE;
    else if (tmp == "TimeStamp")
        value = VER_STYLE_ADD_TIMESTAMP;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DirectionConfig::Variant& value, std::string& output)
{
    switch (value)
    {
        case DirectionConfig::TWOWAY:
            output = "TwoWay";
            break;
        case DirectionConfig::MIRROR:
            output = "Mirror";
            break;
        case DirectionConfig::UPDATE:
            output = "Update";
            break;
        case DirectionConfig::CUSTOM:
            output = "Custom";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DirectionConfig::Variant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TwoWay")
        value = DirectionConfig::TWOWAY;
    else if (tmp == "Mirror")
        value = DirectionConfig::MIRROR;
    else if (tmp == "Update")
        value = DirectionConfig::UPDATE;
    else if (tmp == "Custom")
        value = DirectionConfig::CUSTOM;
    else
        return false;
    return true;
}


template <> inline
bool readStruc(const XmlElement& input, ColumnAttributeRim& value)
{
    XmlIn in(input);
    bool rv1 = in.attribute("Type",    value.type_);
    bool rv2 = in.attribute("Visible", value.visible_);
    bool rv3 = in.attribute("Width",   value.offset_); //offset == width if stretch is 0
    bool rv4 = in.attribute("Stretch", value.stretch_);
    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ColumnAttributeRim& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Type",    value.type_);
    out.attribute("Visible", value.visible_);
    out.attribute("Width",   value.offset_);
    out.attribute("Stretch", value.stretch_);
}


template <> inline
bool readStruc(const XmlElement& input, ColumnAttributeNavi& value)
{
    XmlIn in(input);
    bool rv1 = in.attribute("Type",    value.type_);
    bool rv2 = in.attribute("Visible", value.visible_);
    bool rv3 = in.attribute("Width",   value.offset_); //offset == width if stretch is 0
    bool rv4 = in.attribute("Stretch", value.stretch_);
    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ColumnAttributeNavi& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Type",    value.type_);
    out.attribute("Visible", value.visible_);
    out.attribute("Width",   value.offset_);
    out.attribute("Stretch", value.stretch_);
}


template <> inline
bool readStruc(const XmlElement& input, ViewFilterDefault& value)
{
    XmlIn in(input);

    bool success = true;
    auto readAttr = [&](XmlIn& elemIn, const char name[], bool& v)
    {
        if (!elemIn.attribute(name, v))
            success = false;
    };

    XmlIn sharedView = in["Shared"];
    readAttr(sharedView, "Equal"   , value.equal);
    readAttr(sharedView, "Conflict", value.conflict);
    readAttr(sharedView, "Excluded", value.excluded);

    XmlIn catView = in["CategoryView"];
    readAttr(catView, "LeftOnly"  , value.leftOnly);
    readAttr(catView, "RightOnly" , value.rightOnly);
    readAttr(catView, "LeftNewer" , value.leftNewer);
    readAttr(catView, "RightNewer", value.rightNewer);
    readAttr(catView, "Different" , value.different);

    XmlIn actView = in["ActionView"];
    readAttr(actView, "CreateLeft" , value.createLeft);
    readAttr(actView, "CreateRight", value.createRight);
    readAttr(actView, "UpdateLeft" , value.updateLeft);
    readAttr(actView, "UpdateRight", value.updateRight);
    readAttr(actView, "DeleteLeft" , value.deleteLeft);
    readAttr(actView, "DeleteRight", value.deleteRight);
    readAttr(actView, "DoNothing"  , value.doNothing);

    return success; //[!] avoid short-circuit evaluation above
}

template <> inline
void writeStruc(const ViewFilterDefault& value, XmlElement& output)
{
    XmlOut out(output);

    XmlOut sharedView = out["Shared"];
    sharedView.attribute("Equal"   , value.equal);
    sharedView.attribute("Conflict", value.conflict);
    sharedView.attribute("Excluded", value.excluded);

    XmlOut catView = out["CategoryView"];
    catView.attribute("LeftOnly"  , value.leftOnly);
    catView.attribute("RightOnly" , value.rightOnly);
    catView.attribute("LeftNewer" , value.leftNewer);
    catView.attribute("RightNewer", value.rightNewer);
    catView.attribute("Different" , value.different);

    XmlOut actView = out["ActionView"];
    actView.attribute("CreateLeft" , value.createLeft);
    actView.attribute("CreateRight", value.createRight);
    actView.attribute("UpdateLeft" , value.updateLeft);
    actView.attribute("UpdateRight", value.updateRight);
    actView.attribute("DeleteLeft" , value.deleteLeft);
    actView.attribute("DeleteRight", value.deleteRight);
    actView.attribute("DoNothing"  , value.doNothing);
}


template <> inline
bool readStruc(const XmlElement& input, ConfigHistoryItem& value)
{
    XmlIn in(input);
    bool rv1 = in(value.configFile);
    //bool rv2 = in.attribute("LastUsed", value.lastUseTime);
    return rv1 /*&& rv2*/;
}


template <> inline
void writeStruc(const ConfigHistoryItem& value, XmlElement& output)
{
    XmlOut out(output);
    out(value.configFile);
    //out.attribute("LastUsed", value.lastUseTime);
}
}


namespace
{
void readConfig(const XmlIn& in, CompConfig& cmpConfig)
{
    in["Variant"  ](cmpConfig.compareVar);
    in["TimeShift"](cmpConfig.optTimeShiftHours);
    in["Symlinks" ](cmpConfig.handleSymlinks);
}


void readConfig(const XmlIn& in, DirectionConfig& directCfg)
{
    in["Variant"](directCfg.var);

    XmlIn inCustDir = in["CustomDirections"];
    inCustDir["LeftOnly"  ](directCfg.custom.exLeftSideOnly);
    inCustDir["RightOnly" ](directCfg.custom.exRightSideOnly);
    inCustDir["LeftNewer" ](directCfg.custom.leftNewer);
    inCustDir["RightNewer"](directCfg.custom.rightNewer);
    inCustDir["Different" ](directCfg.custom.different);
    inCustDir["Conflict"  ](directCfg.custom.conflict);

    in["DetectMovedFiles"](directCfg.detectMovedFiles);
}


void readConfig(const XmlIn& in, SyncConfig& syncCfg)
{
    readConfig(in, syncCfg.directionCfg);

    in["DeletionPolicy"  ](syncCfg.handleDeletion);
    in["VersioningFolder"](syncCfg.versioningFolderPhrase);
    in["VersioningFolder"].attribute("Style", syncCfg.versioningStyle);
}


void readConfig(const XmlIn& in, FilterConfig& filter)
{
    std::vector<Zstring> tmp = splitFilterByLines(filter.includeFilter); //save default value
    in["Include"](tmp);
    filter.includeFilter = mergeFilterLines(tmp);

    std::vector<Zstring> tmp2 = splitFilterByLines(filter.excludeFilter); //save default value
    in["Exclude"](tmp2);
    filter.excludeFilter = mergeFilterLines(tmp2);

    in["TimeSpan"](filter.timeSpan);
    in["TimeSpan"].attribute("Type", filter.unitTimeSpan);

    in["SizeMin"](filter.sizeMin);
    in["SizeMin"].attribute("Unit", filter.unitSizeMin);

    in["SizeMax"](filter.sizeMax);
    in["SizeMax"].attribute("Unit", filter.unitSizeMax);
}


void readConfig(const XmlIn& in, FolderPairEnh& enhPair)
{
    //read folder pairs
    in["Left" ](enhPair.folderPathPhraseLeft_);
    in["Right"](enhPair.folderPathPhraseRight_);

    //###########################################################
    //alternate comp configuration (optional)
    if (XmlIn inAltCmp = in["CompareConfig"])
    {
        CompConfig altCmpCfg;
        readConfig(inAltCmp, altCmpCfg);

        enhPair.altCmpConfig = std::make_shared<CompConfig>(altCmpCfg);
    }
    //###########################################################
    //alternate sync configuration (optional)
    if (XmlIn inAltSync = in["SyncConfig"])
    {
        SyncConfig altSyncCfg;
        readConfig(inAltSync, altSyncCfg);

        enhPair.altSyncConfig = std::make_shared<SyncConfig>(altSyncCfg);
    }

    //###########################################################
    //alternate filter configuration
    if (XmlIn inLocFilter = in["LocalFilter"])
        readConfig(inLocFilter, enhPair.localFilter);
}


void readConfig(const XmlIn& in, MainConfiguration& mainCfg)
{
    //read compare settings
    XmlIn inMain = in["MainConfig"];

    readConfig(inMain["Comparison"], mainCfg.cmpConfig);
    //###########################################################

    //read sync configuration
    readConfig(inMain["SyncConfig"], mainCfg.syncCfg);
    //###########################################################

    //read filter settings
    readConfig(inMain["GlobalFilter"], mainCfg.globalFilter);

    //###########################################################
    //read all folder pairs
    mainCfg.additionalPairs.clear();

    bool firstItem = true;
    for (XmlIn inPair = inMain["FolderPairs"]["Pair"]; inPair; inPair.next())
    {
        FolderPairEnh newPair;
        readConfig(inPair, newPair);

        if (firstItem)
        {
            firstItem = false;
            mainCfg.firstPair = newPair; //set first folder pair
        }
        else
            mainCfg.additionalPairs.push_back(newPair); //set additional folder pairs
    }

    inMain["OnCompletion"](mainCfg.onCompletion);
}


void readConfig(const XmlIn& in, xmlAccess::XmlGuiConfig& config)
{
    readConfig(in, config.mainCfg); //read main config

    //read GUI specific config data
    XmlIn inGuiCfg = in["GuiConfig"];

    inGuiCfg["HandleError"](config.handleError);

    std::string val;
    if (inGuiCfg["MiddleGridView"](val)) //refactor into enum!?
        config.highlightSyncAction = val == "Action";
}


void readConfig(const XmlIn& in, xmlAccess::XmlBatchConfig& config)
{
    readConfig(in, config.mainCfg); //read main config

    //read GUI specific config data
    XmlIn inBatchCfg = in["BatchConfig"];

    inBatchCfg["HandleError"  ](config.handleError);
    inBatchCfg["RunMinimized" ](config.runMinimized);
    inBatchCfg["LogfileFolder"](config.logFolderPathPhrase);
    inBatchCfg["LogfileFolder"].attribute("Limit", config.logfilesCountLimit);
}


void readConfig(const XmlIn& in, XmlGlobalSettings& config)
{
    XmlIn inShared = in["Shared"];

    inShared["Language"].attribute("Id", config.programLanguage);

    inShared["FailSafeFileCopy"         ].attribute("Enabled", config.failsafeFileCopy);
    inShared["CopyLockedFiles"          ].attribute("Enabled", config.copyLockedFiles);
    inShared["CopyFilePermissions"      ].attribute("Enabled", config.copyFilePermissions);
    inShared["AutomaticRetry"           ].attribute("Count"  , config.automaticRetryCount);
    inShared["AutomaticRetry"           ].attribute("Delay"  , config.automaticRetryDelay);
    inShared["FileTimeTolerance"        ].attribute("Seconds", config.fileTimeTolerance);
    inShared["RunWithBackgroundPriority"].attribute("Enabled", config.runWithBackgroundPriority);
    inShared["LockDirectoriesDuringSync"].attribute("Enabled", config.createLockFile);
    inShared["VerifyCopiedFiles"        ].attribute("Enabled", config.verifyFileCopy);
    inShared["LastSyncsLogSizeMax"      ].attribute("Bytes"  , config.lastSyncsLogFileSizeMax);

    XmlIn inOpt = inShared["OptionalDialogs"];
    inOpt["WarnUnresolvedConflicts"    ].attribute("Enabled", config.optDialogs.warningUnresolvedConflicts);
    inOpt["WarnNotEnoughDiskSpace"     ].attribute("Enabled", config.optDialogs.warningNotEnoughDiskSpace);
    inOpt["WarnSignificantDifference"  ].attribute("Enabled", config.optDialogs.warningSignificantDifference);
    inOpt["WarnRecycleBinNotAvailable" ].attribute("Enabled", config.optDialogs.warningRecyclerMissing);
    inOpt["WarnInputFieldEmpty"        ].attribute("Enabled", config.optDialogs.warningInputFieldEmpty);
    inOpt["WarnDatabaseError"          ].attribute("Enabled", config.optDialogs.warningDatabaseError);
    inOpt["WarnDependentFolders"       ].attribute("Enabled", config.optDialogs.warningDependentFolders);
    inOpt["WarnFolderPairRaceCondition"].attribute("Enabled", config.optDialogs.warningFolderPairRaceCondition);
    inOpt["WarnDirectoryLockFailed"    ].attribute("Enabled", config.optDialogs.warningDirectoryLockFailed);
    inOpt["ConfirmSaveConfig"          ].attribute("Enabled", config.optDialogs.popupOnConfigChange);
    inOpt["ConfirmStartSync"           ].attribute("Enabled", config.optDialogs.confirmSyncStart);
    inOpt["ConfirmExternalCommandMassInvoke"].attribute("Enabled", config.optDialogs.confirmExternalCommandMassInvoke);

    //gui specific global settings (optional)
    XmlIn inGui = in["Gui"];
    XmlIn inWnd = inGui["MainDialog"];

    //read application window size and position
    inWnd.attribute("Width",     config.gui.dlgSize.x);
    inWnd.attribute("Height",    config.gui.dlgSize.y);
    inWnd.attribute("PosX",      config.gui.dlgPos.x);
    inWnd.attribute("PosY",      config.gui.dlgPos.y);
    inWnd.attribute("Maximized", config.gui.isMaximized);

    XmlIn inCopyTo = inWnd["ManualCopyTo"];
    inCopyTo.attribute("KeepRelativePaths", config.gui.copyToCfg.keepRelPaths);
    inCopyTo.attribute("OverwriteIfExists", config.gui.copyToCfg.overwriteIfExists);

    XmlIn inCopyToHistory = inCopyTo["FolderHistory"];
    inCopyToHistory(config.gui.copyToCfg.folderHistory);
    inCopyToHistory.attribute("LastUsedPath" , config.gui.copyToCfg.lastUsedPath);
    inCopyToHistory.attribute("MaxSize"      , config.gui.copyToCfg.historySizeMax);

    XmlIn inManualDel = inWnd["ManualDeletion"];
    inManualDel.attribute("UseRecycler", config.gui.manualDeletionUseRecycler);

    inWnd["CaseSensitiveSearch"].attribute("Enabled", config.gui.textSearchRespectCase);
    inWnd["FolderPairsVisible" ].attribute("Max",     config.gui.maxFolderPairsVisible);

    //###########################################################

    XmlIn inOverview = inWnd["OverviewPanel"];
    inOverview.attribute("ShowPercentage", config.gui.showPercentBar);
    inOverview.attribute("SortByColumn",   config.gui.naviLastSortColumn);
    inOverview.attribute("SortAscending",  config.gui.naviLastSortAscending);

    //read column attributes
    XmlIn inColNavi = inOverview["Columns"];
    inColNavi(config.gui.columnAttribNavi);

    XmlIn inMainGrid = inWnd["MainGrid"];
    inMainGrid.attribute("ShowIcons",  config.gui.showIcons);
    inMainGrid.attribute("IconSize",   config.gui.iconSize);
    inMainGrid.attribute("SashOffset", config.gui.sashOffset);

    XmlIn inColLeft = inMainGrid["ColumnsLeft"];
    inColLeft(config.gui.columnAttribLeft);

    XmlIn inColRight = inMainGrid["ColumnsRight"];
    inColRight(config.gui.columnAttribRight);
    //###########################################################

    inWnd["DefaultView" ](config.gui.viewFilterDefault);
    inWnd["Perspective4"](config.gui.guiPerspectiveLast);

    std::vector<Zstring> tmp = splitFilterByLines(config.gui.defaultExclusionFilter); //default value
    inGui["DefaultExclusionFilter"](tmp);
    config.gui.defaultExclusionFilter = mergeFilterLines(tmp);

    //load config file history
    inGui["LastUsedConfig"](config.gui.lastUsedConfigFiles);

    inGui["ConfigHistory"](config.gui.cfgFileHistory);
    inGui["ConfigHistory"].attribute("MaxSize", config.gui.cfgFileHistMax);

    inGui["FolderHistoryLeft" ](config.gui.folderHistoryLeft);
    inGui["FolderHistoryRight"](config.gui.folderHistoryRight);
    inGui["FolderHistoryLeft"].attribute("MaxSize", config.gui.folderHistMax);

    inGui["OnCompletionHistory"](config.gui.onCompletionHistory);
    inGui["OnCompletionHistory"].attribute("MaxSize", config.gui.onCompletionHistoryMax);

    //external applications
    inGui["ExternalApplications"](config.gui.externelApplications);

    //last update check
    inGui["LastOnlineCheck"  ](config.gui.lastUpdateCheck);
    inGui["LastOnlineVersion"](config.gui.lastOnlineVersion);

    //batch specific global settings
    //XmlIn inBatch = in["Batch"];
}


bool needsMigration(const XmlDoc& doc, int currentXmlFormatVer)
{
    //(try to) migrate old configuration if needed
    int xmlFormatVer = 0;
    /*bool success = */doc.root().getAttribute("XmlFormat", xmlFormatVer);
    return xmlFormatVer < currentXmlFormatVer;
}


template <class ConfigType>
void readConfig(const Zstring& filepath, XmlType type, ConfigType& cfg, int currentXmlFormatVer, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXmlDocument(filepath); //throw FileError

    if (getXmlTypeNoThrow(doc) != type) //noexcept
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filepath)));

    XmlIn in(doc);
    ::readConfig(in, cfg);

    try
    {
        checkForMappingErrors(in, filepath); //throw FileError

        //(try to) migrate old configuration if needed
        if (needsMigration(doc, currentXmlFormatVer))
            try { xmlAccess::writeConfig(cfg, filepath); /*throw FileError*/ }
            catch (FileError&) { assert(false); }   //don't bother user!
    }
    catch (const FileError& e)
    {
        warningMsg = e.toString();
    }
}
}


void xmlAccess::readConfig(const Zstring& filepath, xmlAccess::XmlGuiConfig& cfg, std::wstring& warningMsg)
{
    ::readConfig(filepath, XML_TYPE_GUI, cfg, XML_FORMAT_VER_FFS_GUI, warningMsg); //throw FileError
}


void xmlAccess::readConfig(const Zstring& filepath, xmlAccess::XmlBatchConfig& cfg, std::wstring& warningMsg)
{
    ::readConfig(filepath, XML_TYPE_BATCH, cfg, XML_FORMAT_VER_FFS_BATCH, warningMsg); //throw FileError
}


void xmlAccess::readConfig(const Zstring& filepath, xmlAccess::XmlGlobalSettings& cfg, std::wstring& warningMsg)
{
    ::readConfig(filepath, XML_TYPE_GLOBAL, cfg, XML_FORMAT_VER_GLOBAL, warningMsg); //throw FileError
}


namespace
{
template <class XmlCfg>
XmlCfg parseConfig(const XmlDoc& doc, const Zstring& filepath, int currentXmlFormatVer, std::wstring& warningMsg) //nothrow
{
    XmlIn in(doc);
    XmlCfg cfg;
    ::readConfig(in, cfg);

    try
    {
        checkForMappingErrors(in, filepath); //throw FileError

        //(try to) migrate old configuration if needed
        if (needsMigration(doc, currentXmlFormatVer))
            try { xmlAccess::writeConfig(cfg, filepath); /*throw FileError*/ }
            catch (FileError&) { assert(false); }     //don't bother user!
    }
    catch (const FileError& e)
    {
        if (warningMsg.empty())
            warningMsg = e.toString();
    }

    return cfg;
}
}


void xmlAccess::readAnyConfig(const std::vector<Zstring>& filepaths, XmlGuiConfig& config, std::wstring& warningMsg) //throw FileError
{
    assert(!filepaths.empty());

    std::vector<zen::MainConfiguration> mainCfgs;

    for (auto it = filepaths.begin(); it != filepaths.end(); ++it)
    {
        const Zstring& filepath = *it;
        const bool firstItem = it == filepaths.begin(); //init all non-"mainCfg" settings with first config file

        XmlDoc doc = loadXmlDocument(filepath); //throw FileError

        switch (getXmlTypeNoThrow(doc))
        {
            case XML_TYPE_GUI:
            {
                XmlGuiConfig guiCfg = parseConfig<XmlGuiConfig>(doc, filepath, XML_FORMAT_VER_FFS_GUI, warningMsg); //nothrow
                if (firstItem)
                    config = guiCfg;
                mainCfgs.push_back(guiCfg.mainCfg);
            }
            break;

            case XML_TYPE_BATCH:
            {
                XmlBatchConfig batchCfg = parseConfig<XmlBatchConfig>(doc, filepath, XML_FORMAT_VER_FFS_BATCH, warningMsg); //nothrow
                if (firstItem)
                    config = convertBatchToGui(batchCfg);
                mainCfgs.push_back(batchCfg.mainCfg);
            }
            break;

            case XML_TYPE_GLOBAL:
            case XML_TYPE_OTHER:
                throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filepath)));
        }
    }

    config.mainCfg = merge(mainCfgs);
}

//################################################################################################

namespace
{
void writeConfig(const CompConfig& cmpConfig, XmlOut& out)
{
    out["Variant"  ](cmpConfig.compareVar);
    out["TimeShift"](cmpConfig.optTimeShiftHours);
    out["Symlinks" ](cmpConfig.handleSymlinks);
}


void writeConfig(const DirectionConfig& directCfg, XmlOut& out)
{
    out["Variant"](directCfg.var);

    XmlOut outCustDir = out["CustomDirections"];
    outCustDir["LeftOnly"  ](directCfg.custom.exLeftSideOnly);
    outCustDir["RightOnly" ](directCfg.custom.exRightSideOnly);
    outCustDir["LeftNewer" ](directCfg.custom.leftNewer);
    outCustDir["RightNewer"](directCfg.custom.rightNewer);
    outCustDir["Different" ](directCfg.custom.different);
    outCustDir["Conflict"  ](directCfg.custom.conflict);

    out["DetectMovedFiles"](directCfg.detectMovedFiles);
}


void writeConfig(const SyncConfig& syncCfg, XmlOut& out)
{
    writeConfig(syncCfg.directionCfg, out);

    out["DeletionPolicy"  ](syncCfg.handleDeletion);
    out["VersioningFolder"](syncCfg.versioningFolderPhrase);
    out["VersioningFolder"].attribute("Style", syncCfg.versioningStyle);
}


void writeConfig(const FilterConfig& filter, XmlOut& out)
{
    out["Include"](splitFilterByLines(filter.includeFilter));
    out["Exclude"](splitFilterByLines(filter.excludeFilter));

    out["TimeSpan"](filter.timeSpan);
    out["TimeSpan"].attribute("Type", filter.unitTimeSpan);

    out["SizeMin"](filter.sizeMin);
    out["SizeMin"].attribute("Unit", filter.unitSizeMin);

    out["SizeMax"](filter.sizeMax);
    out["SizeMax"].attribute("Unit", filter.unitSizeMax);
}


void writeConfigFolderPair(const FolderPairEnh& enhPair, XmlOut& out)
{
    XmlOut outPair = out.ref().addChild("Pair");

    //read folder pairs
    outPair["Left" ](enhPair.folderPathPhraseLeft_);
    outPair["Right"](enhPair.folderPathPhraseRight_);

    //###########################################################
    //alternate comp configuration (optional)
    if (enhPair.altCmpConfig.get())
    {
        XmlOut outAlt = outPair["CompareConfig"];
        writeConfig(*enhPair.altCmpConfig, outAlt);
    }
    //###########################################################
    //alternate sync configuration (optional)
    if (enhPair.altSyncConfig.get())
    {
        XmlOut outAltSync = outPair["SyncConfig"];
        writeConfig(*enhPair.altSyncConfig, outAltSync);
    }

    //###########################################################
    //alternate filter configuration
    if (enhPair.localFilter != FilterConfig()) //don't spam .ffs_gui file with default filter entries
    {
        XmlOut outFilter = outPair["LocalFilter"];
        writeConfig(enhPair.localFilter, outFilter);
    }
}


void writeConfig(const MainConfiguration& mainCfg, XmlOut& out)
{
    XmlOut outMain = out["MainConfig"];

    XmlOut outCmp = outMain["Comparison"];

    writeConfig(mainCfg.cmpConfig, outCmp);
    //###########################################################

    XmlOut outSync = outMain["SyncConfig"];

    writeConfig(mainCfg.syncCfg, outSync);
    //###########################################################

    XmlOut outFilter = outMain["GlobalFilter"];
    //write filter settings
    writeConfig(mainCfg.globalFilter, outFilter);

    //###########################################################
    //write all folder pairs

    XmlOut outFp = outMain["FolderPairs"];

    //write first folder pair
    writeConfigFolderPair(mainCfg.firstPair, outFp);

    //write additional folder pairs
    std::for_each(mainCfg.additionalPairs.begin(), mainCfg.additionalPairs.end(),
    [&](const FolderPairEnh& fp) { writeConfigFolderPair(fp, outFp); });

    outMain["OnCompletion"](mainCfg.onCompletion);
}


void writeConfig(const XmlGuiConfig& config, XmlOut& out)
{
    writeConfig(config.mainCfg, out); //write main config

    //write GUI specific config data
    XmlOut outGuiCfg = out["GuiConfig"];

    outGuiCfg["HandleError"   ](config.handleError);
    outGuiCfg["MiddleGridView"](config.highlightSyncAction ? "Action" : "Category"); //refactor into enum!?
}

void writeConfig(const XmlBatchConfig& config, XmlOut& out)
{

    writeConfig(config.mainCfg, out); //write main config

    //write GUI specific config data
    XmlOut outBatchCfg = out["BatchConfig"];

    outBatchCfg["HandleError"  ](config.handleError);
    outBatchCfg["RunMinimized" ](config.runMinimized);
    outBatchCfg["LogfileFolder"](config.logFolderPathPhrase);
    outBatchCfg["LogfileFolder"].attribute("Limit", config.logfilesCountLimit);
}


void writeConfig(const XmlGlobalSettings& config, XmlOut& out)
{
    XmlOut outShared = out["Shared"];

    outShared["Language"].attribute("Id", config.programLanguage);

    outShared["FailSafeFileCopy"         ].attribute("Enabled", config.failsafeFileCopy);
    outShared["CopyLockedFiles"          ].attribute("Enabled", config.copyLockedFiles);
    outShared["CopyFilePermissions"      ].attribute("Enabled", config.copyFilePermissions);
    outShared["AutomaticRetry"           ].attribute("Count"  , config.automaticRetryCount);
    outShared["AutomaticRetry"           ].attribute("Delay"  , config.automaticRetryDelay);
    outShared["FileTimeTolerance"        ].attribute("Seconds", config.fileTimeTolerance);
    outShared["RunWithBackgroundPriority"].attribute("Enabled", config.runWithBackgroundPriority);
    outShared["LockDirectoriesDuringSync"].attribute("Enabled", config.createLockFile);
    outShared["VerifyCopiedFiles"        ].attribute("Enabled", config.verifyFileCopy);
    outShared["LastSyncsLogSizeMax"      ].attribute("Bytes"  , config.lastSyncsLogFileSizeMax);

    XmlOut outOpt = outShared["OptionalDialogs"];
    outOpt["WarnUnresolvedConflicts"    ].attribute("Enabled", config.optDialogs.warningUnresolvedConflicts);
    outOpt["WarnNotEnoughDiskSpace"     ].attribute("Enabled", config.optDialogs.warningNotEnoughDiskSpace);
    outOpt["WarnSignificantDifference"  ].attribute("Enabled", config.optDialogs.warningSignificantDifference);
    outOpt["WarnRecycleBinNotAvailable" ].attribute("Enabled", config.optDialogs.warningRecyclerMissing);
    outOpt["WarnInputFieldEmpty"        ].attribute("Enabled", config.optDialogs.warningInputFieldEmpty);
    outOpt["WarnDatabaseError"          ].attribute("Enabled", config.optDialogs.warningDatabaseError);
    outOpt["WarnDependentFolders"       ].attribute("Enabled", config.optDialogs.warningDependentFolders);
    outOpt["WarnFolderPairRaceCondition"].attribute("Enabled", config.optDialogs.warningFolderPairRaceCondition);
    outOpt["WarnDirectoryLockFailed"    ].attribute("Enabled", config.optDialogs.warningDirectoryLockFailed);
    outOpt["ConfirmSaveConfig"          ].attribute("Enabled", config.optDialogs.popupOnConfigChange);
    outOpt["ConfirmStartSync"           ].attribute("Enabled", config.optDialogs.confirmSyncStart);
    outOpt["ConfirmExternalCommandMassInvoke"].attribute("Enabled", config.optDialogs.confirmExternalCommandMassInvoke);

    //gui specific global settings (optional)
    XmlOut outGui = out["Gui"];
    XmlOut outWnd = outGui["MainDialog"];

    //write application window size and position
    outWnd.attribute("Width",     config.gui.dlgSize.x);
    outWnd.attribute("Height",    config.gui.dlgSize.y);
    outWnd.attribute("PosX",      config.gui.dlgPos.x);
    outWnd.attribute("PosY",      config.gui.dlgPos.y);
    outWnd.attribute("Maximized", config.gui.isMaximized);

    XmlOut outCopyTo = outWnd["ManualCopyTo"];
    outCopyTo.attribute("KeepRelativePaths", config.gui.copyToCfg.keepRelPaths);
    outCopyTo.attribute("OverwriteIfExists", config.gui.copyToCfg.overwriteIfExists);

    XmlOut outCopyToHistory = outCopyTo["FolderHistory"];
    outCopyToHistory(config.gui.copyToCfg.folderHistory);
    outCopyToHistory.attribute("LastUsedPath" , config.gui.copyToCfg.lastUsedPath);
    outCopyToHistory.attribute("MaxSize"      , config.gui.copyToCfg.historySizeMax);

    XmlOut outManualDel = outWnd["ManualDeletion"];
    outManualDel.attribute("UseRecycler", config.gui.manualDeletionUseRecycler);

    outWnd["CaseSensitiveSearch"].attribute("Enabled", config.gui.textSearchRespectCase);
    outWnd["FolderPairsVisible" ].attribute("Max",     config.gui.maxFolderPairsVisible);

    //###########################################################

    XmlOut outOverview = outWnd["OverviewPanel"];
    outOverview.attribute("ShowPercentage", config.gui.showPercentBar);
    outOverview.attribute("SortByColumn",   config.gui.naviLastSortColumn);
    outOverview.attribute("SortAscending",  config.gui.naviLastSortAscending);

    //write column attributes
    XmlOut outColNavi = outOverview["Columns"];
    outColNavi(config.gui.columnAttribNavi);

    XmlOut outMainGrid = outWnd["MainGrid"];
    outMainGrid.attribute("ShowIcons",  config.gui.showIcons);
    outMainGrid.attribute("IconSize",   config.gui.iconSize);
    outMainGrid.attribute("SashOffset", config.gui.sashOffset);

    XmlOut outColLeft = outMainGrid["ColumnsLeft"];
    outColLeft(config.gui.columnAttribLeft);

    XmlOut outColRight = outMainGrid["ColumnsRight"];
    outColRight(config.gui.columnAttribRight);
    //###########################################################

    outWnd["DefaultView" ](config.gui.viewFilterDefault);
    outWnd["Perspective4"](config.gui.guiPerspectiveLast);

    outGui["DefaultExclusionFilter"](splitFilterByLines(config.gui.defaultExclusionFilter));

    //load config file history
    outGui["LastUsedConfig"](config.gui.lastUsedConfigFiles);

    outGui["ConfigHistory" ](config.gui.cfgFileHistory);
    outGui["ConfigHistory"].attribute("MaxSize", config.gui.cfgFileHistMax);

    outGui["FolderHistoryLeft" ](config.gui.folderHistoryLeft);
    outGui["FolderHistoryRight"](config.gui.folderHistoryRight);
    outGui["FolderHistoryLeft" ].attribute("MaxSize", config.gui.folderHistMax);

    outGui["OnCompletionHistory"](config.gui.onCompletionHistory);
    outGui["OnCompletionHistory"].attribute("MaxSize", config.gui.onCompletionHistoryMax);

    //external applications
    outGui["ExternalApplications"](config.gui.externelApplications);

    //last update check
    outGui["LastOnlineCheck"  ](config.gui.lastUpdateCheck);
    outGui["LastOnlineVersion"](config.gui.lastOnlineVersion);

    //batch specific global settings
    //XmlOut outBatch = out["Batch"];
}


template <class ConfigType>
void writeConfig(const ConfigType& config, XmlType type, int xmlFormatVer, const Zstring& filepath)
{
    XmlDoc doc("FreeFileSync");
    setXmlType(doc, type); //throw()

    doc.root().setAttribute("XmlFormat", xmlFormatVer);

    XmlOut out(doc);
    writeConfig(config, out);

    saveXmlDocument(doc, filepath); //throw FileError
}
}

void xmlAccess::writeConfig(const XmlGuiConfig& cfg, const Zstring& filepath)
{
    ::writeConfig(cfg, XML_TYPE_GUI, XML_FORMAT_VER_FFS_GUI, filepath); //throw FileError
}


void xmlAccess::writeConfig(const XmlBatchConfig& cfg, const Zstring& filepath)
{
    ::writeConfig(cfg, XML_TYPE_BATCH, XML_FORMAT_VER_FFS_BATCH, filepath); //throw FileError
}


void xmlAccess::writeConfig(const XmlGlobalSettings& cfg, const Zstring& filepath)
{
    ::writeConfig(cfg, XML_TYPE_GLOBAL, XML_FORMAT_VER_GLOBAL, filepath); //throw FileError
}


std::wstring xmlAccess::extractJobName(const Zstring& configFilename)
{
    const Zstring shortName = afterLast(configFilename, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
    const Zstring jobName   = beforeLast(shortName, Zstr('.'), IF_MISSING_RETURN_ALL);
    return utfCvrtTo<std::wstring>(jobName);
}
