// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef EXEC_FINISHED_BOX_18947773210473214
#define EXEC_FINISHED_BOX_18947773210473214

#include <vector>
#include <string>
#include <map>
#include <wx/combobox.h>
#include <zen/string_tools.h>
#include <zen/zstring.h>

//combobox with history function + functionality to delete items (DEL)

//special command
bool isCloseProgressDlgCommand(const Zstring& value);


class OnCompletionBox : public wxComboBox
{
public:
    OnCompletionBox(wxWindow* parent,
                    wxWindowID id,
                    const wxString& value = wxEmptyString,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    int n = 0,
                    const wxString choices[] = nullptr,
                    long style = 0,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxComboBoxNameStr);

    void initHistory(std::vector<Zstring>& history, size_t historyMax) { history_ = &history; historyMax_ = historyMax; }
    void addItemHistory(); //adds current item to history

    // use these two accessors instead of GetValue()/SetValue():
    Zstring getValue() const;
    void setValue(const Zstring& value);
    //required for setting value correctly + Linux to ensure the dropdown is shown as being populated

private:
    void OnKeyEvent(wxKeyEvent& event);
    void OnMouseWheel(wxMouseEvent& event) {} //swallow! this gives confusing UI feedback anyway
    void OnSelection(wxCommandEvent& event);
    void OnValidateSelection(wxCommandEvent& event);
    void OnUpdateList(wxEvent& event);

    void setValueAndUpdateList(const std::wstring& value);

    std::vector<Zstring>* history_;
    size_t historyMax_;

    const std::vector<std::pair<std::wstring, Zstring>> defaultCommands;
};


#endif //EXEC_FINISHED_BOX_18947773210473214
