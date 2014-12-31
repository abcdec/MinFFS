// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "sync_cfg.h"
#include <memory>
#include <wx/wupdlock.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/choice_enum.h>
#include <wx+/image_tools.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
#include "on_completion_box.h"
#include "dir_name.h"
#include "../file_hierarchy.h"
#include "../lib/help_provider.h"
#include "../lib/norm_filter.h"

#ifdef ZEN_WIN
    #include <wx+/mouse_move_dlg.h>
#endif

using namespace zen;
using namespace xmlAccess;


namespace
{
class ConfigDialog : public ConfigDlgGenerated
{
public:
    ConfigDialog(wxWindow* parent,
                 SyncConfigPanel panelToShow,
                 bool* useAlternateCmpCfg,  //optional parameter
                 CompConfig&   cmpCfg,
                 FilterConfig& filterCfg,
                 bool* useAlternateSyncCfg,  //
                 SyncConfig&   syncCfg,
                 CompareVariant globalCmpVar,
                 MiscGlobalCfg* miscCfg, //
                 const wxString& title);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSyncConfig::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSyncConfig::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    enum class ConfigTypeImage
    {
        COMPARISON = 0, //used as zero-based wxImageList index!
        COMPARISON_GREY,
        FILTER,
        FILTER_GREY,
        SYNC,
        SYNC_GREY,
    };

    //------------- comparison panel ----------------------
    void OnHelpComparisonSettings(wxHyperlinkEvent& event) override { displayHelpEntry(L"html/Comparison Settings.html" , this); }
    void OnHelpTimeShift         (wxHyperlinkEvent& event) override { displayHelpEntry(L"html/Daylight Saving Time.html", this); }

    void OnToggleLocalCompSettings(wxCommandEvent& event) override { updateCompGui(); updateSyncGui(); /*affects sync settings, too!*/ }
    void OnTimeSize               (wxCommandEvent& event) override { localCmpVar = CMP_BY_TIME_SIZE; updateCompGui(); updateSyncGui(); /*affects sync settings, too!*/ }
    void OnContent                (wxCommandEvent& event) override { localCmpVar = CMP_BY_CONTENT;   updateCompGui(); updateSyncGui(); /*affects sync settings, too!*/ }
    void OnTimeSizeDouble         (wxMouseEvent&   event) override;
    void OnContentDouble          (wxMouseEvent&   event) override;
    void OnChangeCompOption       (wxCommandEvent& event) override { updateCompGui(); }

    void updateCompGui();

    CompConfig& cmpCfgOut; //for output only
    bool* useAlternateCmpCfgOptOut;
    CompareVariant localCmpVar;

    //------------- filter panel --------------------------
    void OnHelpShowExamples(wxHyperlinkEvent& event) override { displayHelpEntry(L"html/Exclude Items.html", this); }
    void OnChangeFilterOption(wxCommandEvent& event) override { updateFilterGui(); }
    void OnFilterReset       (wxCommandEvent& event) override { setFilter(FilterConfig()); }

    void onFilterKeyEvent(wxKeyEvent& event);
    void setFilter(const FilterConfig& filter);
    FilterConfig getFilter() const;

    void updateFilterGui();

    FilterConfig& filterCfgOut;
    EnumDescrList<UnitTime> enumTimeDescr;
    EnumDescrList<UnitSize> enumSizeDescr;

    //------------- synchronization panel -----------------
    void OnSyncTwoWay(wxCommandEvent& event) override { directionCfg.var = DirectionConfig::TWOWAY; updateSyncGui(); }
    void OnSyncMirror(wxCommandEvent& event) override { directionCfg.var = DirectionConfig::MIRROR; updateSyncGui(); }
    void OnSyncUpdate(wxCommandEvent& event) override { directionCfg.var = DirectionConfig::UPDATE; updateSyncGui(); }
    void OnSyncCustom(wxCommandEvent& event) override { directionCfg.var = DirectionConfig::CUSTOM; updateSyncGui(); }

    void OnToggleLocalSyncSettings(wxCommandEvent& event) override { updateSyncGui(); }
    void OnToggleDetectMovedFiles (wxCommandEvent& event) override { directionCfg.detectMovedFiles = !directionCfg.detectMovedFiles; updateSyncGui(); }
    void OnChangeSyncOption       (wxCommandEvent& event) override { updateSyncGui(); }

    void OnSyncTwoWayDouble(wxMouseEvent& event) override;
    void OnSyncMirrorDouble(wxMouseEvent& event) override;
    void OnSyncUpdateDouble(wxMouseEvent& event) override;
    void OnSyncCustomDouble(wxMouseEvent& event) override;

    void OnExLeftSideOnly (wxCommandEvent& event) override;
    void OnExRightSideOnly(wxCommandEvent& event) override;
    void OnLeftNewer      (wxCommandEvent& event) override;
    void OnRightNewer     (wxCommandEvent& event) override;
    void OnDifferent      (wxCommandEvent& event) override;
    void OnConflict       (wxCommandEvent& event) override;

