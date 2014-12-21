// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILE_DROP_H_INCLUDED
#define FILE_DROP_H_INCLUDED

#include <vector>
#include <wx/window.h>
#include <wx/event.h>
#include <wx/dnd.h>

namespace zen
{
//register simple file drop event (without issue of freezing dialogs and without wxFileDropTarget overdesign)
//CAVEAT: a drop target window must not be directly or indirectly contained within a wxStaticBoxSizer until the following wxGTK bug
//is fixed. According to wxWidgets release cycles this is expected to be: never http://trac.wxwidgets.org/ticket/2763

//1. setup a window to emit EVENT_DROP_FILE
void setupFileDrop(wxWindow& wnd);

//2. register events:
//wnd.Connect   (EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);
//wnd.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);

//3. do something:
//void MyDlg::OnFilesDropped(FileDropEvent& event);








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
    FileDropEvent(const std::vector<wxString>& filesDropped, const wxWindow& dropWindow, wxPoint dropPos) :
        wxCommandEvent(EVENT_DROP_FILE),
        filesDropped_(filesDropped),
        dropWindow_(dropWindow),
        dropPos_(dropPos) {}

    wxEvent* Clone() const override { return new FileDropEvent(*this); }

    const std::vector<wxString>& getFiles()        const { return filesDropped_; }
    const wxWindow&              getDropWindow()   const { return dropWindow_;   }
    wxPoint                      getDropPosition() const { return dropPos_;      } //position relative to drop window

private:
    const std::vector<wxString> filesDropped_;
    const wxWindow& dropWindow_;
    const wxPoint dropPos_;
};

typedef void (wxEvtHandler::*FileDropEventFunction)(FileDropEvent&);

#define FileDropEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(FileDropEventFunction, &func)


namespace impl
{
class WindowDropTarget : public wxFileDropTarget
{
public:
    WindowDropTarget(wxWindow& dropWindow) : dropWindow_(dropWindow) {}

private:
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileArray) override
    {
        std::vector<wxString> filepaths(fileArray.begin(), fileArray.end());
        if (!filepaths.empty())
            //create a custom event on drop window: execute event after file dropping is completed! (after mouse is released)
            if (wxEvtHandler* handler = dropWindow_.GetEventHandler())
                handler->AddPendingEvent(FileDropEvent(filepaths, dropWindow_, wxPoint(x, y)));
        return true;
    }

    wxWindow& dropWindow_;
};
}


inline
void setupFileDrop(wxWindow& wnd)
{
    wnd.SetDropTarget(new impl::WindowDropTarget(wnd)); //takes ownership
}
}

#endif // FILE_DROP_H_INCLUDED
