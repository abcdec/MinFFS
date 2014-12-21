// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "popup_dlg.h"
#include <wx/app.h>
#include <wx+/std_button_layout.h>
#include <wx+/font_size.h>
#include <wx+/image_resources.h>
#include "popup_dlg_generated.h"

#ifdef ZEN_WIN
    #include <wx+/mouse_move_dlg.h>
#endif

using namespace zen;

namespace
{
void setAsStandard(wxButton& btn)
{
    btn.SetDefault();
    btn.SetFocus();
}


void setBestInitialSize(wxTextCtrl& ctrl, const wxString& text, wxSize maxSize)
{
    const int scrollbarWidth = 30;
    if (maxSize.x <= scrollbarWidth) //implicitly checks for non-zero, too!
        return;
    maxSize.x -= scrollbarWidth;

    int bestWidth = 0;
    int rowCount  = 0;
    int rowHeight = 0;

    auto evalLineExtent = [&](const wxSize& sz) -> bool //return true when done
    {
        if (sz.x > bestWidth)
            bestWidth = std::min(maxSize.x, sz.x);

        rowCount += (sz.x + maxSize.x - 1) / maxSize.x; //integer round up: consider line-wraps!
        rowHeight = std::max(rowHeight, sz.y); //all rows *should* have same height

        return rowCount * rowHeight >= maxSize.y;
    };

    for (auto it = text.begin();;)
    {
        auto itEnd = std::find(it, text.end(), L'\n');
        wxString line(it, itEnd);
        if (line.empty())
            line = L" "; //GetTextExtent() returns (0, 0) for empty strings!

        wxSize sz = ctrl.GetTextExtent(line); //exactly gives row height, but does *not* consider newlines
        if (evalLineExtent(sz))
            break;

        if (itEnd == text.end())
            break;
        it = itEnd + 1;
    }

#if defined ZEN_WIN || defined ZEN_LINUX
    const int rowGap = 0;
#elif defined ZEN_MAC
    const int rowGap = 1;
#endif
    const wxSize bestSize(bestWidth + scrollbarWidth, std::min(rowCount * (rowHeight + rowGap), maxSize.y));
    ctrl.SetMinSize(bestSize); //alas, SetMinClientSize() is just not working!
}
}


class zen::StandardPopupDialog : public PopupDialogGenerated
{
public:
    StandardPopupDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg) :
        PopupDialogGenerated(parent),
        checkBoxValue_(cfg.checkBoxValue)
    {
#ifdef ZEN_WIN
        new zen::MouseMoveWindow(*this); //allow moving main dialog by clicking (nearly) anywhere...; ownership passed to "this"
#endif
        wxString titleTmp = cfg.title;
        switch (type)
        {
            case DialogInfoType::INFO:
                //"information" is meaningless as caption text!
                //confirmation doesn't use info icon
                //m_bitmapMsgType->Hide();
                //m_bitmapMsgType->SetSize(30, -1);
                //m_bitmapMsgType->SetBitmap(getResourceImage(L"msg_info"));
                break;
            case DialogInfoType::WARNING:
                if (titleTmp.empty()) titleTmp = _("Warning");
                m_bitmapMsgType->SetBitmap(getResourceImage(L"msg_warning"));
                break;
            case DialogInfoType::ERROR2:
                if (titleTmp.empty()) titleTmp = _("Error");
                m_bitmapMsgType->SetBitmap(getResourceImage(L"msg_error"));
                break;
        }

        if (titleTmp.empty())
            SetTitle(wxTheApp->GetAppDisplayName());
        else
        {
            if (parent && parent->IsShownOnScreen())
                SetTitle(titleTmp);
            else
                SetTitle(wxTheApp->GetAppDisplayName() + L" - " + titleTmp);
        }

        const wxSize maxSize(500, 380);

        assert(!cfg.textMain.empty() || !cfg.textDetail.empty());
        if (!cfg.textMain.empty())
        {
            setMainInstructionFont(*m_staticTextMain);
            m_staticTextMain->SetLabel(cfg.textMain);
            m_staticTextMain->Wrap(maxSize.GetWidth()); //call *after* SetLabel()
        }
        else
            m_staticTextMain->Hide();

        if (!cfg.textDetail.empty())
        {
            const wxString& text = L"\n" + cfg.textDetail + L"\n"; //add empty top/bottom lines *instead* of using border space!
            setBestInitialSize(*m_textCtrlTextDetail, text, maxSize);
            m_textCtrlTextDetail->ChangeValue(text);
        }
        else
            m_textCtrlTextDetail->Hide();

        if (checkBoxValue_)
        {
            assert(contains(cfg.checkBoxLabel, L"&"));
            m_checkBoxCustom->SetLabel(cfg.checkBoxLabel);
            m_checkBoxCustom->SetValue(*checkBoxValue_);
        }
        else
            m_checkBoxCustom->Hide();

        Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(StandardPopupDialog::OnKeyPressed), nullptr, this);
    }

