// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "on_completion_box.h"
#include <deque>
#include <zen/i18n.h>
#include <algorithm>
#include <zen/stl_tools.h>
#include <zen/utf.h>
#ifdef ZEN_WIN
    #include <zen/win_ver.h>
#endif

using namespace zen;


namespace
{
const std::wstring cmdTxtCloseProgressDlg = L"Close progress dialog"; //special command //mark for extraction: _("Close progress dialog")

const std::wstring separationLine(L"---------------------------------------------------------------------------------------------------------------");


std::vector<std::pair<std::wstring, Zstring>> getDefaultCommands() //(gui name/command) pairs
{
    std::vector<std::pair<std::wstring, Zstring>> output;

    auto addEntry = [&](const std::wstring& name, const Zstring& value) { output.emplace_back(name, value); };

#ifdef ZEN_WIN
    if (zen::vistaOrLater())
    {
        addEntry(_("Log off"  ), Zstr("shutdown /l"));
        addEntry(_("Standby"  ), Zstr("rundll32.exe powrprof.dll,SetSuspendState Sleep")); //suspend/Suspend to RAM/sleep
        addEntry(_("Shut down"), Zstr("shutdown /s /t 60"));
    }
    else //XP
    {
        addEntry(_("Log off"  ), Zstr("shutdown -l"));
        addEntry(_("Standby"  ), Zstr("rundll32.exe powrprof.dll,SetSuspendState")); //this triggers standby OR hibernate, depending on whether hibernate setting is active!
        addEntry(_("Shut down"), Zstr("shutdown -s -t 60"));
        //no suspend on XP?
    }

#elif defined ZEN_LINUX
    addEntry(_("Log off"  ), Zstr("gnome-session-quit")); //alternative requiring admin: sudo killall Xorg
    addEntry(_("Standby"  ), Zstr("sudo pm-suspend"));
    addEntry(_("Shut down"), Zstr("dbus-send --print-reply --dest=org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager.RequestShutdown"));
    //alternative requiring admin: sudo shutdown -h 1

#elif defined ZEN_MAC
    addEntry(_("Log off"  ), Zstr("osascript -e \'tell application \"System Events\" to log out\'"));
    addEntry(_("Standby"  ), Zstr("osascript -e \'tell application \"System Events\" to sleep\'"));
    addEntry(_("Shut down"), Zstr("osascript -e \'tell application \"System Events\" to shut down\'"));
#endif
    return output;
}

const wxEventType wxEVT_VALIDATE_USER_SELECTION = wxNewEventType();
}


bool isCloseProgressDlgCommand(const Zstring& value)
{
    return trimCpy(utfCvrtTo<std::wstring>(value)) == cmdTxtCloseProgressDlg;
}


OnCompletionBox::OnCompletionBox(wxWindow* parent,
                                 wxWindowID id,
                                 const wxString& value,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 int n,
                                 const wxString choices[],
                                 long style,
                                 const wxValidator& validator,
                                 const wxString& name) :
    wxComboBox(parent, id, value, pos, size, n, choices, style, validator, name),
    defaultCommands(getDefaultCommands())
{
    //#####################################
    /*##*/ SetMinSize(wxSize(150, -1)); //## workaround yet another wxWidgets bug: default minimum size is much too large for a wxComboBox
    //#####################################

    Connect(wxEVT_KEY_DOWN,                  wxKeyEventHandler    (OnCompletionBox::OnKeyEvent  ), nullptr, this);
    Connect(wxEVT_LEFT_DOWN,                 wxEventHandler       (OnCompletionBox::OnUpdateList), nullptr, this);
    Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(OnCompletionBox::OnSelection ), nullptr, this);
    Connect(wxEVT_MOUSEWHEEL,                wxMouseEventHandler  (OnCompletionBox::OnMouseWheel), nullptr, this);

    Connect(wxEVT_VALIDATE_USER_SELECTION, wxCommandEventHandler(OnCompletionBox::OnValidateSelection), nullptr, this);
}


void OnCompletionBox::addItemHistory()
{
    const Zstring command = trimCpy(getValue());

    if (command == utfCvrtTo<Zstring>(separationLine) || //do not add sep. line
        command == utfCvrtTo<Zstring>(cmdTxtCloseProgressDlg) || //do not add special command
        command.empty())
        return;

    //do not add built-in commands to history
    for (const auto& item : defaultCommands)
        if (command == utfCvrtTo<Zstring>(item.first) ||
            ::EqualFilePath()(command, item.second))
            return;

    erase_if(history_, [&](const Zstring& item) { return ::EqualFilePath()(command, item); });

    history_.insert(history_.begin(), command);

    if (history_.size() > historyMax_)
        history_.resize(historyMax_);
}


Zstring OnCompletionBox::getValue() const
{
    auto value = trimCpy(copyStringTo<std::wstring>(GetValue()));

    if (value == implementation::translate(cmdTxtCloseProgressDlg)) //undo translation for config file storage
        value = cmdTxtCloseProgressDlg;

    return utfCvrtTo<Zstring>(value);
}


