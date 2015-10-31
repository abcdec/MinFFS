// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PROCESSXML_H_INCLUDED
#define PROCESSXML_H_INCLUDED

#include <zen/xml_io.h>
#include <wx/gdicmn.h>
#include "localization.h"
#include "../structures.h"
#include "../ui/column_attr.h"
#include "../ui/folder_history_types.h"

namespace xmlAccess
{
enum XmlType
{
    XML_TYPE_GUI,
    XML_TYPE_BATCH,
    XML_TYPE_GLOBAL,
    XML_TYPE_OTHER
};

XmlType getXmlType(const Zstring& filepath); //throw FileError


enum OnError
{
    ON_ERROR_IGNORE,
    ON_ERROR_POPUP,
    ON_ERROR_STOP
};

enum OnGuiError
{
    ON_GUIERROR_POPUP,
    ON_GUIERROR_IGNORE
};

typedef std::wstring Description;
typedef std::wstring Commandline;
typedef std::vector<std::pair<Description, Commandline>> ExternalApps;

//---------------------------------------------------------------------
struct XmlGuiConfig
{
    XmlGuiConfig() :
        handleError(ON_GUIERROR_POPUP),
        highlightSyncAction(true) {} //initialize values

    zen::MainConfiguration mainCfg;

    OnGuiError handleError; //reaction on error situation during synchronization
    bool highlightSyncAction;
};


inline
bool operator==(const XmlGuiConfig& lhs, const XmlGuiConfig& rhs)
{
    return lhs.mainCfg             == rhs.mainCfg           &&
           lhs.handleError         == rhs.handleError       &&
           lhs.highlightSyncAction == rhs.highlightSyncAction;
}


struct XmlBatchConfig
{
    XmlBatchConfig() :
        runMinimized(false),
        logfilesCountLimit(-1),
        handleError(ON_ERROR_POPUP) {}

    zen::MainConfiguration mainCfg;

    bool runMinimized;
    Zstring logFileDirectory;
    int logfilesCountLimit; //max logfiles; 0 := don't save logfiles; < 0 := no limit
    OnError handleError;    //reaction on error situation during synchronization
};


struct OptionalDialogs
{
    OptionalDialogs() { resetDialogs();}

    void resetDialogs();

    bool warningDependentFolders;
    bool warningFolderPairRaceCondition;
    bool warningSignificantDifference;
    bool warningNotEnoughDiskSpace;
    bool warningUnresolvedConflicts;
    bool warningDatabaseError;
    bool warningRecyclerMissing;
    bool warningInputFieldEmpty;
    bool warningDirectoryLockFailed;
    bool popupOnConfigChange;
    bool confirmSyncStart;
    bool confirmExternalCommandMassInvoke;
};


enum FileIconSize
{
    ICON_SIZE_SMALL,
    ICON_SIZE_MEDIUM,
    ICON_SIZE_LARGE
};


struct ViewFilterDefault
{
    ViewFilterDefault() : equal(false), conflict(true), excluded(true)
    {
        leftOnly = rightOnly = leftNewer = rightNewer = different = true;
        createLeft = createRight = updateLeft = updateRight = deleteLeft = deleteRight = doNothing = true;
    }
    bool equal;    //
    bool conflict; //shared
    bool excluded; //
    bool leftOnly, rightOnly, leftNewer, rightNewer, different; //category view
    bool createLeft, createRight, updateLeft, updateRight, deleteLeft, deleteRight, doNothing; //action view
};

Zstring getGlobalConfigFile();

struct XmlGlobalSettings
{
    //---------------------------------------------------------------------
    //Shared (GUI/BATCH) settings
    XmlGlobalSettings() :
        programLanguage(zen::retrieveSystemLanguage()),
        failsafeFileCopy(true),
        copyLockedFiles(false), //safer default: avoid copies of partially written files
        copyFilePermissions(false),
        automaticRetryCount(0),
        automaticRetryDelay(5),
        fileTimeTolerance(2),  //default 2s: FAT vs NTFS
        runWithBackgroundPriority(false),
        createLockFile(true),
        verifyFileCopy(false),
        lastSyncsLogFileSizeMax(100000) //maximum size for LastSyncs.log: use a human-readable number
    {}

    int programLanguage;
    bool failsafeFileCopy;
    bool copyLockedFiles;
    bool copyFilePermissions;
    size_t automaticRetryCount;
    size_t automaticRetryDelay; //unit: [sec]

    int fileTimeTolerance; //max. allowed file time deviation; < 0 means unlimited tolerance
    bool runWithBackgroundPriority;
    bool createLockFile;
    bool verifyFileCopy;   //verify copied files
    size_t lastSyncsLogFileSizeMax;

    OptionalDialogs optDialogs;