    void OnDeletionPermanent  (wxCommandEvent& event) override { handleDeletion = DELETE_PERMANENTLY;   updateSyncGui(); }
    void OnDeletionRecycler   (wxCommandEvent& event) override { handleDeletion = DELETE_TO_RECYCLER;   updateSyncGui(); }
    void OnDeletionVersioning (wxCommandEvent& event) override { handleDeletion = DELETE_TO_VERSIONING; updateSyncGui(); }

    void OnErrorPopup (wxCommandEvent& event) override { onGuiError = ON_GUIERROR_POPUP;  updateSyncGui(); }
    void OnErrorIgnore(wxCommandEvent& event) override { onGuiError = ON_GUIERROR_IGNORE; updateSyncGui(); }

    void OnHelpVersioning(wxHyperlinkEvent& event) override { displayHelpEntry(L"html/Versioning.html", this); }

    struct SyncOptions
    {
        SyncConfig syncCfg;
        xmlAccess::OnGuiError onGuiError;
        Zstring onCompletion;
    };

    void setSyncOptions(const SyncOptions& so);
    SyncOptions getSyncOptions() const;

    void updateSyncGui();

    const CompareVariant globalCmpVar_;
    SyncConfig& syncCfgOut;
    bool* useAlternateSyncCfgOptOut;
    MiscGlobalCfg* miscCfgOut;

    //parameters with ownership NOT within GUI controls!
    DirectionConfig directionCfg;
    DeletionPolicy handleDeletion; //use Recycler, delete permanently or move to user-defined location
    OnGuiError onGuiError;

    EnumDescrList<VersioningStyle> enumVersioningStyle;
    DirectoryName<FolderHistoryBox> versioningFolder;
};

//#################################################################################################################

std::wstring getCompVariantDescription(CompareVariant var)
{
    switch (var)
    {
        case CMP_BY_TIME_SIZE:
            return _("Identify equal files by comparing modification time and size.");
        case CMP_BY_CONTENT:
            return _("Identify equal files by comparing the file content.");
    }
    assert(false);
    return _("Error");
}


std::wstring getSyncVariantDescription(DirectionConfig::Variant var)
{
    switch (var)
    {
        case DirectionConfig::TWOWAY:
            return _("Identify and propagate changes on both sides. Deletions, moves and conflicts are detected automatically using a database.");
        case DirectionConfig::MIRROR:
            return _("Create a mirror backup of the left folder by adapting the right folder to match.");
        case DirectionConfig::UPDATE:
            return _("Copy new and updated files to the right folder.");
        case DirectionConfig::CUSTOM:
            return _("Configure your own synchronization rules.");
    }
    assert(false);
    return _("Error");
}


ConfigDialog::ConfigDialog(wxWindow* parent,
                           SyncConfigPanel panelToShow,
                           bool* useAlternateCmpCfg,  //optional parameter
                           CompConfig&   cmpCfg,
                           FilterConfig& filterCfg,
                           bool* useAlternateSyncCfg,  //
                           SyncConfig&   syncCfg,
                           CompareVariant globalCmpVar,
                           MiscGlobalCfg* miscCfg, //
                           const wxString& title) :
    ConfigDlgGenerated(parent),
    cmpCfgOut(cmpCfg),
    useAlternateCmpCfgOptOut(useAlternateCmpCfg),
    localCmpVar(cmpCfg.compareVar),
    filterCfgOut(filterCfg),
    globalCmpVar_(globalCmpVar),
    syncCfgOut(syncCfg),
    useAlternateSyncCfgOptOut(useAlternateSyncCfg),
    miscCfgOut(miscCfg),
    handleDeletion(DELETE_TO_RECYCLER), //
    onGuiError(ON_GUIERROR_POPUP),      //dummy init
    versioningFolder(*m_panelVersioning, *m_buttonSelectDirVersioning, *m_versioningFolder/*, m_staticTextResolvedPath*/)
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //allow moving main dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif//TODO_MinFFS_MouseMoveWindow
#endif
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    SetTitle(title);

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imageListSize = getResourceImage(L"cfg_compare_small").GetHeight();
    assert(imageListSize == 16); //Windows default size for panel caption
    auto imgList = make_unique<wxImageList>(imageListSize, imageListSize);

    auto addToImageList = [&](const wxBitmap& bmp)
    {
        assert(bmp.GetWidth () <= imageListSize);
        assert(bmp.GetHeight() <= imageListSize);
        imgList->Add(bmp);
        imgList->Add(greyScale(bmp));
    };
    //make sure to add images in same sequence as ConfigTypeImage enum!!!
    addToImageList(getResourceImage(L"cfg_compare_small"));
    addToImageList(getResourceImage(L"filter_small"     ));
    addToImageList(getResourceImage(L"cfg_sync_small"   ));
    assert(imgList->GetImageCount() == static_cast<int>(ConfigTypeImage::SYNC_GREY) + 1);

    m_notebook->AssignImageList(imgList.release()); //notebook takes ownership

