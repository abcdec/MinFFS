// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "tooltip.h"
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/settings.h>
#include <wx/app.h>
#include <wx+/image_tools.h>

using namespace zen;


class Tooltip::TooltipDialogGenerated : public wxDialog
{
public:
    TooltipDialogGenerated(wxWindow* parent,
                           wxWindowID id = wxID_ANY,
                           const wxString& title = wxEmptyString,
                           const wxPoint& pos = wxDefaultPosition,
                           const wxSize& size = wxDefaultSize,
                           long style = 0) : wxDialog(parent, id, title, pos, size, style)
    {
        //Suse Linux/X11: needs parent window, else there are z-order issues

        this->SetSizeHints(wxDefaultSize, wxDefaultSize);
        this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));   //both required: on Ubuntu background is black, foreground white!
        this->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT)); //

        wxBoxSizer* bSizer158 = new wxBoxSizer(wxHORIZONTAL);
        m_bitmapLeft = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(m_bitmapLeft, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

        m_staticTextMain = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(m_staticTextMain, 0, wxALL | wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 5);

        this->SetSizer(bSizer158);
        this->Layout();
        bSizer158->Fit(this);

#ifdef ZEN_WIN //prevent window from stealing focus!
        Disable(); //= dark/grey text and image on Linux; no visible difference on OS X
#endif
    }

    wxStaticText* m_staticTextMain;
    wxStaticBitmap* m_bitmapLeft;
};


void Tooltip::show(const wxString& text, wxPoint mousePos, const wxBitmap* bmp)
{
    if (!tipWindow)
        tipWindow = new TooltipDialogGenerated(&parent_); //ownership passed to parent

    const wxBitmap& newBmp = bmp ? *bmp : wxNullBitmap;

    if (!isEqual(tipWindow->m_bitmapLeft->GetBitmap(), newBmp))
    {
        tipWindow->m_bitmapLeft->SetBitmap(newBmp);
        tipWindow->Refresh(); //needed if bitmap size changed!
    }

    if (text != tipWindow->m_staticTextMain->GetLabel())
    {
        tipWindow->m_staticTextMain->SetLabel(text);
        tipWindow->m_staticTextMain->Wrap(600);
    }

    tipWindow->GetSizer()->SetSizeHints(tipWindow); //~=Fit() + SetMinSize()
    //Linux: Fit() seems to be somewhat broken => this needs to be called EVERY time inside show, not only if text or bmp change

    const wxPoint newPos = wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ?
                           mousePos - wxPoint(30 + tipWindow->GetSize().GetWidth(), 0) :
                           mousePos + wxPoint(30, 0);

    if (newPos != tipWindow->GetScreenPosition())
        tipWindow->SetSize(newPos.x, newPos.y, wxDefaultCoord, wxDefaultCoord);
    //attention!!! possible endless loop: mouse pointer must NOT be within tipWindow!
    //else it will trigger a wxEVT_LEAVE_WINDOW on middle grid which will hide the window, causing the window to be shown again via this method, etc.

    if (!tipWindow->IsShown())
        tipWindow->Show();
}


void Tooltip::hide()
{
    if (tipWindow)
    {
#ifdef ZEN_LINUX
        //on wxGTK the tooltip is sometimes not shown again after it was hidden: e.g. drag-selection on middle grid
        tipWindow->Destroy(); //apply brute force:
        tipWindow = nullptr;  //
#else
        tipWindow->Hide();
#endif
    }
}