    //---------------------------------------------------------------------
    struct Gui
    {
        Gui() :
            dlgPos(wxDefaultCoord, wxDefaultCoord),
            dlgSize(wxDefaultCoord, wxDefaultCoord),
            isMaximized(false),
            sashOffset(0),
            maxFolderPairsVisible(6),
            columnAttribNavi (zen::getDefaultColumnAttributesNavi()),
            columnAttribLeft (zen::getDefaultColumnAttributesLeft()),
            columnAttribRight(zen::getDefaultColumnAttributesRight()),
            naviLastSortColumn(zen::defaultValueLastSortColumn),
            naviLastSortAscending(zen::defaultValueLastSortAscending),
            showPercentBar(zen::defaultValueShowPercentage),
            cfgFileHistMax(30),
            folderHistMax(15),
            onCompletionHistoryMax(8),
#ifdef ZEN_WIN
            defaultExclusionFilter(Zstr("\\System Volume Information\\") Zstr("\n")
                                   Zstr("\\$Recycle.Bin\\")              Zstr("\n")
                                   Zstr("\\RECYCLER\\")                  Zstr("\n")
                                   Zstr("\\RECYCLED\\")                  Zstr("\n")
                                   Zstr("*\\desktop.ini")                Zstr("\n")
                                   Zstr("*\\thumbs.db")),
#elif defined ZEN_LINUX
            defaultExclusionFilter(Zstr("/.Trash-*/") Zstr("\n")
                                   Zstr("/.recycle/")),
#elif defined ZEN_MAC
            defaultExclusionFilter(Zstr("/.fseventsd/")      Zstr("\n")
                                   Zstr("/.Spotlight-V100/") Zstr("\n")
                                   Zstr("/.Trashes/")        Zstr("\n")
                                   Zstr("*/.DS_Store")       Zstr("\n")
                                   Zstr("*/._.*")),
#endif
            //deleteOnBothSides(false),
            useRecyclerForManualDeletion(true), //enable if OS supports it; else user will have to activate first and then get an error message
#if defined ZEN_WIN || defined ZEN_MAC
            textSearchRespectCase(false),
#elif defined ZEN_LINUX
            textSearchRespectCase(true),
#endif
            showIcons(true),
            iconSize(ICON_SIZE_SMALL),
            lastUpdateCheck(0)
        {
            //default external apps will be translated "on the fly"!!! First entry will be used for [Enter] or mouse double-click!
#ifdef ZEN_WIN
            externelApplications.emplace_back(L"Show in Explorer",              L"explorer /select, \"%item_path%\"");
            externelApplications.emplace_back(L"Open with default application", L"\"%item_path%\"");
            //mark for extraction: _("Show in Explorer")
            //mark for extraction: _("Open with default application")
#elif defined ZEN_LINUX
            externelApplications.emplace_back(L"Browse directory",              L"xdg-open \"%item_folder%\"");
            externelApplications.emplace_back(L"Open with default application", L"xdg-open \"%item_path%\"");
            //mark for extraction: _("Browse directory") Linux doesn't use the term "folder"
#elif defined ZEN_MAC
            externelApplications.emplace_back(L"Browse directory",              L"open -R \"%item_path%\"");
            externelApplications.emplace_back(L"Open with default application", L"open \"%item_path%\"");
#endif
        }

        wxPoint dlgPos;
        wxSize dlgSize;
        bool isMaximized;
        int sashOffset;

        int maxFolderPairsVisible;

        std::vector<zen::ColumnAttributeNavi> columnAttribNavi; //compressed view/navigation
        std::vector<zen::ColumnAttributeRim>  columnAttribLeft;
        std::vector<zen::ColumnAttributeRim>  columnAttribRight;

        zen::ColumnTypeNavi naviLastSortColumn; //remember sort on navigation panel
        bool naviLastSortAscending; //

        bool showPercentBar; //in navigation panel

        ExternalApps externelApplications;

        std::vector<zen::ConfigHistoryItem> cfgFileHistory;
        size_t cfgFileHistMax;

        std::vector<Zstring> lastUsedConfigFiles;

        std::vector<Zstring> folderHistoryLeft;
        std::vector<Zstring> folderHistoryRight;
        size_t folderHistMax;

        std::vector<Zstring> onCompletionHistory;
        size_t onCompletionHistoryMax;

        Zstring defaultExclusionFilter;

        //bool deleteOnBothSides;
        bool useRecyclerForManualDeletion;
        bool textSearchRespectCase;

        bool showIcons;
        FileIconSize iconSize;

        long lastUpdateCheck; //time of last update check
		wxString lastOnlineVersion;

        ViewFilterDefault viewFilterDefault;
        wxString guiPerspectiveLast; //used by wxAuiManager
    } gui;

    //---------------------------------------------------------------------
    //struct Batch
};

//read/write specific config types
void readConfig(const Zstring& filepath, XmlGuiConfig&      config, std::wstring& warningMsg); //
void readConfig(const Zstring& filepath, XmlBatchConfig&    config, std::wstring& warningMsg); //throw FileError
void readConfig(const Zstring& filepath, XmlGlobalSettings& config, std::wstring& warningMsg); //

void writeConfig(const XmlGuiConfig&      config, const Zstring& filepath); //
void writeConfig(const XmlBatchConfig&    config, const Zstring& filepath); //throw FileError
void writeConfig(const XmlGlobalSettings& config, const Zstring& filepath); //

//convert (multiple) *.ffs_gui, *.ffs_batch files or combinations of both into target config structure:
void readAnyConfig(const std::vector<Zstring>& filepaths, XmlGuiConfig& config, std::wstring& warningMsg); //throw FileError

//config conversion utilities
XmlGuiConfig   convertBatchToGui(const XmlBatchConfig& batchCfg); //noexcept
XmlBatchConfig convertGuiToBatch(const XmlGuiConfig&   guiCfg, const XmlBatchConfig* referenceBatchCfg); //

std::wstring extractJobName(const Zstring& configFilename);
}

#endif // PROCESSXML_H_INCLUDED