    //------------- comparison panel ----------------------
    setRelativeFontSize(*m_toggleBtnTimeSize, 1.25);
    setRelativeFontSize(*m_toggleBtnContent,  1.25);

    m_toggleBtnTimeSize->SetToolTip(getCompVariantDescription(CMP_BY_TIME_SIZE));
    m_toggleBtnContent ->SetToolTip(getCompVariantDescription(CMP_BY_CONTENT));

    switch (cmpCfg.handleSymlinks)
    {
        case SYMLINK_EXCLUDE:
            m_checkBoxSymlinksInclude->SetValue(false);
            break;
        case SYMLINK_DIRECT:
            m_checkBoxSymlinksInclude->SetValue(true);
            m_radioBtnSymlinksDirect->SetValue(true);
            break;
        case SYMLINK_FOLLOW:
            m_checkBoxSymlinksInclude->SetValue(true);
            m_radioBtnSymlinksFollow->SetValue(true);
            break;
    }

    m_checkBoxTimeShift->SetValue(cmpCfg.optTimeShiftHours != 0);
    m_spinCtrlTimeShift->SetValue(cmpCfg.optTimeShiftHours == 0 ? 1 : cmpCfg.optTimeShiftHours);

    if (useAlternateCmpCfg)
        m_checkBoxUseLocalCmpOptions->SetValue(*useAlternateCmpCfg);
    else
    {
        m_checkBoxUseLocalCmpOptions->SetValue(true);
        bSizerLocalCompSettings->Show(false);
        m_panelCompSettingsHolder->Layout(); //fix comp panel glitch on Win 7 125% font size
    }
    updateCompGui();

    //------------- filter panel --------------------------

#ifndef __WXGTK__  //wxWidgets breaks portability promise once again
    m_textCtrlInclude->SetMaxLength(0); //allow large filter entries!
    m_textCtrlExclude->SetMaxLength(0); //
#endif
    assert(!contains(m_buttonClear->GetLabel(), L"&C") && !contains(m_buttonClear->GetLabel(), L"&c")); //gazillionth wxWidgets bug on OS X: Command + C mistakenly hits "&C" access key!

    m_textCtrlInclude->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(ConfigDialog::onFilterKeyEvent), nullptr, this);
    m_textCtrlExclude->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(ConfigDialog::onFilterKeyEvent), nullptr, this);

    enumTimeDescr.
    add(UTIME_NONE, L"(" + _("None") + L")"). //meta options should be enclosed in parentheses
    add(UTIME_TODAY,       _("Today")).
    //    add(UTIME_THIS_WEEK,   _("This week")).
    add(UTIME_THIS_MONTH,  _("This month")).
    add(UTIME_THIS_YEAR,   _("This year")).
    add(UTIME_LAST_X_DAYS, _("Last x days"));

    enumSizeDescr.
    add(USIZE_NONE, L"(" + _("None") + L")"). //meta options should be enclosed in parentheses
    add(USIZE_BYTE, _("Byte")).
    add(USIZE_KB,   _("KB")).
    add(USIZE_MB,   _("MB"));

    assert((useAlternateCmpCfg != nullptr) == (useAlternateSyncCfg != nullptr));
    if (!useAlternateCmpCfg)
    {
        bSizerLocalFilterSettings->Show(false);
        m_panelFilterSettingsHolder->Layout();
    }

    setFilter(filterCfg);

    //------------- synchronization panel -----------------
    m_toggleBtnTwoWay->SetLabel(getVariantName(DirectionConfig::TWOWAY));
    m_toggleBtnMirror->SetLabel(getVariantName(DirectionConfig::MIRROR));
    m_toggleBtnUpdate->SetLabel(getVariantName(DirectionConfig::UPDATE));
    m_toggleBtnCustom->SetLabel(getVariantName(DirectionConfig::CUSTOM));

    m_toggleBtnTwoWay->SetToolTip(getSyncVariantDescription(DirectionConfig::TWOWAY));
    m_toggleBtnMirror->SetToolTip(getSyncVariantDescription(DirectionConfig::MIRROR));
    m_toggleBtnUpdate->SetToolTip(getSyncVariantDescription(DirectionConfig::UPDATE));
    m_toggleBtnCustom->SetToolTip(getSyncVariantDescription(DirectionConfig::CUSTOM));

    m_bitmapLeftOnly  ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_left_only"  ))));
    m_bitmapRightOnly ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_right_only" ))));
    m_bitmapLeftNewer ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_left_newer" ))));
    m_bitmapRightNewer->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_right_newer"))));
    m_bitmapDifferent ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_different"  ))));
    m_bitmapConflict  ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_conflict"   ))));

    setRelativeFontSize(*m_toggleBtnTwoWay, 1.25);
    setRelativeFontSize(*m_toggleBtnMirror, 1.25);
    setRelativeFontSize(*m_toggleBtnUpdate, 1.25);
    setRelativeFontSize(*m_toggleBtnCustom, 1.25);

    enumVersioningStyle.
    add(VER_STYLE_REPLACE,       _("Replace"),    _("Move files and replace if existing")).
    add(VER_STYLE_ADD_TIMESTAMP, _("Time stamp"), _("Append a time stamp to each file name"));

    //use spacer to keep dialog height stable, no matter if versioning options are visible
    bSizerVersioning->Add(0, m_panelVersioning->GetSize().GetHeight());

    if (useAlternateSyncCfg)
        m_checkBoxUseLocalSyncOptions->SetValue(*useAlternateSyncCfg);
    else
    {
        m_checkBoxUseLocalSyncOptions->SetValue(true);
        bSizerLocalSyncSettings->Show(false);
        m_panelSyncSettingsHolder->Layout();
    }

    if (miscCfg)
        m_comboBoxOnCompletion->initHistory(miscCfg->onCompletionHistory, miscCfg->onCompletionHistoryMax);
    else //hide controls for optional parameters
    {
        bSizerMiscConfig->Show(false);
        Layout();
    }

    const SyncOptions so = { syncCfg,
                             miscCfg ? miscCfg->handleError : ON_GUIERROR_POPUP,
                             miscCfg ? miscCfg->onCompletionCommand : Zstring()
                           };
    setSyncOptions(so);
    //-----------------------------------------------------

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(ConfigDialog::onLocalKeyEvent), nullptr, this);

    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::COMPARISON), _("Comparison")      + L" (F6)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::FILTER    ), _("Filter")          + L" (F7)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::SYNC      ), _("Synchronization") + L" (F8)");

    m_notebook->ChangeSelection(static_cast<size_t>(panelToShow));

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    m_buttonOkay->SetFocus();
}


