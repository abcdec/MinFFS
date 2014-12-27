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

#include "dir_name.h"
#ifdef TODO_MinFFS_UI
#include <zen/thread.h>
#endif//TODO_MinFFS_UI
#include <zen/file_access.h>
#include <wx/dnd.h>
#include <wx/window.h>
#include <wx/textctrl.h>
#include <wx/statbox.h>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx+/string_conv.h>
#include <wx+/popup_dlg.h>
#include "../lib/resolve_path.h"
#include "folder_history_box.h"

#ifdef ZEN_WIN
    #include <zen/dll.h>
    #include <zen/win_ver.h>
    #include "../dll/IFileDialog_Vista\ifile_dialog.h"
#endif

using namespace zen;


const wxEventType zen::EVENT_ON_DIR_SELECTED          = wxNewEventType();
const wxEventType zen::EVENT_ON_DIR_MANUAL_CORRECTION = wxNewEventType();

namespace
{
void setDirectoryNameImpl(const wxString& dirpath, wxWindow& tooltipWnd, wxStaticText* staticText)
{
    const wxString dirFormatted = utfCvrtTo<wxString>(getFormattedDirectoryPath(toZ(dirpath))); //may block when resolving [<volume name>]

    tooltipWnd.SetToolTip(nullptr); //workaround wxComboBox bug http://trac.wxwidgets.org/ticket/10512 / http://trac.wxwidgets.org/ticket/12659
    tooltipWnd.SetToolTip(dirFormatted); //who knows when the real bugfix reaches mere mortals via an official release...

    if (staticText)
    {
        //change static box label only if there is a real difference to what is shown in wxTextCtrl anyway
        wxString dirNormalized = dirpath;
        trim(dirNormalized);
        if (!dirNormalized.empty())
            if (!endsWith(dirNormalized, FILE_NAME_SEPARATOR))
                dirNormalized += FILE_NAME_SEPARATOR;

        staticText->SetLabel(dirNormalized == dirFormatted ? wxString(_("Drag && drop")) : dirFormatted);
    }
}


void setDirectoryName(const wxString&  dirpath,
                      wxTextCtrl*      txtCtrl,
                      wxWindow&        tooltipWnd,
                      wxStaticText*    staticText) //pointers are optional
{
    if (txtCtrl)
        txtCtrl->ChangeValue(dirpath);
    setDirectoryNameImpl(dirpath, tooltipWnd, staticText);
}


void setDirectoryName(const wxString&   dirpath,
                      FolderHistoryBox* comboBox,
                      wxWindow&         tooltipWnd,
                      wxStaticText*    staticText) //pointers are optional
{
    if (comboBox)
        comboBox->setValue(dirpath);
    setDirectoryNameImpl(dirpath, tooltipWnd, staticText);
}
}
//##############################################################################################################

template <class NameControl>
DirectoryName<NameControl>::DirectoryName(wxWindow&     dropWindow,
                                          wxButton&     selectButton,
                                          NameControl&  dirpath,
                                          wxStaticText* staticText,
                                          wxWindow*     dropWindow2) :
    dropWindow_(dropWindow),
    dropWindow2_(dropWindow2),
    selectButton_(selectButton),
    dirpath_(dirpath),
    staticText_(staticText)
{
    //prepare drag & drop
    setupFileDrop(dropWindow_);
    dropWindow_.Connect(EVENT_DROP_FILE, FileDropEventHandler(DirectoryName::onFilesDropped), nullptr, this);

    if (dropWindow2_)
    {
        setupFileDrop(*dropWindow2_);
        dropWindow2_->Connect(EVENT_DROP_FILE, FileDropEventHandler(DirectoryName::onFilesDropped), nullptr, this);
    }

    //keep dirPicker and dirpath synchronous
    dirpath_     .Connect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (DirectoryName::onMouseWheel      ), nullptr, this);
    dirpath_     .Connect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(DirectoryName::onWriteDirManually), nullptr, this);
    selectButton_.Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DirectoryName::onSelectDir       ), nullptr, this);
}


template <class NameControl>
DirectoryName<NameControl>::~DirectoryName()
{
    dropWindow_.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(DirectoryName::onFilesDropped), nullptr, this);
    if (dropWindow2_)
        dropWindow2_->Disconnect(EVENT_DROP_FILE, FileDropEventHandler(DirectoryName::onFilesDropped), nullptr, this);

    dirpath_     .Disconnect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (DirectoryName::onMouseWheel      ), nullptr, this);
    dirpath_     .Disconnect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(DirectoryName::onWriteDirManually), nullptr, this);
    selectButton_.Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DirectoryName::onSelectDir       ), nullptr, this);
}


template <class NameControl>
void DirectoryName<NameControl>::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT windows default, e.g. explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    wxWindow* wnd = &dirpath_;
    while ((wnd = wnd->GetParent()) != nullptr) //silence MSVC warning
        if (dynamic_cast<wxScrolledWindow*>(wnd) != nullptr)
            if (wxEvtHandler* evtHandler = wnd->GetEventHandler())
            {
                evtHandler->AddPendingEvent(event);
                break;
            }

    //	event.Skip();
}


