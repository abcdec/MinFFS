// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.13        *
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

#include "small_dlgs.h"
#include <wx/wupdlock.h>
#include <zen/format_unit.h>
#include <zen/build_info.h>
#include <zen/tick_count.h>
#include <zen/stl_tools.h>
#include <wx+/choice_enum.h>
#include <wx+/bitmap_button.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/image_tools.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
#include "custom_grid.h"
#include "../algorithm.h"
#include "../synchronization.h"
#include "../lib/help_provider.h"
#include "../lib/hard_filter.h"
#include "../version/version.h"

#ifdef ZEN_WIN
    #include <wx+/mouse_move_dlg.h>
#endif

using namespace zen;


class AboutDlg : public AboutDlgGenerated
{
public:
    AboutDlg(wxWindow* parent);

private:
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnOK    (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_OKAY); }
    void OnDonate(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"http://www.freefilesync.org/donate.php"); }
};


AboutDlg::AboutDlg(wxWindow* parent) : AboutDlgGenerated(parent)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonClose));

    setRelativeFontSize(*m_buttonDonate, 1.25);

    assert(m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    m_bitmap9 ->SetBitmap(getResourceImage(L"website"));
    m_bitmap10->SetBitmap(getResourceImage(L"email"));
    m_bitmap13->SetBitmap(getResourceImage(L"gpl"));
    //m_bitmapSmiley->SetBitmap(getResourceImage(L"smiley"));

    m_animCtrlWink->SetAnimation(getResourceAnimation(L"wink"));
    m_animCtrlWink->Play();

    //create language credits
    for (const ExistingTranslations::Entry& trans : ExistingTranslations::get())
    {
        //flag
        wxStaticBitmap* staticBitmapFlag = new wxStaticBitmap(m_scrolledWindowTranslators, wxID_ANY, getResourceImage(trans.languageFlag), wxDefaultPosition, wxSize(-1, 11), 0);
        fgSizerTranslators->Add(staticBitmapFlag, 0, wxALIGN_CENTER);

        //translator name
        wxStaticText* staticTextTranslator = new wxStaticText(m_scrolledWindowTranslators, wxID_ANY, trans.translatorName, wxDefaultPosition, wxDefaultSize, 0);
        staticTextTranslator->Wrap(-1);
        fgSizerTranslators->Add(staticTextTranslator, 0, wxALIGN_CENTER_VERTICAL);

        staticBitmapFlag    ->SetToolTip(trans.languageName);
        staticTextTranslator->SetToolTip(trans.languageName);
    }
    fgSizerTranslators->Fit(m_scrolledWindowTranslators);

#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //-> put *after* creating credits
#endif//TODO_MinFFS_MouseMoveWindow
#endif

    //build information
    wxString build = __TDATE__;
    build += L" - Unicode";
#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    build += zen::is64BitBuild ? L" x64" : L" x86";
    static_assert(zen::is32BitBuild || zen::is64BitBuild, "");

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()

    //generate logo: put *after* first Fit()
    Layout(); //make sure m_panelLogo has final width (required by wxGTK)

#ifdef MinFFS_PATCH // Information Dialog Box Title Change
    wxImage appnameImg = createImageFromText(wxString(L"MinFFS (Modified FreeFileSync)"),
                                             wxFont(wxNORMAL_FONT->GetPointSize() * 1.8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, L"Tahoma"),
                                             *wxBLACK); //accessibility: align foreground/background colors!
    wxImage buildImg = createImageFromText(replaceCpy(_("Build: %x") + L" [Based on FreeFileSync " + zen::currentVersion + L"]", L"%x", build),
                                           *wxNORMAL_FONT,
                                           *wxBLACK);
#else//MinFFS_PATCH
    wxImage appnameImg = createImageFromText(wxString(L"FreeFileSync ") + zen::currentVersion,
                                             wxFont(wxNORMAL_FONT->GetPointSize() * 1.8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, L"Tahoma"),
                                             *wxBLACK); //accessibility: align foreground/background colors!
    wxImage buildImg = createImageFromText(replaceCpy(_("Build: %x"), L"%x", build),
                                           *wxNORMAL_FONT,
                                           *wxBLACK);
#endif//MinFFS_PATCH
    wxImage versionImage = stackImages(appnameImg, buildImg, ImageStackLayout::VERTICAL, ImageStackAlignment::CENTER, 0);

    const int BORDER_SIZE = 5;
    wxBitmap headerBmp(GetClientSize().GetWidth(), versionImage.GetHeight() + 2 * BORDER_SIZE, 24);
    //attention: *must* pass 24 bits, auto-determination fails on Windows high-contrast colors schemes!!!
    //problem only manifests when calling wxDC::DrawBitmap
    {
        wxMemoryDC dc(headerBmp);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        const wxBitmap& bmpGradient = getResourceImage(L"logo_gradient");
        dc.DrawBitmap(bmpGradient, wxPoint(0, (headerBmp.GetHeight() - bmpGradient.GetHeight()) / 2));

        const int logoSize = versionImage.GetHeight();
        const wxBitmap logoBmp = getResourceImage(L"FreeFileSync").ConvertToImage().Scale(logoSize, logoSize, wxIMAGE_QUALITY_HIGH);
        dc.DrawBitmap(logoBmp, wxPoint(2 * BORDER_SIZE, (headerBmp.GetHeight() - logoBmp.GetHeight()) / 2));

        dc.DrawBitmap(versionImage, wxPoint((headerBmp.GetWidth () - versionImage.GetWidth ()) / 2,
                                            (headerBmp.GetHeight() - versionImage.GetHeight()) / 2));
    }
    m_bitmapLogo->SetBitmap(headerBmp);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!

    m_buttonClose->SetFocus(); //on GTK ESC is only associated with wxID_OK correctly if we set at least *any* focus at all!!!
}