void ConfigDialog::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    const int keyCode = event.GetKeyCode();

    switch (keyCode)
    {
        case WXK_F6:
            m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::COMPARISON));
            return; //handled!
        case WXK_F7:
            m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::FILTER));
            return;
        case WXK_F8:
            m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
            return;
    }

    event.Skip();
}


void ConfigDialog::OnTimeSizeDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnTimeSize(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnContentDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnContent(dummy);
    OnOkay(dummy);
}


void ConfigDialog::updateCompGui()
{
    m_panelComparisonSettings->Enable(m_checkBoxUseLocalCmpOptions->GetValue());

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::COMPARISON),
                             static_cast<int>(m_checkBoxUseLocalCmpOptions->GetValue() ? ConfigTypeImage::COMPARISON : ConfigTypeImage::COMPARISON_GREY));

    //update toggle buttons -> they have no parameter-ownership at all!
    m_toggleBtnTimeSize->SetValue(false);
    m_toggleBtnContent ->SetValue(false);

    if (m_checkBoxUseLocalCmpOptions->GetValue()) //help wxWidgets a little to render inactive config state (need on Windows, NOT on Linux!)
        switch (localCmpVar)
        {
            case CMP_BY_TIME_SIZE:
                m_toggleBtnTimeSize->SetValue(true);
                break;
            case CMP_BY_CONTENT:
                m_toggleBtnContent->SetValue(true);
                break;
        }

    auto setBitmap = [&](wxStaticBitmap& bmpCtrl, bool active, const wxBitmap& bmp)
    {
        if (active &&
            m_checkBoxUseLocalCmpOptions->GetValue()) //help wxWidgets a little to render inactive config state (need on Windows, NOT on Linux!)
            bmpCtrl.SetBitmap(bmp);
        else
            bmpCtrl.SetBitmap(greyScale(bmp));
    };
    setBitmap(*m_bitmapByTime,    localCmpVar == CMP_BY_TIME_SIZE, getResourceImage(L"clock"));
    setBitmap(*m_bitmapByContent, localCmpVar == CMP_BY_CONTENT,   getResourceImage(L"cmpByContent"));

    //active variant description:
    setText(*m_textCtrlCompVarDescription, L"\n" + getCompVariantDescription(localCmpVar));

    m_spinCtrlTimeShift->Enable(m_checkBoxTimeShift->GetValue());

    m_radioBtnSymlinksDirect->Enable(m_checkBoxSymlinksInclude->GetValue());
    m_radioBtnSymlinksFollow->Enable(m_checkBoxSymlinksInclude->GetValue());
}


void ConfigDialog::onFilterKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    if (event.ControlDown())
        switch (keyCode)
        {
            case 'A': //CTRL + A
                if (auto textCtrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject()))
                    textCtrl->SetSelection(-1, -1); //select all
                return;
        }
    event.Skip();
}