private:
    void OnClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton3::CANCEL)); }
    void OnCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton3::CANCEL)); }

    void OnKeyPressed(wxKeyEvent& event)
    {
        const int keyCode = event.GetKeyCode();
        if (keyCode == WXK_ESCAPE) //handle case where cancel button is hidden!
        {
            EndModal(static_cast<int>(ConfirmationButton3::CANCEL));
            return;
        }
        event.Skip();
    }

    void OnButtonAffirmative(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            * checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::DO_IT));
    }

    void OnButtonNegative(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            * checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::DONT_DO_IT));
    }

    bool* checkBoxValue_;
};


namespace
{
class NotificationDialog : public StandardPopupDialog
{
public:
    NotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg) :
        StandardPopupDialog(parent, type, cfg)
    {
        m_buttonAffirmative->SetLabel(_("Close")); //UX Guide: use "Close" for errors, warnings and windows in which users can't make changes (no ampersand!)
        m_buttonNegative->Hide();
        m_buttonCancel->Hide();

        //set std order after button visibility was set
        setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonAffirmative));
        setAsStandard(*m_buttonAffirmative);
        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    }
};


class ConfirmationDialog : public StandardPopupDialog
{
public:
    ConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelDoIt) :
        StandardPopupDialog(parent, type, cfg)
    {
        assert(contains(labelDoIt, L"&"));
        m_buttonAffirmative->SetLabel(labelDoIt);
        m_buttonNegative->Hide();

        //set std order after button visibility was set
        setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonAffirmative).setCancel(m_buttonCancel));
        setAsStandard(*m_buttonAffirmative);
        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    }
};
}

class zen::ConfirmationDialog3 : public StandardPopupDialog
{
public:
    ConfirmationDialog3(wxWindow* parent, DialogInfoType type, const PopupDialogCfg3& cfg, const wxString& labelDoIt, const wxString& labelDontDoIt) :
        StandardPopupDialog(parent, type, cfg.pdCfg),
        buttonToDisableWhenChecked(cfg.buttonToDisableWhenChecked)
    {
        assert(contains(labelDoIt,     L"&"));
        assert(contains(labelDontDoIt, L"&"));
        m_buttonAffirmative->SetLabel(labelDoIt);
        m_buttonNegative   ->SetLabel(labelDontDoIt);

        //m_buttonAffirmative->SetId(wxID_IGNORE); -> setting id after button creation breaks "mouse snap to" functionality
        //m_buttonNegative   ->SetId(wxID_RETRY);  -> also wxWidgets docs seem to hide some info: "Normally, the identifier should be provided on creation and should not be modified subsequently."

        updateGui();

        //set std order after button visibility was set
        setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonAffirmative).setNegative(m_buttonNegative).setCancel(m_buttonCancel));
        setAsStandard(*m_buttonAffirmative);
        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    }

private:
    void OnCheckBoxClick(wxCommandEvent& event) override { updateGui(); event.Skip(); }

    void updateGui()
    {
        switch (buttonToDisableWhenChecked)
        {
            case ConfirmationButton3::DO_IT:
                m_buttonAffirmative->Enable(!m_checkBoxCustom->GetValue());
                break;
            case ConfirmationButton3::DONT_DO_IT:
                m_buttonNegative->Enable(!m_checkBoxCustom->GetValue());
                break;
            case ConfirmationButton3::CANCEL:
                break;
        }
    }

    const ConfirmationButton3 buttonToDisableWhenChecked;
};

//########################################################################################

void zen::showNotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg)
{
    NotificationDialog dlg(parent, type, cfg);
    dlg.ShowModal();
}


ConfirmationButton zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelDoIt)
{
    ConfirmationDialog dlg(parent, type, cfg, labelDoIt);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}


ConfirmationButton3 zen::showConfirmationDialog3(wxWindow* parent, DialogInfoType type, const PopupDialogCfg3& cfg, const wxString& labelDoIt, const wxString& labelDontDoIt)
{
    ConfirmationDialog3 dlg(parent, type, cfg, labelDoIt, labelDontDoIt);
    return static_cast<ConfirmationButton3>(dlg.ShowModal());
}