void zen::showAboutDialog(wxWindow* parent)
{
    AboutDlg aboutDlg(parent);
    aboutDlg.ShowModal();
}

//########################################################################################

class DeleteDialog : public DeleteDlgGenerated
{
public:
    DeleteDialog(wxWindow* parent,
                 const std::vector<zen::FileSystemObject*>& rowsOnLeft,
                 const std::vector<zen::FileSystemObject*>& rowsOnRight,
                 bool& useRecycleBin);

private:
    void OnOK    (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnUseRecycler   (wxCommandEvent& event) override;

    void updateGui();

    const std::vector<zen::FileSystemObject*>& rowsToDeleteOnLeft;
    const std::vector<zen::FileSystemObject*>& rowsToDeleteOnRight;
    bool& outRefuseRecycleBin;
    const TickVal tickCountStartup;
};


DeleteDialog::DeleteDialog(wxWindow* parent,
                           const std::vector<FileSystemObject*>& rowsOnLeft,
                           const std::vector<FileSystemObject*>& rowsOnRight,
                           bool& useRecycleBin) :
    DeleteDlgGenerated(parent),
    rowsToDeleteOnLeft(rowsOnLeft),
    rowsToDeleteOnRight(rowsOnRight),
    outRefuseRecycleBin(useRecycleBin),
    tickCountStartup(getTicks())
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //allow moving main dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif//TODO_MinFFS_MouseMoveWindow
#endif
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_checkBoxUseRecycler->SetValue(useRecycleBin);

#ifndef __WXGTK__  //wxWidgets holds portability promise by not supporting for multi-line controls...not
    m_textCtrlFileList->SetMaxLength(0); //allow large entries!
#endif

    updateGui();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!

    Layout();

    m_buttonOK->SetFocus();
}