void ConfigDialog::setFilter(const FilterConfig& filter)
{
    m_textCtrlInclude->ChangeValue(utfCvrtTo<wxString>(filter.includeFilter));
    m_textCtrlExclude->ChangeValue(utfCvrtTo<wxString>(filter.excludeFilter));

    setEnumVal(enumTimeDescr, *m_choiceUnitTimespan, filter.unitTimeSpan);
    setEnumVal(enumSizeDescr, *m_choiceUnitMinSize,  filter.unitSizeMin);
    setEnumVal(enumSizeDescr, *m_choiceUnitMaxSize,  filter.unitSizeMax);

    m_spinCtrlTimespan->SetValue(static_cast<int>(filter.timeSpan));
    m_spinCtrlMinSize ->SetValue(static_cast<int>(filter.sizeMin));
    m_spinCtrlMaxSize ->SetValue(static_cast<int>(filter.sizeMax));

    updateFilterGui();
}


FilterConfig ConfigDialog::getFilter() const
{
    return FilterConfig(utfCvrtTo<Zstring>(m_textCtrlInclude->GetValue()),
                        utfCvrtTo<Zstring>(m_textCtrlExclude->GetValue()),
                        m_spinCtrlTimespan->GetValue(),
                        getEnumVal(enumTimeDescr, *m_choiceUnitTimespan),
                        m_spinCtrlMinSize->GetValue(),
                        getEnumVal(enumSizeDescr, *m_choiceUnitMinSize),
                        m_spinCtrlMaxSize->GetValue(),
                        getEnumVal(enumSizeDescr, *m_choiceUnitMaxSize));
}


void ConfigDialog::updateFilterGui()
{
    const FilterConfig activeCfg = getFilter();

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::FILTER),
                             static_cast<int>(!isNullFilter(activeCfg) ? ConfigTypeImage::FILTER: ConfigTypeImage::FILTER_GREY));

    auto setStatusBitmap = [&](wxStaticBitmap& staticBmp, const wxString& bmpName, bool active)
    {
        if (active)
            staticBmp.SetBitmap(getResourceImage(bmpName));
        else
            staticBmp.SetBitmap(greyScale(getResourceImage(bmpName)));
    };
    setStatusBitmap(*m_bitmapInclude,    L"filter_include", !NameFilter::isNull(activeCfg.includeFilter, FilterConfig().excludeFilter));
    setStatusBitmap(*m_bitmapExclude,    L"filter_exclude", !NameFilter::isNull(FilterConfig().includeFilter, activeCfg.excludeFilter));
    setStatusBitmap(*m_bitmapFilterDate, L"clock", activeCfg.unitTimeSpan != UTIME_NONE);
    setStatusBitmap(*m_bitmapFilterSize, L"size",  activeCfg.unitSizeMin  != USIZE_NONE || activeCfg.unitSizeMax != USIZE_NONE);

    m_spinCtrlTimespan->Enable(activeCfg.unitTimeSpan == UTIME_LAST_X_DAYS);
    m_spinCtrlMinSize ->Enable(activeCfg.unitSizeMin != USIZE_NONE);
    m_spinCtrlMaxSize ->Enable(activeCfg.unitSizeMax != USIZE_NONE);

    m_buttonClear->Enable(!(activeCfg == FilterConfig()));
}


void ConfigDialog::OnSyncTwoWayDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncTwoWay(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncMirrorDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncMirror(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncUpdateDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncUpdate(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncCustomDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncCustom(dummy);
    OnOkay(dummy);
}


void toggleSyncDirection(SyncDirection& current)
{
    switch (current)
    {
        case SyncDirection::RIGHT:
            current = SyncDirection::LEFT;
            break;
        case SyncDirection::LEFT:
            current = SyncDirection::NONE;
            break;
        case SyncDirection::NONE:
            current = SyncDirection::RIGHT;
            break;
    }
}


void toggleSyncConfig(DirectionConfig& directionCfg, SyncDirection& custSyncdir)
{
    switch (directionCfg.var)
    {
        case DirectionConfig::TWOWAY:
            assert(false);
            break;
        case DirectionConfig::MIRROR:
        case DirectionConfig::UPDATE:
            directionCfg.custom = extractDirections(directionCfg);
            directionCfg.var = DirectionConfig::CUSTOM;
            toggleSyncDirection(custSyncdir);
            break;
        case DirectionConfig::CUSTOM:
            toggleSyncDirection(custSyncdir);

            //some config optimization: if custom settings happen to match "mirror" or "update", just switch variant
            const DirectionSet mirrorSet = []
            {
                DirectionConfig mirrorCfg;
                mirrorCfg.var = DirectionConfig::MIRROR;
                return extractDirections(mirrorCfg);
            }();

            const DirectionSet updateSet = []
            {
                DirectionConfig updateCfg;
                updateCfg.var = DirectionConfig::UPDATE;
                return extractDirections(updateCfg);
            }();

            if (directionCfg.custom == mirrorSet)
                directionCfg.var = DirectionConfig::MIRROR;
            else if (directionCfg.custom == updateSet)
                directionCfg.var = DirectionConfig::UPDATE;
            break;
    }
}


void ConfigDialog::OnExLeftSideOnly(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.exLeftSideOnly);
    updateSyncGui();
}


void ConfigDialog::OnExRightSideOnly(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.exRightSideOnly);
    updateSyncGui();
}