template <class NameControl>
void DirectoryName<NameControl>::onFilesDropped(FileDropEvent& event)
{
    const auto& files = event.getFiles();
    if (files.empty())
        return;

    if (acceptDrop(files, event.getDropPosition(), event.getDropWindow()))
    {
        const wxString fileName = event.getFiles()[0];
        if (dirExists(toZ(fileName)))
            setDirectoryName(fileName, &dirpath_, dirpath_, staticText_);
        else
        {
            wxString parentName = beforeLast(fileName, utfCvrtTo<wxString>(FILE_NAME_SEPARATOR)); //returns empty string if ch not found
#ifdef ZEN_WIN
            if (endsWith(parentName, L":")) //volume name
                parentName += FILE_NAME_SEPARATOR;
#endif
            if (dirExists(toZ(parentName)))
                setDirectoryName(parentName, &dirpath_, dirpath_, staticText_);
            else //set original name unconditionally: usecase: inactive mapped network shares
                setDirectoryName(fileName, &dirpath_, dirpath_, staticText_);
        }

        //notify action invoked by user
        wxCommandEvent dummy(EVENT_ON_DIR_SELECTED);
        ProcessEvent(dummy);
    }
    else
        event.Skip(); //let other handlers try!!!
}


template <class NameControl>
void DirectoryName<NameControl>::onWriteDirManually(wxCommandEvent& event)
{
    setDirectoryName(event.GetString(), static_cast<NameControl*>(nullptr), dirpath_, staticText_);

    wxCommandEvent dummy(EVENT_ON_DIR_MANUAL_CORRECTION);
    ProcessEvent(dummy);
    event.Skip();
}


template <class NameControl>
void DirectoryName<NameControl>::onSelectDir(wxCommandEvent& event)
{
    wxString defaultdirpath; //default selection for dir picker
    {
        const Zstring dirFmt = getFormattedDirectoryPath(toZ(getPath()));
        if (!dirFmt.empty())
        {
#ifdef TODO_MinFFS_UI
            //convert to Zstring first: we don't want to pass wxString by value and risk MT issues!
            auto ft = async([=] { return zen::dirExists(dirFmt); });

            if (ft.timed_wait(boost::posix_time::milliseconds(200)) && ft.get()) //potentially slow network access: wait 200ms at most
                defaultdirpath = utfCvrtTo<wxString>(dirFmt);
#endif//TODO_MinFFS_UI
        }
    }

    //wxDirDialog internally uses lame-looking SHBrowseForFolder(); we better use IFileDialog() instead! (remembers size and position!)
    std::unique_ptr<wxString> newFolder;
#ifdef ZEN_WIN
    if (vistaOrLater())
    {
#define DEF_DLL_FUN(name) const DllFun<ifile::FunType_##name> name(ifile::getDllName(), ifile::funName_##name);
        DEF_DLL_FUN(showFolderPicker);
        DEF_DLL_FUN(freeString);
#undef DEF_DLL_FUN

        if (showFolderPicker && freeString)
        {
            wchar_t* selectedFolder = nullptr;
            wchar_t* errorMsg       = nullptr;
            bool cancelled = false;
            ZEN_ON_SCOPE_EXIT(freeString(selectedFolder));
            ZEN_ON_SCOPE_EXIT(freeString(errorMsg));

            const ifile::GuidProxy guid = { '\x0', '\x4a', '\xf9', '\x31', '\xb4', '\x92', '\x40', '\xa0',
                                            '\x8d', '\xc2', '\xc', '\xa5', '\xef', '\x59', '\x6e', '\x3b'
                                          }; //some random GUID => have Windows save IFileDialog state separately from other file/dir pickers!

            showFolderPicker(static_cast<HWND>(selectButton_.GetHWND()), //in;  ==HWND
                             defaultdirpath.empty() ? static_cast<const wchar_t*>(nullptr) : defaultdirpath.c_str(), //in, optional!
                             &guid,
                             selectedFolder, //out: call freeString() after use!
                             cancelled,      //out
                             errorMsg);      //out, optional: call freeString() after use!
            if (errorMsg)
            {
                showNotificationDialog(&dropWindow_, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(errorMsg));
                return;
            }
            if (cancelled || !selectedFolder)
                return;
            newFolder = make_unique<wxString>(selectedFolder);
        }
    }
#endif
    if (!newFolder.get())
    {
        wxDirDialog dirPicker(&selectButton_, _("Select a folder"), defaultdirpath); //put modal wxWidgets dialogs on stack: creating on freestore leads to memleak!
        if (dirPicker.ShowModal() != wxID_OK)
            return;
        newFolder = make_unique<wxString>(dirPicker.GetPath());
    }

    setDirectoryName(*newFolder, &dirpath_, dirpath_, staticText_);

    //notify action invoked by user
    wxCommandEvent dummy(EVENT_ON_DIR_SELECTED);
    ProcessEvent(dummy);
}


//#ifdef TODO_MinFFS_GUICONFLICT
template <class NameControl>
wxString DirectoryName<NameControl>::getPath() const
{
    return dirpath_.GetValue();
}


template <class NameControl>
void DirectoryName<NameControl>::setPath(const wxString& dirpath)
{
    setDirectoryName(dirpath, &dirpath_, dirpath_, staticText_);
}

//explicit template instantiations
namespace zen
{
template class DirectoryName<wxTextCtrl>;
template class DirectoryName<FolderHistoryBox>;
}
//#endif//TODO_MinFFS2