void DeleteDialog::updateGui()
{
#ifdef ZEN_WIN
    wxWindowUpdateLocker dummy(this); //leads to GUI corruption problems on Linux/OS X!
#endif

    const std::pair<Zstring, int> delInfo = zen::deleteFromGridAndHDPreview(rowsToDeleteOnLeft,
                                                                            rowsToDeleteOnRight);
    wxString header;
    if (m_checkBoxUseRecycler->GetValue())
    {
        header = _P("Do you really want to move the following item to the recycle bin?",
                    "Do you really want to move the following %x items to the recycle bin?", delInfo.second);
        m_bitmapDeleteType->SetBitmap(getResourceImage(L"delete_recycler"));
        m_buttonOK->SetLabel(_("Move")); //no access key needed: use ENTER!
    }
    else
    {
        header = _P("Do you really want to delete the following item?",
                    "Do you really want to delete the following %x items?", delInfo.second);
        m_bitmapDeleteType->SetBitmap(getResourceImage(L"delete_permanently"));
        m_buttonOK->SetLabel(_("Delete"));
    }
    m_staticTextHeader->SetLabel(header);
    //it seems like Wrap() needs to be reapplied after SetLabel()
    m_staticTextHeader->Wrap(460);

    const wxString& fileList = utfCvrtTo<wxString>(delInfo.first);
    m_textCtrlFileList->ChangeValue(fileList);
    /*
    There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
    it re-enables all windows that are supposed to be disabled during the current modal loop!
    This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
    => another Unity problem like the following?
    http://trac.wxwidgets.org/ticket/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"
    */

    Layout();
    Refresh(); //needed after m_buttonOK label change
}


void DeleteDialog::OnOK(wxCommandEvent& event)
{
    //additional safety net, similar to Windows Explorer: time delta between DEL and ENTER must be at least 50ms to avoid accidental deletion!
    const TickVal now = getTicks();   //0 on error
    std::int64_t tps = ticksPerSec(); //
    if (now.isValid() && tickCountStartup.isValid() && tps != 0)
        if (dist(tickCountStartup, now) * 1000 / tps < 50)
            return;

    outRefuseRecycleBin = m_checkBoxUseRecycler->GetValue();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


void DeleteDialog::OnUseRecycler(wxCommandEvent& event)
{
    updateGui();
}


ReturnSmallDlg::ButtonPressed zen::showDeleteDialog(wxWindow* parent,
                                                    const std::vector<zen::FileSystemObject*>& rowsOnLeft,
                                                    const std::vector<zen::FileSystemObject*>& rowsOnRight,
                                                    bool& useRecycleBin)
{
    DeleteDialog confirmDeletion(parent,
                                 rowsOnLeft,
                                 rowsOnRight,
                                 useRecycleBin);
    return static_cast<ReturnSmallDlg::ButtonPressed>(confirmDeletion.ShowModal());
}

//########################################################################################

class SyncConfirmationDlg : public SyncConfirmationDlgGenerated
{
public:
    SyncConfirmationDlg(wxWindow* parent,
                        const wxString& variantName,
                        const zen::SyncStatistics& st,
                        bool& dontShowAgain);
private:
    void OnClose    (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnCancel   (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnStartSync(wxCommandEvent& event) override;

    bool& m_dontShowAgain;
};


SyncConfirmationDlg::SyncConfirmationDlg(wxWindow* parent,
                                         const wxString& variantName,
                                         const SyncStatistics& st,
                                         bool& dontShowAgain) :
    SyncConfirmationDlgGenerated(parent),
    m_dontShowAgain(dontShowAgain)
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //allow moving main dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif//TODO_MinFFS_MouseMoveWindow
#endif
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonStartSync).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);
    m_bitmapSync->SetBitmap(getResourceImage(L"sync"));

    m_staticTextVariant->SetLabel(variantName);
    m_checkBoxDontShowAgain->SetValue(dontShowAgain);

    //update preview of item count and bytes to be transferred:
    auto setValue = [](wxStaticText& txtControl, bool isZeroValue, const wxString& valueAsString, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        wxFont fnt = txtControl.GetFont();
        fnt.SetWeight(isZeroValue ? wxFONTWEIGHT_NORMAL : wxFONTWEIGHT_BOLD);
        txtControl.SetFont(fnt);

        setText(txtControl, valueAsString);

        if (isZeroValue)
            bmpControl.SetBitmap(greyScale(mirrorIfRtl(getResourceImage(bmpName))));
        else
            bmpControl.SetBitmap(mirrorIfRtl(getResourceImage(bmpName)));
    };

    auto setIntValue = [&setValue](wxStaticText& txtControl, int value, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        setValue(txtControl, value == 0, toGuiString(value), bmpControl, bmpName);
    };

    setValue(*m_staticTextData, st.getDataToProcess() == 0, filesizeToShortString(st.getDataToProcess()), *m_bitmapData,  L"data");
    setIntValue(*m_staticTextCreateLeft,  st.getCreate<LEFT_SIDE >(), *m_bitmapCreateLeft,  L"so_create_left_small");
    setIntValue(*m_staticTextUpdateLeft,  st.getUpdate<LEFT_SIDE >(), *m_bitmapUpdateLeft,  L"so_update_left_small");
    setIntValue(*m_staticTextDeleteLeft,  st.getDelete<LEFT_SIDE >(), *m_bitmapDeleteLeft,  L"so_delete_left_small");
    setIntValue(*m_staticTextCreateRight, st.getCreate<RIGHT_SIDE>(), *m_bitmapCreateRight, L"so_create_right_small");
    setIntValue(*m_staticTextUpdateRight, st.getUpdate<RIGHT_SIDE>(), *m_bitmapUpdateRight, L"so_update_right_small");
    setIntValue(*m_staticTextDeleteRight, st.getDelete<RIGHT_SIDE>(), *m_bitmapDeleteRight, L"so_delete_right_small");

    m_panelStatistics->Layout();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!

    m_buttonStartSync->SetFocus();
}