void ConfigDialog::OnLeftNewer(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.leftNewer);
    updateSyncGui();
}


void ConfigDialog::OnRightNewer(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.rightNewer);
    updateSyncGui();
}


void ConfigDialog::OnDifferent(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.different);
    updateSyncGui();
}


void ConfigDialog::OnConflict(wxCommandEvent& event)
{
    toggleSyncConfig(directionCfg, directionCfg.custom.conflict);
    updateSyncGui();
}


void updateSyncDirectionIcons(const DirectionConfig& directionCfg,
                              wxBitmapButton& buttonLeftOnly,
                              wxBitmapButton& buttonRightOnly,
                              wxBitmapButton& buttonLeftNewer,
                              wxBitmapButton& buttonRightNewer,
                              wxBitmapButton& buttonDifferent,
                              wxBitmapButton& buttonConflict)
{
    if (directionCfg.var != DirectionConfig::TWOWAY) //automatic mode needs no sync-directions
    {
        auto updateButton = [](wxBitmapButton& button, SyncDirection dir,
                               const wchar_t* imgNameLeft, const wchar_t* imgNameNone, const wchar_t* imgNameRight,
                               SyncOperation opLeft, SyncOperation opNone, SyncOperation opRight)
        {
            switch (dir)
            {
                case SyncDirection::LEFT:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameLeft)));
                    button.SetToolTip(getSyncOpDescription(opLeft));
                    break;
                case SyncDirection::NONE:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameNone)));
                    button.SetToolTip(getSyncOpDescription(opNone));
                    break;
                case SyncDirection::RIGHT:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameRight)));
                    button.SetToolTip(getSyncOpDescription(opRight));
                    break;
            }
        };

        const DirectionSet dirCfg = extractDirections(directionCfg);

        updateButton(buttonLeftOnly  , dirCfg.exLeftSideOnly , L"so_delete_left", L"so_none", L"so_create_right", SO_DELETE_LEFT    , SO_DO_NOTHING, SO_CREATE_NEW_RIGHT);
        updateButton(buttonRightOnly , dirCfg.exRightSideOnly, L"so_create_left", L"so_none", L"so_delete_right", SO_CREATE_NEW_LEFT, SO_DO_NOTHING, SO_DELETE_RIGHT    );
        updateButton(buttonLeftNewer , dirCfg.leftNewer      , L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT , SO_DO_NOTHING, SO_OVERWRITE_RIGHT );
        updateButton(buttonRightNewer, dirCfg.rightNewer     , L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT , SO_DO_NOTHING, SO_OVERWRITE_RIGHT );
        updateButton(buttonDifferent , dirCfg.different      , L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT , SO_DO_NOTHING, SO_OVERWRITE_RIGHT );

        switch (dirCfg.conflict)
        {
            case SyncDirection::LEFT:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"so_update_left")));
                buttonConflict.SetToolTip(getSyncOpDescription(SO_OVERWRITE_LEFT));
                break;
            case SyncDirection::NONE:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"cat_conflict"))); //silent dependency from algorithm.cpp::Redetermine!!!
                buttonConflict.SetToolTip(_("Leave as unresolved conflict"));
                break;
            case SyncDirection::RIGHT:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"so_update_right")));
                buttonConflict.SetToolTip(getSyncOpDescription(SO_OVERWRITE_RIGHT));
                break;
        }
    }
}


void ConfigDialog::setSyncOptions(const SyncOptions& so)
{
    directionCfg   = so.syncCfg.directionCfg;  //make working copy; ownership *not* on GUI
    handleDeletion = so.syncCfg.handleDeletion;

    versioningFolder.setPath(utfCvrtTo<wxString>(so.syncCfg.versioningDirectory));
    setEnumVal(enumVersioningStyle, *m_choiceVersioningStyle, so.syncCfg.versioningStyle);

    //misc config
    onGuiError = so.onGuiError;
    m_comboBoxOnCompletion->setValue(so.onCompletion);

    updateSyncGui();
}


ConfigDialog::SyncOptions ConfigDialog::getSyncOptions() const
{
    SyncOptions output;

    output.syncCfg.directionCfg        = directionCfg;
    output.syncCfg.handleDeletion      = handleDeletion;
    output.syncCfg.versioningDirectory = utfCvrtTo<Zstring>(versioningFolder.getPath());
    output.syncCfg.versioningStyle     = getEnumVal(enumVersioningStyle, *m_choiceVersioningStyle);

    output.onGuiError = onGuiError;
    output.onCompletion = m_comboBoxOnCompletion->getValue();

    return output;
}


