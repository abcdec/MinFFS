// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef MAINDIALOG_H_891048132454564
#define MAINDIALOG_H_891048132454564

#include <map>
#include <list>
#include <stack>
#include <memory>
#include <wx+/async_task.h>
#include <wx+/file_drop.h>
#include <wx/aui/aui.h>
#include "gui_generated.h"
#include "custom_grid.h"
#include "sync_cfg.h"
#include "tree_view.h"
#include "folder_history_box.h"
#include "../lib/process_xml.h"

class FolderPairFirst;
class FolderPairPanel;
class CompareProgressDialog;


class MainDialog : public MainDialogGenerated
{
public:
    //default behavior, application start, restores last used config
    static void create(const Zstring& globalConfigFile);

    //when loading dynamically assembled config,
    //when switching language,
    //or switching from batch run to GUI on warnings
    static void create(const Zstring& globalConfigFile,
                       const xmlAccess::XmlGlobalSettings* globalSettings, //optional: take over ownership => save on exit
                       const xmlAccess::XmlGuiConfig& guiCfg,
                       const std::vector<Zstring>& referenceFiles,
                       bool startComparison);

    void disableAllElements(bool enableAbort); //dis-/enables all elements (except abort button) that might receive user input
    void enableAllElements();                  //during long-running processes: comparison, deletion

    void onQueryEndSession(); //last chance to do something useful before killing the application!

private:
    MainDialog(const Zstring& globalConfigFile,
               const xmlAccess::XmlGuiConfig& guiCfg,
               const std::vector<Zstring>& referenceFiles,
               const xmlAccess::XmlGlobalSettings& globalSettings, //take over ownership => save on exit
               bool startComparison);
    ~MainDialog();

    friend class StatusHandlerTemporaryPanel;
    friend class StatusHandlerFloatingDialog;
    friend class ManualDeletionHandler;
    friend class FolderPairFirst;
    friend class FolderPairPanel;
    friend class FolderSelectorImpl;
    template <class GuiPanel>
    friend class FolderPairCallback;
    friend class PanelMoveWindow;

    //configuration load/save
    void setLastUsedConfig(const Zstring& filepath, const xmlAccess::XmlGuiConfig& guiConfig);
    void setLastUsedConfig(const std::vector<Zstring>& filepaths, const xmlAccess::XmlGuiConfig& guiConfig);

    xmlAccess::XmlGuiConfig getConfig() const;
    void setConfig(const xmlAccess::XmlGuiConfig& newGuiCfg, const std::vector<Zstring>& referenceFiles);

    void setGlobalCfgOnInit(const xmlAccess::XmlGlobalSettings& globalSettings); //messes with Maximize(), window sizes, so call just once!
    xmlAccess::XmlGlobalSettings getGlobalCfgBeforeExit(); //destructive "get" thanks to "Iconize(false), Maximize(false)"

    bool loadConfiguration(const std::vector<Zstring>& filepaths); //return true if loaded successfully

    bool trySaveConfig     (const Zstring* guiFilename); //return true if saved successfully
    bool trySaveBatchConfig(const Zstring* batchFileToUpdate); //
    bool saveOldConfig(); //return false on user abort

    static const Zstring& lastRunConfigName();

    void updateGlobalFilterButton();

    void initViewFilterButtons();
    void setViewFilterDefault();

    void addFileToCfgHistory(const std::vector<Zstring>& filepaths); //= update/insert + apply selection
    void removeObsoleteCfgHistoryItems(const std::vector<Zstring>& filepaths);
    void removeCfgHistoryItems(const std::vector<Zstring>& filepaths);

    void insertAddFolderPair(const std::vector<zen::FolderPairEnh>& newPairs, size_t pos);
    void moveAddFolderPairUp(size_t pos);
    void removeAddFolderPair(size_t pos);
    void setAddFolderPairs(const std::vector<zen::FolderPairEnh>& newPairs);

    void updateGuiForFolderPair(); //helper method: add usability by showing/hiding buttons related to folder pairs

    //main method for putting gridDataView on UI: updates data respecting current view settings
    void updateGui(); //kitchen-sink update
    void updateGuiDelayedIf(bool condition); // 400 ms delay

    void updateGridViewData();     //
    void updateStatistics();       // more fine-grained updaters
    void updateUnsavedCfgStatus(); //
    void updateTopButtonImages();  //

    //context menu functions
    std::vector<zen::FileSystemObject*> getGridSelection(bool fromLeft = true, bool fromRight = true) const;
    std::vector<zen::FileSystemObject*> getTreeSelection() const;

    void setSyncDirManually(const std::vector<zen::FileSystemObject*>& selection, zen::SyncDirection direction);
    void setFilterManually(const std::vector<zen::FileSystemObject*>& selection, bool setIncluded);
    void copySelectionToClipboard(const std::vector<const zen::Grid*>& gridRefs);

    void copyToAlternateFolder(const std::vector<zen::FileSystemObject*>& selectionLeft,
                               const std::vector<zen::FileSystemObject*>& selectionRight);