void OnCompletionBox::setValue(const Zstring& value)
{
    auto tmp = trimCpy(utfCvrtTo<std::wstring>(value));

    if (tmp == cmdTxtCloseProgressDlg)
        tmp = implementation::translate(cmdTxtCloseProgressDlg); //have this symbolic constant translated properly

    setValueAndUpdateList(tmp);
}


//set value and update list are technically entangled: see potential bug description below
void OnCompletionBox::setValueAndUpdateList(const std::wstring& value)
{
    //it may be a little lame to update the list on each mouse-button click, but it should be working and we dont't have to manipulate wxComboBox internals

    std::deque<std::wstring> items;

    //1. special command
    items.push_back(implementation::translate(cmdTxtCloseProgressDlg));

    //2. built in commands
    for (const auto& item : defaultCommands)
        items.push_back(item.first);

    //3. history elements
    if (!history_.empty())
    {
        items.push_back(separationLine);
        for (const Zstring& hist : history_)
            items.push_back(utfCvrtTo<std::wstring>(hist));
        std::sort(items.end() - history_.size(), items.end());
    }

    //attention: if the target value is not part of the dropdown list, SetValue() will look for a string that *starts with* this value:
    //e.g. if the dropdown list contains "222" SetValue("22") will erroneously set and select "222" instead, while "111" would be set correctly!
    // -> by design on Windows!
    if (std::find(items.begin(), items.end(), value) == items.end())
    {
        if (!value.empty())
            items.push_front(separationLine);
        items.push_front(value);
    }

    //this->Clear(); -> NO! emits yet another wxEVT_COMMAND_TEXT_UPDATED!!!
    wxItemContainer::Clear(); //suffices to clear the selection items only!

    for (const std::wstring& item : items)
        this->Append(item);
    //this->SetSelection(wxNOT_FOUND); //don't select anything
    ChangeValue(value); //preserve main text!
}


void OnCompletionBox::OnSelection(wxCommandEvent& event)
{
    wxCommandEvent dummy2(wxEVT_VALIDATE_USER_SELECTION); //we cannot replace built-in commands at this position in call stack, so defer to a later time!
    if (auto handler = GetEventHandler())
        handler->AddPendingEvent(dummy2);

    event.Skip();
}


void OnCompletionBox::OnValidateSelection(wxCommandEvent& event)
{
    const auto value = copyStringTo<std::wstring>(GetValue());

    if (value == separationLine)
        return setValueAndUpdateList(std::wstring());

    for (const auto& item : defaultCommands)
        if (item.first == value)
            return setValueAndUpdateList(utfCvrtTo<std::wstring>(item.second)); //replace GUI name by actual command string
}


void OnCompletionBox::OnUpdateList(wxEvent& event)
{
    setValue(getValue());
    event.Skip();
}


void OnCompletionBox::OnKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    switch (keyCode)
    {
        case WXK_DELETE:
        case WXK_NUMPAD_DELETE:
        {
            //try to delete the currently selected config history item
            int pos = this->GetCurrentSelection();
            if (0 <= pos && pos < static_cast<int>(this->GetCount()) &&
                //what a mess...:
                (GetValue() != GetString(pos) || //avoid problems when a character shall be deleted instead of list item
                 GetValue() == wxEmptyString)) //exception: always allow removing empty entry
            {
                const auto selValue = utfCvrtTo<Zstring>(GetString(pos));

                if (std::find(history_.begin(), history_.end(), selValue) != history_.end()) //only history elements may be deleted
                {
                    //save old (selected) value: deletion seems to have influence on this
                    const wxString currentVal = this->GetValue();
                    //this->SetSelection(wxNOT_FOUND);

                    //delete selected row
                    erase_if(history_, [&](const Zstring& item) { return item == selValue; });

                    SetString(pos, wxString()); //in contrast to Delete(), this one does not kill the drop-down list and gives a nice visual feedback!
                    //Delete(pos);

                    //(re-)set value
                    SetValue(currentVal);
                }
                return; //eat up key event
            }
        }
        break;

        case WXK_UP:
        case WXK_NUMPAD_UP:
        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
        case WXK_PAGEUP:
        case WXK_NUMPAD_PAGEUP:
        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            return; //swallow -> using these keys gives a weird effect due to this weird control
    }

#ifdef ZEN_MAC
    //copy/paste is broken on wxCocoa: http://trac.wxwidgets.org/ticket/14953 => implement manually:
    assert(CanCopy() && CanPaste() && CanCut());
    if (event.ControlDown())
        switch (keyCode)
        {
            case 'C': //Command + C
                Copy();
                return;
            case 'V': //Command + V
                Paste();
                return;
            case 'X': //Command + X
                Cut();
                return;
        }
#endif

    event.Skip();
}