void ConfigDialog::updateSyncGui()
{
#ifdef ZEN_WIN
    wxWindowUpdateLocker dummy(this); //leads to GUI corruption problems on Linux/OS X!
    wxWindowUpdateLocker dummy2(m_panelVersioning);
    wxWindowUpdateLocker dummy3(m_bpButtonLeftOnly);
    wxWindowUpdateLocker dummy4(m_bpButtonRightOnly);
    wxWindowUpdateLocker dummy5(m_bpButtonLeftNewer);
    wxWindowUpdateLocker dummy6(m_bpButtonRightNewer);
    wxWindowUpdateLocker dummy7(m_bpButtonDifferent);
    wxWindowUpdateLocker dummy8(m_bpButtonConflict);
#endif

    m_panelSyncSettings->Enable(m_checkBoxUseLocalSyncOptions->GetValue());

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::SYNC),
                             static_cast<int>(m_checkBoxUseLocalSyncOptions->GetValue() ? ConfigTypeImage::SYNC: ConfigTypeImage::SYNC_GREY));

    const SyncOptions so = getSyncOptions(); //resolve parameter ownership: some on GUI controls, others member variables

    updateSyncDirectionIcons(so.syncCfg.directionCfg,
                             *m_bpButtonLeftOnly,
                             *m_bpButtonRightOnly,
                             *m_bpButtonLeftNewer,
                             *m_bpButtonRightNewer,
                             *m_bpButtonDifferent,
                             *m_bpButtonConflict);

    //selecting "detect move files" does not always make sense:
    m_checkBoxDetectMove->Enable(detectMovedFilesSelectable(directionCfg));
    m_checkBoxDetectMove->SetValue(detectMovedFilesEnabled(directionCfg)); //parameter NOT owned by checkbox!

    auto setBitmap = [&](wxStaticBitmap& bmpCtrl, bool active, const wxBitmap& bmp)
    {
        if (active &&
            m_checkBoxUseLocalSyncOptions->GetValue()) //help wxWidgets a little to render inactive config state (need on Windows, NOT on Linux!)
            bmpCtrl.SetBitmap(bmp);
        else
            bmpCtrl.SetBitmap(greyScale(bmp));
    };

    //display only relevant sync options
    m_bitmapDatabase     ->Show(so.syncCfg.directionCfg.var == DirectionConfig::TWOWAY);
    fgSizerSyncDirections->Show(so.syncCfg.directionCfg.var != DirectionConfig::TWOWAY);

    if (so.syncCfg.directionCfg.var == DirectionConfig::TWOWAY)
        setBitmap(*m_bitmapDatabase, true, getResourceImage(L"database"));
    else
    {
        const CompareVariant activeCmpVar = m_checkBoxUseLocalCmpOptions->GetValue() ?  localCmpVar : globalCmpVar_;

        m_bitmapDifferent  ->Show(activeCmpVar != CMP_BY_TIME_SIZE);
        m_bpButtonDifferent->Show(activeCmpVar != CMP_BY_TIME_SIZE);

        m_bitmapLeftNewer   ->Show(activeCmpVar == CMP_BY_TIME_SIZE);
        m_bpButtonLeftNewer ->Show(activeCmpVar == CMP_BY_TIME_SIZE);
        m_bitmapRightNewer  ->Show(activeCmpVar == CMP_BY_TIME_SIZE);
        m_bpButtonRightNewer->Show(activeCmpVar == CMP_BY_TIME_SIZE);
    }

    //active variant description:
    setText(*m_textCtrlSyncVarDescription, L"\n" + getSyncVariantDescription(so.syncCfg.directionCfg.var));

    //update toggle buttons -> they have no parameter-ownership at all!
    m_toggleBtnTwoWay->SetValue(false);
    m_toggleBtnMirror->SetValue(false);
    m_toggleBtnUpdate->SetValue(false);
    m_toggleBtnCustom->SetValue(false);

    if (m_checkBoxUseLocalSyncOptions->GetValue()) //help wxWidgets a little to render inactive config state (need on Windows, NOT on Linux!)
        switch (so.syncCfg.directionCfg.var)
        {
            case DirectionConfig::TWOWAY:
                m_toggleBtnTwoWay->SetValue(true);
                break;
            case DirectionConfig::MIRROR:
                m_toggleBtnMirror->SetValue(true);
                break;
            case DirectionConfig::UPDATE:
                m_toggleBtnUpdate->SetValue(true);
                break;
            case DirectionConfig::CUSTOM:
                m_toggleBtnCustom->SetValue(true);
                break;
        }

    m_toggleBtnPermanent ->SetValue(false);
    m_toggleBtnRecycler  ->SetValue(false);
    m_toggleBtnVersioning->SetValue(false);

    if (m_checkBoxUseLocalSyncOptions->GetValue()) //help wxWidgets a little to render inactive config state (need on Windows, NOT on Linux!)
        switch (so.syncCfg.handleDeletion)
        {
            case DELETE_PERMANENTLY:
                m_toggleBtnPermanent->SetValue(true);
                break;
            case DELETE_TO_RECYCLER:
                m_toggleBtnRecycler->SetValue(true);
                break;
            case DELETE_TO_VERSIONING:
                m_toggleBtnVersioning->SetValue(true);
                break;
        }

    const bool versioningSelected = so.syncCfg.handleDeletion == DELETE_TO_VERSIONING;
    m_panelVersioning->Show(versioningSelected);

    if (versioningSelected)
    {
        updateTooltipEnumVal(enumVersioningStyle, *m_choiceVersioningStyle);

        const std::wstring pathSep = utfCvrtTo<std::wstring>(FILE_NAME_SEPARATOR);
        switch (so.syncCfg.versioningStyle)
        {
            case VER_STYLE_REPLACE:
                setText(*m_staticTextNamingCvtPart1, pathSep + _("Folder") + pathSep + _("File") + L".doc");
                setText(*m_staticTextNamingCvtPart2Bold, L"");
                setText(*m_staticTextNamingCvtPart3, L"");
                break;

            case VER_STYLE_ADD_TIMESTAMP:
                setText(*m_staticTextNamingCvtPart1, pathSep + _("Folder") + pathSep + _("File") + L".doc ");
                setText(*m_staticTextNamingCvtPart2Bold, _("YYYY-MM-DD hhmmss"));
                setText(*m_staticTextNamingCvtPart3, L".doc");
                break;
        }
    }

    m_toggleBtnErrorIgnore->SetValue(false);
    m_toggleBtnErrorPopup ->SetValue(false);

    switch (so.onGuiError)
    {
        case ON_GUIERROR_IGNORE:
            m_toggleBtnErrorIgnore->SetValue(true);
            break;
        case ON_GUIERROR_POPUP:
            m_toggleBtnErrorPopup->SetValue(true);
            break;
    }

    m_panelSyncSettings->Layout();
    //Layout();
    //Refresh(); //removes a few artifacts when toggling display of versioning folder

    //GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
}


