// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef HELPPROVIDER_H_INCLUDED
#define HELPPROVIDER_H_INCLUDED

#ifdef ZEN_WIN
    #include <zen/zstring.h>
    #include <wx/msw/helpchm.h>

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <wx/html/helpctrl.h>
#endif

#include "ffs_paths.h"

namespace zen
{
//use '/' as path separator!
void displayHelpEntry(wxWindow* parent);
void displayHelpEntry(const wxString& section, wxWindow* parent);









//######################## implementation ########################
namespace impl
{
//finish wxWidgets' job
#ifdef ZEN_WIN
class FfsHelpController
{
public:
    FfsHelpController()
    {
        chmHlp.Initialize(utfCvrtTo<wxString>(zen::getResourceDir()) + L"FreeFileSync.chm");
    }

    void openSection(const wxString& section, wxWindow* parent)
    {
        if (section.empty())
            chmHlp.DisplayContents();
        else
            chmHlp.DisplaySection(replaceCpy(section, L'/', utfCvrtTo<wxString>(FILE_NAME_SEPARATOR)));
    }
private:
    wxCHMHelpController chmHlp;
};

#elif defined ZEN_LINUX || defined ZEN_MAC
class FfsHelpController
{
public:
    void openSection(const wxString& section, wxWindow* parent)
    {
        wxHtmlModalHelp dlg(parent, utfCvrtTo<wxString>(zen::getResourceDir()) + L"Help/FreeFileSync.hhp", section,
                            wxHF_DEFAULT_STYLE | wxHF_DIALOG | wxHF_MODAL | wxHF_MERGE_BOOKS);
        (void)dlg;
        //-> solves modal help craziness on OSX!
        //-> Suse Linux: avoids program hang on exit if user closed help parent dialog before the help dialog itself was closed (why is this even possible???)
        //               avoids ESC key not being recognized by help dialog (but by parent dialog instead)
    }
};
#endif


inline
FfsHelpController& getHelpCtrl()
{
    static FfsHelpController ctrl; //external linkage, despite inline definition!
    return ctrl;
}
}


inline
void displayHelpEntry(const wxString& section, wxWindow* parent)
{
    impl::getHelpCtrl().openSection(section, parent);
}


inline
void displayHelpEntry(wxWindow* parent)
{
    impl::getHelpCtrl().openSection(wxString(), parent);
}
}

#endif //HELPPROVIDER_H_INCLUDED
