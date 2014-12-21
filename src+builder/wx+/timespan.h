// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef WX_TIMESPAN_CTRL_HEADER_INCLUDED
#define WX_TIMESPAN_CTRL_HEADER_INCLUDED

#include <wx/textctrl.h>
#include <wx/datetime.h>
#include <wx/spinbutt.h>
#include <wx/sizer.h>

//user friendly time span control
//- constructor is compatible with a wxTextControl
//- emits change event: wxEVT_TIMESPAN_CHANGE

namespace zen
{
inline
wxEventType getEventType()
{
    static wxEventType evt = wxNewEventType(); //external linkage!
    return evt;
}
const wxEventType wxEVT_TIMESPAN_CHANGE = getEventType();


class TimeSpanCtrl : public wxPanel
{
public:
    TimeSpanCtrl(wxWindow* parent, wxWindowID id,
                 const wxString& value = wxEmptyString,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxTextCtrlNameStr) :
        wxPanel(parent, id, pos, size, style, name),
        FORMAT_TIMESPAN(wxT("%H:%M:%S"))
    {
        wxBoxSizer* bSizer27 = new wxBoxSizer( wxHORIZONTAL );

        m_textCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_CENTRE );
        bSizer27->Add(m_textCtrl, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND, 5 );

        m_spinBtn = new wxSpinButton(this, wxID_ANY, wxDefaultPosition, wxSize( 20, -1 ), wxSP_ARROW_KEYS );
        bSizer27->Add(m_spinBtn, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND, 5 );

        SetSizer(bSizer27);
        Layout();

        //connect events
        m_spinBtn ->Connect(wxEVT_SCROLL_LINEUP,   wxEventHandler     (TimeSpanCtrl::OnSpinUp),      nullptr, this);
        m_spinBtn ->Connect(wxEVT_SCROLL_LINEDOWN, wxEventHandler     (TimeSpanCtrl::OnSpinDown),    nullptr, this);
        m_textCtrl->Connect(wxEVT_KEY_DOWN,        wxKeyEventHandler  (TimeSpanCtrl::OnKeyPress),    nullptr, this);
        m_textCtrl->Connect(wxEVT_MOUSEWHEEL,      wxMouseEventHandler(TimeSpanCtrl::OnMouseAction), nullptr, this);

        setValue(0);
    }

    void setValue(int span) //unit: [s]
    {
        wxString newValue;
        if (span < 0)
        {
            newValue += wxT("- ");
            span = -span;
        }
        newValue += wxTimeSpan::Seconds(span).Format(FORMAT_TIMESPAN);

        long pos = m_textCtrl->GetInsertionPoint();
        pos += newValue.size() - m_textCtrl->GetValue().size();

        m_textCtrl->ChangeValue(newValue);
        m_textCtrl->SetInsertionPoint(pos);

        wxCommandEvent chgEvent(wxEVT_TIMESPAN_CHANGE);
        wxPostEvent(this, chgEvent);
    }

    int getValue() const
    {
        wxString textVal = m_textCtrl->GetValue();
        textVal.Trim(false);

        bool isNegative = false;
        if (textVal.StartsWith(wxT("-")))
        {
            isNegative = true;
            textVal = textVal.substr(1);
        }
        textVal.Trim(false);

        wxDateTime tmp(time_t(0));
        if (tmp.ParseFormat(textVal, FORMAT_TIMESPAN, wxDateTime(tmp)) == nullptr)
            return 0;

        return (isNegative ? -1 : 1) *
               (tmp.GetHour  () * 3600 +
                tmp.GetMinute() *   60 +
                tmp.GetSecond());
    }

private:
    void OnSpinUp  (wxEvent& event) { spinValue(true); }
    void OnSpinDown(wxEvent& event) { spinValue(false); }

    void OnKeyPress(wxKeyEvent& event)
    {
        const int keyCode = event.GetKeyCode();
        switch (keyCode)
        {
            case WXK_UP:
            case WXK_NUMPAD_UP:
                return spinValue(true);
            case WXK_DOWN:
            case WXK_NUMPAD_DOWN:
                return spinValue(false);
            default:
                event.Skip();
        }
    }

    void OnMouseAction(wxMouseEvent& event)
    {
        int delta = event.GetWheelRotation();
        if (delta > 0)
            spinValue(true);
        else if (delta < 0)
            spinValue(false);
        else
            event.Skip();
    }

    void spinValue(bool up)
    {
        wxString textval = m_textCtrl->GetValue();
        long pos = m_textCtrl->GetInsertionPoint();

        int stepSize = 1;
        if (pos <= static_cast<long>(textval.size()))
        {
            int delimCount = std::count(textval.begin() + pos, textval.end(), wxT(':'));
            if (delimCount == 1)
                stepSize = 60; //minute
            else if (delimCount == 2)
                stepSize = 3600; //hour
        }

        if (!up)
            stepSize *= -1;

        setValue(getValue() + stepSize);
    }

    wxTextCtrl*   m_textCtrl;
    wxSpinButton* m_spinBtn;

    const wxString FORMAT_TIMESPAN;
};
}


#endif //WX_TIMESPAN_CTRL_HEADER_INCLUDED
