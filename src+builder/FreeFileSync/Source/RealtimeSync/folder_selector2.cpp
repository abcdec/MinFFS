// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "folder_selector2.h"
#include <zen/thread.h>
#include <zen/file_access.h>
#include <zen/optional.h>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx+/string_conv.h>
#include <wx+/popup_dlg.h>
#include "../lib/resolve_path.h"
#ifdef ZEN_WIN_VISTA_AND_LATER
    #include "../ui/ifile_dialog.h"

#elif defined ZEN_LINUX
    #include <gtk/gtk.h>
#endif

using namespace zen;


namespace
{
void setFolderPath(const Zstring& dirpath, wxTextCtrl* txtCtrl, wxWindow& tooltipWnd, wxStaticText* staticText) //pointers are optional
{
    if (txtCtrl)
        txtCtrl->ChangeValue(toWx(dirpath));

    const Zstring folderPathFmt = getResolvedFilePath(dirpath); //may block when resolving [<volume name>]

    tooltipWnd.SetToolTip(nullptr); //workaround wxComboBox bug http://trac.wxwidgets.org/ticket/10512 / http://trac.wxwidgets.org/ticket/12659
    tooltipWnd.SetToolTip(toWx(folderPathFmt)); //who knows when the real bugfix reaches mere mortals via an official release...

    if (staticText) //change static box label only if there is a real difference to what is shown in wxTextCtrl anyway
        staticText->SetLabel(equalFilePath(appendSeparator(trimCpy(dirpath)), appendSeparator(folderPathFmt)) ? wxString(_("Drag && drop")) : toWx(folderPathFmt));
}
}

//##############################################################################################################

FolderSelector2::FolderSelector2(wxWindow&     dropWindow,
                                 wxButton&     selectButton,
                                 wxTextCtrl&   folderPathCtrl,
                                 wxStaticText* staticText) :
    dropWindow_(dropWindow),
    selectButton_(selectButton),
    folderPathCtrl_(folderPathCtrl),
    staticText_(staticText)
{
#ifdef ZEN_LINUX
    //file drag and drop directly into the text control unhelpfully inserts in format "file://..<cr><nl>"; see folder_history_box.cpp
    if (GtkWidget* widget = folderPathCtrl.GetConnectWidget())
        ::gtk_drag_dest_unset(widget);
#endif

    //prepare drag & drop
    setupFileDrop(dropWindow_);
    dropWindow_.Connect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector2::onFilesDropped), nullptr, this);

    //keep dirPicker and dirpath synchronous
    folderPathCtrl_.Connect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector2::onMouseWheel    ), nullptr, this);
    folderPathCtrl_.Connect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector2::onEditFolderPath), nullptr, this);
    selectButton_  .Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector2::onSelectDir     ), nullptr, this);
}


FolderSelector2::~FolderSelector2()
{
    dropWindow_.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector2::onFilesDropped), nullptr, this);

    folderPathCtrl_.Disconnect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector2::onMouseWheel    ), nullptr, this);
    folderPathCtrl_.Disconnect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector2::onEditFolderPath), nullptr, this);
    selectButton_  .Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector2::onSelectDir     ), nullptr, this);
}


void FolderSelector2::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT windows default, e.g. explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    wxWindow* wnd = &folderPathCtrl_;
    while ((wnd = wnd->GetParent()) != nullptr) //silence MSVC warning
        if (dynamic_cast<wxScrolledWindow*>(wnd) != nullptr)
            if (wxEvtHandler* evtHandler = wnd->GetEventHandler())
            {
                evtHandler->AddPendingEvent(event);
                break;
            }
    //  event.Skip();
}


void FolderSelector2::onFilesDropped(FileDropEvent& event)
{
    const auto& itemPaths = event.getPaths();
    if (itemPaths.empty())
        return;

    Zstring itemPath = itemPaths[0];
    if (!dirExists(itemPath))
    {
        Zstring parentPath = beforeLast(itemPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
#ifdef ZEN_WIN
        if (endsWith(parentPath, L":")) //volume root
            parentPath += FILE_NAME_SEPARATOR;
#endif
        if (dirExists(parentPath))
            itemPath = parentPath;
        //else: keep original name unconditionally: usecase: inactive mapped network shares
    }
    setPath(itemPath);

    //event.Skip();
}


void FolderSelector2::onEditFolderPath(wxCommandEvent& event)
{
    setFolderPath(toZ(event.GetString()), nullptr, folderPathCtrl_, staticText_);
    event.Skip();
}


#ifdef ZEN_WIN_VISTA_AND_LATER
bool onIFileDialogAcceptFolder(HWND wnd, const Zstring& folderPath)
{
    if (dirExists(folderPath))
        return true;

    const std::wstring msg = replaceCpy(_("Cannot find folder %x."), L"%x", fmtPath(folderPath));
    ::MessageBox(wnd, msg.c_str(), (_("Select a folder")).c_str(), MB_ICONWARNING);
    //showNotificationDialog would not support HWND parent
    return false;
}
#endif


void FolderSelector2::onSelectDir(wxCommandEvent& event)
{
    //IFileDialog requirements for default path: 1. accepts native paths only!!! 2. path must exist!
    Zstring defaultFolderPath;
    {
        const Zstring folderPath = getResolvedFilePath(getPath());
        if (!folderPath.empty())
        {
            auto ft = runAsync([folderPath] { return dirExists(folderPath); });

            if (ft.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready && ft.get()) //potentially slow network access: wait 200ms at most
                defaultFolderPath = folderPath;
        }
    }

#ifdef ZEN_WIN_VISTA_AND_LATER
    Zstring newFolder;
    try
    {
        //some random GUID => have Windows save IFileDialog state separately from other file/dir pickers!
        const GUID guid = { 0xe89c1f5d, 0xb217, 0x5546, { 0xa3, 0xc0, 0xdc, 0xcb, 0x37, 0xbb, 0x4e, 0x35 } };

        const std::pair<Zstring, bool> rv = ifile::showFolderPicker(static_cast<HWND>(selectButton_.GetHWND()), defaultFolderPath, nullptr, &guid, onIFileDialogAcceptFolder); //throw FileError
        if (!rv.second) //cancelled
            return;
        newFolder = rv.first;
    }
    catch (const FileError& e) { showNotificationDialog(&dropWindow_, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); return; }
#else
    wxDirDialog dirPicker(&selectButton_, _("Select a folder"), toWx(defaultFolderPath)); //put modal wxWidgets dialogs on stack: creating on freestore leads to memleak!
    if (dirPicker.ShowModal() != wxID_OK)
        return;
    const Zstring newFolder = toZ(dirPicker.GetPath());
#endif

    setFolderPath(newFolder, &folderPathCtrl_, folderPathCtrl_, staticText_);
}


Zstring FolderSelector2::getPath() const
{
    return toZ(folderPathCtrl_.GetValue());
}


void FolderSelector2::setPath(const Zstring& dirpath)
{
    setFolderPath(dirpath, &folderPathCtrl_, folderPathCtrl_, staticText_);
}