void ConfigDialog::OnOkay(wxCommandEvent& event)
{
    const SyncOptions so = getSyncOptions();

    //------- parameter validation (BEFORE writing output!) -------

    //check if user-defined directory for deletion was specified:
    if (m_checkBoxUseLocalSyncOptions->GetValue() &&
        so.syncCfg.handleDeletion == zen::DELETE_TO_VERSIONING)
    {
        Zstring versioningDir = so.syncCfg.versioningDirectory;
        trim(versioningDir);
        if (versioningDir.empty())
        {
            m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
            showNotificationDialog(this, DialogInfoType::INFO, PopupDialogCfg().setMainInstructions(_("Please enter a target folder for versioning.")));
            //don't show error icon to follow "Windows' encouraging tone"
            m_panelVersioning->SetFocus();
            return;
        }
    }

    //------------- comparison panel ----------------------
    if (useAlternateCmpCfgOptOut)
        *useAlternateCmpCfgOptOut = m_checkBoxUseLocalCmpOptions->GetValue();

    cmpCfgOut.compareVar = localCmpVar;
    cmpCfgOut.handleSymlinks = !m_checkBoxSymlinksInclude->GetValue() ? SYMLINK_EXCLUDE : m_radioBtnSymlinksDirect->GetValue() ? SYMLINK_DIRECT : SYMLINK_FOLLOW;
    cmpCfgOut.optTimeShiftHours = m_checkBoxTimeShift->GetValue() ? m_spinCtrlTimeShift->GetValue() : 0;

    //------------- filter panel --------------------------
    FilterConfig filterCfg = getFilter();

    //parameter correction: include filter must not be empty!
    {
        Zstring tmp = filterCfg.includeFilter;
        trim(tmp);
        if (tmp.empty())
            filterCfg.includeFilter = FilterConfig().includeFilter; //no need to show error message, just correct user input
    }
    filterCfgOut = filterCfg;

    //------------- synchronization panel -----------------
    if (useAlternateSyncCfgOptOut)
        *useAlternateSyncCfgOptOut = m_checkBoxUseLocalSyncOptions->GetValue();

    syncCfgOut = so.syncCfg;

    if (miscCfgOut)
    {
        miscCfgOut->handleError         = so.onGuiError;
        miscCfgOut->onCompletionCommand = so.onCompletion;
        //a good place to commit current "on completion" history item
        m_comboBoxOnCompletion->addItemHistory();
    }

    EndModal(ReturnSyncConfig::BUTTON_OKAY);
}
}

//########################################################################################

ReturnSyncConfig::ButtonPressed zen::showSyncConfigDlg(wxWindow* parent,
                                                       SyncConfigPanel panelToShow,
                                                       bool* useAlternateCmpCfg,  //optional parameter
                                                       CompConfig&   cmpCfg,
                                                       FilterConfig& filterCfg,
                                                       bool* useAlternateSyncCfg,  //
                                                       SyncConfig&   syncCfg,
                                                       CompareVariant globalCmpVar,
                                                       MiscGlobalCfg* miscCfg, //
                                                       const wxString& title)
{
    ConfigDialog syncDlg(parent, panelToShow, useAlternateCmpCfg, cmpCfg, filterCfg, useAlternateSyncCfg, syncCfg, globalCmpVar, miscCfg, title);
    return static_cast<ReturnSyncConfig::ButtonPressed>(syncDlg.ShowModal());
}
