// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef CUSTOMCOMBOBOX_H_INCLUDED
#define CUSTOMCOMBOBOX_H_INCLUDED

#include <wx/combobox.h>
#include <memory>
#include <zen/zstring.h>
#include <zen/stl_tools.h>
#include <zen/utf.h>

//combobox with history function + functionality to delete items (DEL)


class FolderHistory
{
public:
    FolderHistory() : maxSize_(0) {}

    FolderHistory(const std::vector<Zstring>& dirpaths, size_t maxSize) :
        maxSize_(maxSize),
        dirpaths_(dirpaths)
    {
        if (dirpaths_.size() > maxSize_) //keep maximal size of history list
            dirpaths_.resize(maxSize_);
    }

    const std::vector<Zstring>& getList() const { return dirpaths_; }

    static const wxString separationLine() { return L"---------------------------------------------------------------------------------------------------------------"; }

    void addItem(const Zstring& dirpath)
    {
        if (dirpath.empty() || dirpath == zen::utfCvrtTo<Zstring>(separationLine()))
            return;

        Zstring nameTmp = dirpath;
        zen::trim(nameTmp);

        //insert new folder or put it to the front if already existing
        zen::vector_remove_if(dirpaths_, [&](const Zstring& item) { return ::EqualFilename()(item, nameTmp); });

        dirpaths_.insert(dirpaths_.begin(), nameTmp);

        if (dirpaths_.size() > maxSize_) //keep maximal size of history list
            dirpaths_.resize(maxSize_);
    }

    void delItem(const Zstring& dirpath) { zen::vector_remove_if(dirpaths_, [&](const Zstring& item) { return ::EqualFilename()(item, dirpath); }); }

private:
    size_t maxSize_;
    std::vector<Zstring> dirpaths_;
};


class FolderHistoryBox : public wxComboBox
{
public:
    FolderHistoryBox(wxWindow* parent,
                     wxWindowID id,
                     const wxString& value = wxEmptyString,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     int n = 0,
                     const wxString choices[] = nullptr,
                     long style = 0,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxComboBoxNameStr);

    void init(const std::shared_ptr<FolderHistory>& sharedHistory) { sharedHistory_ = sharedHistory; }

    void setValue(const wxString& dirpath)
    {
        setValueAndUpdateList(dirpath); //required for setting value correctly; Linux: ensure the dropdown is shown as being populated
    }

    // GetValue

private:
    void OnKeyEvent(wxKeyEvent& event);
    void OnRequireHistoryUpdate(wxEvent& event);
    void setValueAndUpdateList(const wxString& dirpath);

    std::shared_ptr<FolderHistory> sharedHistory_;
};


#endif // CUSTOMCOMBOBOX_H_INCLUDED
