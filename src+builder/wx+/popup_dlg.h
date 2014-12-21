// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef MESSAGEPOPUP_H_820780154723456
#define MESSAGEPOPUP_H_820780154723456

#include <wx/window.h>
#include <wx/string.h>

namespace zen
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems
//this module requires error, warning and info image files in resources.zip, see <wx+/image_resources.h>

struct PopupDialogCfg;
struct PopupDialogCfg3;

enum class DialogInfoType
{
    INFO,
    WARNING,
    ERROR2, //fuck the ERROR macro in WinGDI.h!
};

enum class ConfirmationButton3
{
    DO_IT,
    DONT_DO_IT,
    CANCEL
};

enum class ConfirmationButton
{
    DO_IT  = static_cast<int>(ConfirmationButton3::DO_IT), //[!]
    CANCEL = static_cast<int>(ConfirmationButton3::CANCEL), //Clang requires a "static_cast"
};

void                showNotificationDialog (wxWindow* parent, DialogInfoType type, const PopupDialogCfg&  cfg);
ConfirmationButton  showConfirmationDialog (wxWindow* parent, DialogInfoType type, const PopupDialogCfg&  cfg, const wxString& labelDoIt);
ConfirmationButton3 showConfirmationDialog3(wxWindow* parent, DialogInfoType type, const PopupDialogCfg3& cfg, const wxString& labelDoIt, const wxString& labelDontDoIt);

//----------------------------------------------------------------------------------------------------------------
class StandardPopupDialog;
class ConfirmationDialog3;

struct PopupDialogCfg
{
    PopupDialogCfg() : checkBoxValue() {}
    PopupDialogCfg& setTitle             (const wxString& label) { title      = label; return *this; }
    PopupDialogCfg& setMainInstructions  (const wxString& label) { textMain   = label; return *this; } //set at least one of these!
    PopupDialogCfg& setDetailInstructions(const wxString& label) { textDetail = label; return *this; } //
    PopupDialogCfg& setCheckBox(bool& value, const wxString& label) { checkBoxValue = &value; checkBoxLabel = label; return *this; }

private:
    friend class StandardPopupDialog;

    wxString title;
    wxString textMain;
    wxString textDetail;
    bool* checkBoxValue; //in/out
    wxString checkBoxLabel;
};


struct PopupDialogCfg3
{
    PopupDialogCfg3() : buttonToDisableWhenChecked(ConfirmationButton3::CANCEL) {}
    PopupDialogCfg3& setTitle             (const wxString& label) { pdCfg.setTitle             (label); return *this; }
    PopupDialogCfg3& setMainInstructions  (const wxString& label) { pdCfg.setMainInstructions  (label); return *this; } //set at least one of these!
    PopupDialogCfg3& setDetailInstructions(const wxString& label) { pdCfg.setDetailInstructions(label); return *this; } //
    PopupDialogCfg3& setCheckBox(bool& value, const wxString& label) { pdCfg.setCheckBox(value, label); return *this; }
    PopupDialogCfg3& setCheckBox(bool& value, const wxString& label, ConfirmationButton3 disableWhenChecked)
    {
        assert(disableWhenChecked != ConfirmationButton3::CANCEL);
        setCheckBox(value, label);
        buttonToDisableWhenChecked = disableWhenChecked;
        return *this;
    }

private:
    friend class ConfirmationDialog3;

    PopupDialogCfg pdCfg;
    ConfirmationButton3 buttonToDisableWhenChecked;
};
}

#endif //MESSAGEPOPUP_H_820780154723456