void SyncConfirmationDlg::OnStartSync(wxCommandEvent& event)
{
    m_dontShowAgain = m_checkBoxDontShowAgain->GetValue();
    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed zen::showSyncConfirmationDlg(wxWindow* parent,
                                                           const wxString& variantName,
                                                           const zen::SyncStatistics& statistics,
                                                           bool& dontShowAgain)
{
    SyncConfirmationDlg dlg(parent,
                            variantName,
                            statistics,
                            dontShowAgain);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

class OptionsDlg : public OptionsDlgGenerated
{
public:
    OptionsDlg(wxWindow* parent, xmlAccess::XmlGlobalSettings& globalSettings);

private:
    void OnOkay        (wxCommandEvent& event) override;
    void OnResetDialogs(wxCommandEvent& event) override;
    void OnDefault     (wxCommandEvent& event) override;
    void OnCancel      (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose       (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnAddRow      (wxCommandEvent& event) override;
    void OnRemoveRow   (wxCommandEvent& event) override;
    void OnHelpShowExamples(wxHyperlinkEvent& event) override { displayHelpEntry(L"html/External Applications.html", this); }
    void onResize(wxSizeEvent& event);
    void updateGui();

    void OnToggleAutoRetryCount(wxCommandEvent& event) override { updateGui(); }

    void setExtApp(const xmlAccess::ExternalApps& extApp);
    xmlAccess::ExternalApps getExtApp() const;

    xmlAccess::XmlGlobalSettings& settings;
    std::map<std::wstring, std::wstring> descriptionTransToEng; //"translated description" -> "english" mapping for external application config
};


OptionsDlg::OptionsDlg(wxWindow* parent, xmlAccess::XmlGlobalSettings& globalSettings) :
    OptionsDlgGenerated(parent),
    settings(globalSettings)
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //allow moving dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif//TODO_MinFFS_MouseMoveWindow
#endif
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    warn_static("remove after test")
    //#ifdef ZEN_MAC
    //	SetTitle(_("Preferences")); //follow OS conventions
    //#endif

    //setMainInstructionFont(*m_staticTextHeader);

    m_gridCustomCommand->SetTabBehaviour(wxGrid::Tab_Leave);

    m_bitmapSettings    ->SetBitmap     (getResourceImage(L"settings"));
    m_bpButtonAddRow    ->SetBitmapLabel(getResourceImage(L"item_add"));
    m_bpButtonRemoveRow ->SetBitmapLabel(getResourceImage(L"item_remove"));
    setBitmapTextLabel(*m_buttonResetDialogs, getResourceImage(L"reset_dialogs").ConvertToImage(), m_buttonResetDialogs->GetLabel());

    m_checkBoxFailSafe       ->SetValue(globalSettings.failsafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(globalSettings.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(globalSettings.copyFilePermissions);

    m_spinCtrlAutoRetryCount->SetValue(globalSettings.automaticRetryCount);
    m_spinCtrlAutoRetryDelay->SetValue(globalSettings.automaticRetryDelay);

    setExtApp(globalSettings.gui.externelApplications);

    updateGui();

#ifdef ZEN_WIN
    m_checkBoxCopyPermissions->SetLabel(_("Copy NTFS permissions"));
#elif defined ZEN_LINUX || defined ZEN_MAC
    bSizerLockedFiles->Show(false);
#endif

    const wxString toolTip = wxString(_("Integrate external applications into context menu. The following macros are available:")) + L"\n\n" +
                             L"%item_path%    \t" + _("- full file or folder name") + L"\n" +
                             L"%item_folder%  \t" + _("- folder part only") + L"\n" +
                             L"%item2_path%   \t" + _("- Other side's counterpart to %item_path%") + L"\n" +
                             L"%item2_folder% \t" + _("- Other side's counterpart to %item_folder%");

    m_gridCustomCommand->GetGridWindow()->SetToolTip(toolTip);
    m_gridCustomCommand->GetGridColLabelWindow()->SetToolTip(toolTip);
    m_gridCustomCommand->SetMargins(0, 0);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!

    Layout();

    //automatically fit column width to match total grid width
    Connect(wxEVT_SIZE, wxSizeEventHandler(OptionsDlg::onResize), nullptr, this);
    wxSizeEvent dummy;
    onResize(dummy);

    m_buttonOkay->SetFocus();
}


void OptionsDlg::onResize(wxSizeEvent& event)
{
    const int widthTotal = m_gridCustomCommand->GetGridWindow()->GetClientSize().GetWidth();

    if (widthTotal >= 0 && m_gridCustomCommand->GetNumberCols() == 2)
    {
        const int w0 = widthTotal * 2 / 5; //ratio 2 : 3
        const int w1 = widthTotal - w0;
        m_gridCustomCommand->SetColSize(0, w0);
        m_gridCustomCommand->SetColSize(1, w1);

        m_gridCustomCommand->Refresh(); //required on Ubuntu
    }

    event.Skip();
}


void OptionsDlg::updateGui()
{
    const bool autoRetryActive = m_spinCtrlAutoRetryCount->GetValue() > 0;
    m_staticTextAutoRetryDelay->Enable(autoRetryActive);
    m_spinCtrlAutoRetryDelay->Enable(autoRetryActive);
}


void OptionsDlg::OnOkay(wxCommandEvent& event)
{
    //write settings only when okay-button is pressed!
    settings.failsafeFileCopy    = m_checkBoxFailSafe->GetValue();
    settings.copyLockedFiles     = m_checkBoxCopyLocked->GetValue();
    settings.copyFilePermissions = m_checkBoxCopyPermissions->GetValue();

    settings.automaticRetryCount = m_spinCtrlAutoRetryCount->GetValue();
    settings.automaticRetryDelay = m_spinCtrlAutoRetryDelay->GetValue();

    settings.gui.externelApplications = getExtApp();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


void OptionsDlg::OnResetDialogs(wxCommandEvent& event)
{
    switch (showConfirmationDialog(this, DialogInfoType::INFO,
                                   PopupDialogCfg().setMainInstructions(_("Show hidden dialogs and warning messages again?")),
                                   _("&Show")))
    {
        case ConfirmationButton::DO_IT:
            settings.optDialogs.resetDialogs();
            break;
        case ConfirmationButton::CANCEL:
            break;
    }
}


void OptionsDlg::OnDefault(wxCommandEvent& event)
{
    const xmlAccess::XmlGlobalSettings defaultCfg;

    m_checkBoxFailSafe       ->SetValue(defaultCfg.failsafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(defaultCfg.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(defaultCfg.copyFilePermissions);

    m_spinCtrlAutoRetryCount->SetValue(defaultCfg.automaticRetryCount);
    m_spinCtrlAutoRetryDelay->SetValue(defaultCfg.automaticRetryDelay);

    setExtApp(defaultCfg.gui.externelApplications);

    updateGui();
}


void OptionsDlg::setExtApp(const xmlAccess::ExternalApps& extApp)
{
    auto extAppTmp = extApp;
    vector_remove_if(extAppTmp, [](decltype(extAppTmp[0])& entry) { return entry.first.empty() && entry.second.empty(); });

    extAppTmp.resize(extAppTmp.size() + 1); //append empty row to facilitate insertions

    const int rowCount = m_gridCustomCommand->GetNumberRows();
    if (rowCount > 0)
        m_gridCustomCommand->DeleteRows(0, rowCount);

    m_gridCustomCommand->AppendRows(static_cast<int>(extAppTmp.size()));
    for (auto it = extAppTmp.begin(); it != extAppTmp.end(); ++it)
    {
        const int row = it - extAppTmp.begin();

        const std::wstring description = zen::implementation::translate(it->first);
        if (description != it->first) //remember english description to save in GlobalSettings.xml later rather than hard-code translation
            descriptionTransToEng[description] = it->first;

        m_gridCustomCommand->SetCellValue(row, 0, description); //description
        m_gridCustomCommand->SetCellValue(row, 1, it->second);  //commandline
    }
}


xmlAccess::ExternalApps OptionsDlg::getExtApp() const
{
    xmlAccess::ExternalApps output;
    for (int i = 0; i < m_gridCustomCommand->GetNumberRows(); ++i)
    {
        auto description = copyStringTo<std::wstring>(m_gridCustomCommand->GetCellValue(i, 0));
        auto commandline = copyStringTo<std::wstring>(m_gridCustomCommand->GetCellValue(i, 1));

        //try to undo translation of description for GlobalSettings.xml
        auto it = descriptionTransToEng.find(description);
        if (it != descriptionTransToEng.end())
            description = it->second;

        if (!description.empty() || !commandline.empty())
            output.emplace_back(description, commandline);
    }
    return output;
}


void OptionsDlg::OnAddRow(wxCommandEvent& event)
{
#ifdef ZEN_WIN
    wxWindowUpdateLocker dummy(this); //leads to GUI corruption problems on Linux/OS X!
#endif

    const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
    if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
        m_gridCustomCommand->InsertRows(selectedRow);
    else
        m_gridCustomCommand->AppendRows();
}


void OptionsDlg::OnRemoveRow(wxCommandEvent& event)
{
    if (m_gridCustomCommand->GetNumberRows() > 0)
    {
#ifdef ZEN_WIN
        wxWindowUpdateLocker dummy(this); //leads to GUI corruption problems on Linux/OS X!
#endif

        const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
        if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
            m_gridCustomCommand->DeleteRows(selectedRow);
        else
            m_gridCustomCommand->DeleteRows(m_gridCustomCommand->GetNumberRows() - 1);
    }
}


ReturnSmallDlg::ButtonPressed zen::showOptionsDlg(wxWindow* parent, xmlAccess::XmlGlobalSettings& globalSettings)
{
    OptionsDlg dlg(parent, globalSettings);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

class SelectTimespanDlg : public SelectTimespanDlgGenerated
{
public:
    SelectTimespanDlg(wxWindow* parent, std::int64_t& timeFrom, std::int64_t& timeTo);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void OnChangeSelectionFrom(wxCalendarEvent& event) override
    {
        if (m_calendarFrom->GetDate() > m_calendarTo->GetDate())
            m_calendarTo->SetDate(m_calendarFrom->GetDate());
    }
    void OnChangeSelectionTo(wxCalendarEvent& event) override
    {
        if (m_calendarFrom->GetDate() > m_calendarTo->GetDate())
            m_calendarFrom->SetDate(m_calendarTo->GetDate());
    }

    std::int64_t& timeFrom_;
    std::int64_t& timeTo_;
};


wxDateTime utcToLocalDateTime(time_t utcTime)
{
    //wxDateTime models local(!) time (in contrast to what documentation says), but this constructor takes time_t UTC
    return wxDateTime(utcTime);
}

time_t localDateTimeToUtc(const wxDateTime& localTime)
{
    return localTime.GetTicks();
}


SelectTimespanDlg::SelectTimespanDlg(wxWindow* parent, std::int64_t& timeFrom, std::int64_t& timeTo) :
    SelectTimespanDlgGenerated(parent),
    timeFrom_(timeFrom),
    timeTo_(timeTo)
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS_MouseMoveWindow
    new zen::MouseMoveWindow(*this); //allow moving main dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif//TODO_MinFFS_MouseMoveWindow
#endif
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    long style = wxCAL_SHOW_HOLIDAYS | wxCAL_SHOW_SURROUNDING_WEEKS;

#ifdef ZEN_WIN
    DWORD firstDayOfWeek = 0;
    if (::GetLocaleInfo(LOCALE_USER_DEFAULT,     //__in   LCID Locale,
                        LOCALE_IFIRSTDAYOFWEEK | // first day of week specifier, 0-6, 0=Monday, 6=Sunday
                        LOCALE_RETURN_NUMBER,    //__in   LCTYPE LCType,
                        reinterpret_cast<LPTSTR>(&firstDayOfWeek),     //__out  LPTSTR lpLCData,
                        sizeof(firstDayOfWeek) / sizeof(TCHAR)) > 0 && //__in   int cchData
        firstDayOfWeek == 6)
        style |= wxCAL_SUNDAY_FIRST;
    else //default
#endif
        style |= wxCAL_MONDAY_FIRST;

    m_calendarFrom->SetWindowStyleFlag(style);
    m_calendarTo  ->SetWindowStyleFlag(style);

    //set default values
    if (timeTo_ == 0)
        timeTo_ = wxGetUTCTime(); //
    if (timeFrom_ == 0)
        timeFrom_ = timeTo_ - 7 * 24 * 3600; //default time span: one week from "now"

    m_calendarFrom->SetDate(utcToLocalDateTime(timeFrom_));
    m_calendarTo  ->SetDate(utcToLocalDateTime(timeTo_));

#if wxCHECK_VERSION(2, 9, 5)
    //doesn't seem to be a problem here:
#else
    //wxDatePickerCtrl::BestSize() does not respect year field and trims it, both wxMSW/wxGTK - why isn't there anybody testing this wxWidgets stuff???
    wxSize minSz = m_calendarFrom->GetBestSize();
    minSz.x += 30;
    m_calendarFrom->SetMinSize(minSz);
    m_calendarTo  ->SetMinSize(minSz);
#endif

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!

    m_buttonOkay->SetFocus();
}


void SelectTimespanDlg::OnOkay(wxCommandEvent& event)
{
    wxDateTime from = m_calendarFrom->GetDate();
    wxDateTime to   = m_calendarTo  ->GetDate();

    //align to full days
    from.ResetTime();
    to += wxTimeSpan::Day();
    to.ResetTime(); //reset local(!) time
    to -= wxTimeSpan::Second(); //go back to end of previous day

    timeFrom_ = localDateTimeToUtc(from);
    timeTo_   = localDateTimeToUtc(to);

    /*
    {
        time_t current = zen::to<time_t>(timeFrom_);
        struct tm* tdfewst = ::localtime(&current);
        int budfk = 3;
    }
    {
        time_t current = zen::to<time_t>(timeTo_);
        struct tm* tdfewst = ::localtime(&current);
        int budfk = 3;
    }
    */

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed zen::showSelectTimespanDlg(wxWindow* parent, std::int64_t& timeFrom, std::int64_t& timeTo)
{
    SelectTimespanDlg timeSpanDlg(parent, timeFrom, timeTo);
    return static_cast<ReturnSmallDlg::ButtonPressed>(timeSpanDlg.ShowModal());
}
