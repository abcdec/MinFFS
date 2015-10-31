// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILE_DROP_H_09457802957842560325626
#define FILE_DROP_H_09457802957842560325626

#include <vector>
#include <functional>
#include <zen/zstring.h>
#include <zen/utf.h>
#include <wx/window.h>
#include <wx/event.h>
#include <wx/dnd.h>

#ifdef ZEN_WIN_VISTA_AND_LATER
    #include <zen/win.h>
#endif


namespace zen
{
//register simple file drop event (without issue of freezing dialogs and without wxFileDropTarget overdesign)
//CAVEAT: a drop target window must not be directly or indirectly contained within a wxStaticBoxSizer until the following wxGTK bug
//is fixed. According to wxWidgets release cycles this is expected to be: never http://trac.wxwidgets.org/ticket/2763

/*
1. setup a window to emit EVENT_DROP_FILE:
    - simple file system paths:        setupFileDrop
    - any shell paths with validation: setupShellItemDrop

2. register events:
wnd.Connect   (EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);
wnd.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);

3. do something:
void MyDlg::OnFilesDropped(FileDropEvent& event);
*/









namespace impl
{
inline
wxEventType createNewEventType()
{
    //inline functions have external linkage by default => this static is also extern, i.e. program wide unique! but defined in a header... ;)
    static wxEventType dummy = wxNewEventType();
    return dummy;
}
}

//define new event type
const wxEventType EVENT_DROP_FILE = impl::createNewEventType();

class FileDropEvent : public wxCommandEvent
{
public:
    FileDropEvent(const std::vector<Zstring>& droppedPaths) : wxCommandEvent(EVENT_DROP_FILE), droppedPaths_(droppedPaths) {}

    const std::vector<Zstring>& getPaths() const { return droppedPaths_; }

private:
    wxEvent* Clone() const override { return new FileDropEvent(*this); }

    const std::vector<Zstring> droppedPaths_;
};

typedef void (wxEvtHandler::*FileDropEventFunction)(FileDropEvent&);

#define FileDropEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(FileDropEventFunction, &func)



#ifdef ZEN_WIN_VISTA_AND_LATER
using DragDropValidator = bool (*)(const std::vector<Zstring>& shellItemPaths); //plain static function => no lifetime management needed!
using DragDropConsumer  = std::function<void (const std::vector<Zstring>& shellItemPaths)>;

namespace impl
{
void registerDragDrop(HWND hwnd, const DragDropValidator& acceptDrop, const DragDropConsumer& onDrop);
void unregisterDragDrop(HWND hwnd);

class DragDropCleanupWindow : private wxWindow
{
public:
    DragDropCleanupWindow(wxWindow& dropWindow) : wxWindow(&dropWindow, wxID_ANY), dropHwnd(dropWindow.GetHWND())
    {
        Hide(); //this is just a dummy window so that its parent can have ownership
        Disable();
    }
    ~DragDropCleanupWindow() { impl::unregisterDragDrop(dropHwnd); }

private:
    HWND dropHwnd;
};
}


inline
void setupShellItemDrop(wxWindow& dropWindow, const DragDropValidator& acceptDrop)
{
    auto onDrop = [&dropWindow](const std::vector<Zstring>& shellItemPaths)
    {
        //create a custom event on drop window: execute event after file dropping is completed! (after mouse is released)
        if (wxEvtHandler* handler = dropWindow.GetEventHandler())
            handler->AddPendingEvent(FileDropEvent(shellItemPaths));
    };

    impl::registerDragDrop(static_cast<HWND>(dropWindow.GetHWND()), acceptDrop, onDrop);

    //make sure clean-up is tied to dropWindow life-time:
    new impl::DragDropCleanupWindow(dropWindow); //ownership passed to "dropWindow"
}
#endif


namespace impl
{
class WindowDropTarget : public wxFileDropTarget
{
public:
    WindowDropTarget(wxWindow& dropWindow) : dropWindow_(dropWindow) {}

private:
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileArray) override
    {
        //wxPoint clientDropPos(x, y)
        std::vector<Zstring> filePaths;
        for (const wxString& file : fileArray)
            filePaths.push_back(utfCvrtTo<Zstring>(file));

        //create a custom event on drop window: execute event after file dropping is completed! (after mouse is released)
        if (wxEvtHandler* handler = dropWindow_.GetEventHandler())
            handler->AddPendingEvent(FileDropEvent(filePaths));
        return true;
    }

    wxWindow& dropWindow_;
};
}


inline
void setupFileDrop(wxWindow& wnd) { wnd.SetDropTarget(new impl::WindowDropTarget(wnd)); /*takes ownership*/ }
}

#endif //FILE_DROP_H_09457802957842560325626