    void deleteSelectedFiles(const std::vector<zen::FileSystemObject*>& selectionLeft,
                             const std::vector<zen::FileSystemObject*>& selectionRight);

    void openExternalApplication(const wxString& commandline, const std::vector<zen::FileSystemObject*>& selection, bool leftSide); //selection may be empty

    //status bar supports one of the following two states at a time:
    void setStatusBarFileStatistics(size_t filesOnLeftView, size_t foldersOnLeftView, size_t filesOnRightView, size_t foldersOnRightView, std::uint64_t filesizeLeftView, std::uint64_t filesizeRightView);
    //void setStatusBarFullText(const wxString& msg);

    void flashStatusInformation(const wxString& msg); //temporarily show different status (only valid for setStatusBarFileStatistics)
    void restoreStatusInformation();                  //called automatically after a few seconds

    //events
    void onGridButtonEventL(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainL,  true); }
    void onGridButtonEventC(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainC,  true); }
    void onGridButtonEventR(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainR, false); }
    void onGridButtonEvent (wxKeyEvent& event, zen::Grid& grid, bool leftSide);

    void onTreeButtonEvent (wxKeyEvent& event);
    void OnContextSetLayout(wxMouseEvent& event);
    void onLocalKeyEvent   (wxKeyEvent& event);

    void OnCompSettingsContext (wxMouseEvent& event) override;
    void OnSyncSettingsContext (wxMouseEvent& event) override;
    void OnGlobalFilterContext (wxMouseEvent& event) override;
    void OnViewButtonRightClick(wxMouseEvent& event) override;

    void applyCompareConfig(bool setDefaultViewType);

    //context menu handler methods
    void onMainGridContextL(zen::GridClickEvent& event);
    void onMainGridContextC(zen::GridClickEvent& event);
    void onMainGridContextR(zen::GridClickEvent& event);
    void onMainGridContextRim(bool leftSide);

    void onNaviGridContext(zen::GridClickEvent& event);

    void onNaviSelection(zen::GridRangeSelectEvent& event);

    void onNaviPanelFilesDropped(zen::FileDropEvent& event);

    void onDirSelected(wxCommandEvent& event);
    void onDirManualCorrection(wxCommandEvent& event);

    void onCheckRows       (zen::CheckRowsEvent&     event);
    void onSetSyncDirection(zen::SyncDirectionEvent& event);

    void onGridDoubleClickL(zen::GridClickEvent& event);
    void onGridDoubleClickR(zen::GridClickEvent& event);
    void onGridDoubleClickRim(size_t row, bool leftSide);

    void onGridLabelLeftClickC(zen::GridClickEvent& event);
    void onGridLabelLeftClickL(zen::GridClickEvent& event);
    void onGridLabelLeftClickR(zen::GridClickEvent& event);
    void onGridLabelLeftClick(bool onLeft, zen::ColumnTypeRim type);

    void onGridLabelContextL(zen::GridClickEvent& event);
    void onGridLabelContextC(zen::GridClickEvent& event);
    void onGridLabelContextR(zen::GridClickEvent& event);
    void onGridLabelContext(zen::Grid& grid, zen::ColumnTypeRim type, const std::vector<zen::ColumnAttributeRim>& defaultColumnAttributes);

    void OnToggleViewType  (wxCommandEvent& event) override;
    void OnToggleViewButton(wxCommandEvent& event) override;

    void OnConfigNew      (wxCommandEvent& event) override;
    void OnConfigSave     (wxCommandEvent& event) override;
    void OnConfigSaveAs   (wxCommandEvent& event) override;
    void OnSaveAsBatchJob (wxCommandEvent& event) override;
    void OnConfigLoad     (wxCommandEvent& event) override;
    void OnLoadFromHistory(wxCommandEvent& event) override;
    void OnLoadFromHistoryDoubleClick(wxCommandEvent& event) override;

    void deleteSelectedCfgHistoryItems();

    void OnCfgHistoryRightClick(wxMouseEvent& event) override;
    void OnCfgHistoryKeyEvent  (wxKeyEvent&   event) override;
    void OnRegularUpdateCheck  (wxIdleEvent&  event);
    void OnLayoutWindowAsync   (wxIdleEvent&  event);

    void OnResizeLeftFolderWidth(wxEvent& event);
    void OnResizeTopButtonPanel (wxEvent& event);
    void OnResizeConfigPanel    (wxEvent& event);
    void OnResizeViewPanel      (wxEvent& event);
    void OnCompare              (wxCommandEvent& event) override;
    void OnStartSync            (wxCommandEvent& event) override;
    void OnSwapSides            (wxCommandEvent& event) override;
    void OnClose                (wxCloseEvent&   event) override;

    void OnCmpSettings    (wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::COMPARISON, -1); }
    void OnConfigureFilter(wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::FILTER    , -1); }
    void OnSyncSettings   (wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::SYNC      , -1); }

    void showConfigDialog(zen::SyncConfigPanel panelToShow, int localPairIndexToShow);

    void filterExtension(const Zstring& extension, bool include);
    void filterShortname(const zen::FileSystemObject& fsObj, bool include);
    void filterItems(const std::vector<zen::FileSystemObject*>& selection, bool include);
    void filterPhrase(const Zstring& phrase, bool include, bool addNewLine);

    void OnTopFolderPairAdd   (wxCommandEvent& event) override;
    void OnTopFolderPairRemove(wxCommandEvent& event) override;
    void OnRemoveFolderPair   (wxCommandEvent& event);
    void OnShowFolderPairOptions(wxCommandEvent& event);

    void OnTopLocalCompCfg  (wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::COMPARISON, 0); }
    void OnTopLocalSyncCfg  (wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::SYNC,       0); }
    void OnTopLocalFilterCfg(wxCommandEvent& event) override { showConfigDialog(zen::SyncConfigPanel::FILTER,     0); }

    void OnLocalCompCfg  (wxCommandEvent& event);
    void OnLocalSyncCfg  (wxCommandEvent& event);
    void OnLocalFilterCfg(wxCommandEvent& event);

    void onTopFolderPairKeyEvent(wxKeyEvent& event);
    void onAddFolderPairKeyEvent(wxKeyEvent& event);

    void applyFilterConfig();
    void applySyncConfig();

    void showFindPanel(); //CTRL + F
    void hideFindPanel();
    void startFindNext(); //F3

    void resetLayout();

    void OnSearchGridEnter(wxCommandEvent& event) override;
    void OnHideSearchPanel(wxCommandEvent& event) override;
    void OnSearchPanelKeyPressed(wxKeyEvent& event);

    //menu events
    void OnMenuOptions       (wxCommandEvent& event) override;
    void OnMenuExportFileList(wxCommandEvent& event) override;
    void OnMenuResetLayout   (wxCommandEvent& event) override { resetLayout(); }
    void OnMenuFindItem      (wxCommandEvent& event) override;
    void OnMenuCheckVersion  (wxCommandEvent& event) override;
    void OnMenuCheckVersionAutomatically(wxCommandEvent& event) override;
    void OnMenuDownloadNewVersion       (wxCommandEvent& event);
    void OnMenuAbout         (wxCommandEvent& event) override;
    void OnShowHelp          (wxCommandEvent& event) override;
    void OnMenuQuit          (wxCommandEvent& event) override { Close(); }

    void OnMenuLanguageSwitch(wxCommandEvent& event);

    void switchProgramLanguage(int langID);

    void clearGrid(ptrdiff_t pos = -1);

    typedef int MenuItemID;
    typedef int LanguageID;
    std::map<MenuItemID, LanguageID> languageMenuItemMap; //needed to attach menu item events

    //***********************************************
    //application variables are stored here:

    //global settings shared by GUI and batch mode
    xmlAccess::XmlGlobalSettings globalCfg;

    const Zstring globalConfigFile_;

    //-------------------------------------
    //program configuration
    xmlAccess::XmlGuiConfig currentCfg;

    //used when saving configuration
    std::vector<Zstring> activeConfigFiles; //name of currently loaded config file (may be more than 1)

    xmlAccess::XmlGuiConfig lastConfigurationSaved; //support for: "Save changed configuration?" dialog
    //-------------------------------------

    //UI view of FolderComparison structure (partially owns folderCmp)
    std::shared_ptr<zen::GridView> gridDataView; //always bound!
    std::shared_ptr<zen::TreeView> treeDataView; //

    //the prime data structure of this tool *bling*:
    zen::FolderComparison folderCmp; //optional!: sync button not available if empty

    //folder pairs:
    std::unique_ptr<FolderPairFirst> firstFolderPair; //always bound!!!
    std::vector<FolderPairPanel*> additionalFolderPairs; //additional pairs to the first pair
    //-------------------------------------

    //***********************************************
    //status information
    std::list<wxString> oldStatusMsgs; //the first one is the original/non-flash status message

    //compare status panel (hidden on start, shown when comparing)
    std::unique_ptr<CompareProgressDialog> compareStatus; //always bound

    bool cleanedUp;

    //toggle to display configuration preview instead of comparison result:
    //for read access use: m_bpButtonViewTypeSyncAction->isActive()
    //when changing value use:
    void setViewTypeSyncAction(bool value);

    wxAuiManager auiMgr; //implement dockable GUI design

    wxString defaultPerspective;

    std::int64_t manualTimeSpanFrom = 0;
    std::int64_t manualTimeSpanTo   = 0; //buffer manual time span selection at session level

    std::shared_ptr<FolderHistory> folderHistoryLeft;  //shared by all wxComboBox dropdown controls
    std::shared_ptr<FolderHistory> folderHistoryRight; //always bound!

    zen::AsyncGuiQueue guiQueue; //schedule and run long-running tasks asynchronously, but process results on GUI queue

    std::unique_ptr<zen::FilterConfig> filterCfgOnClipboard; //copy/paste of filter config

    wxWindow* focusWindowAfterSearch = nullptr; //used to restore focus after search panel is closed

    bool localKeyEventsEnabled = true;
};

#endif //MAINDIALOG_H_891048132454564
