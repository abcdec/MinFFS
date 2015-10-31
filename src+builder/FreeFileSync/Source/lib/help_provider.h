// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef HELPPROVIDER_H_85930427583421563126
#define HELPPROVIDER_H_85930427583421563126

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

void uninitializeHelp(); //clean up gracefully during app shutdown: leaving this up to static destruction crashes on Win 8.1!






//######################## implementation ########################
namespace impl
{
//finish wxWidgets' job:
#ifdef ZEN_WIN
class FfsHelpController
{
public:
    static FfsHelpController& getInstance()
    {
        static FfsHelpController inst; //external linkage, despite inline definition!
        return inst;
    }

    void openSection(const wxString& section, wxWindow* parent)
    {
        init();
        if (section.empty())
            chmHlp->DisplayContents();
        else
            chmHlp->DisplaySection(replaceCpy(section, L'/', utfCvrtTo<wxString>(FILE_NAME_SEPARATOR)));
    }

    void uninitialize()
    {
        if (chmHlp)
        {
            chmHlp->Quit(); //don't let help windows open while app is shut down! => crash on Win 8.1!
            chmHlp.reset();
        }
    }

private:
    FfsHelpController() {}
    ~FfsHelpController() { assert(!chmHlp); }

    void init() //don't put in constructor: not needed if only uninitialize() is ever called!
    {
        if (!chmHlp)
        {
            chmHlp = std::make_unique<wxCHMHelpController>();
            chmHlp->Initialize(utfCvrtTo<wxString>(zen::getResourceDir()) + L"FreeFileSync.chm");
        }
    }

    std::unique_ptr<wxCHMHelpController> chmHlp;
};

#elif defined ZEN_LINUX || defined ZEN_MAC
class FfsHelpController
{
public:
    static FfsHelpController& getInstance()
    {
        static FfsHelpController inst;
        return inst;
    }

    void uninitialize() {}

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
}


inline
void displayHelpEntry(const wxString& section, wxWindow* parent)
{
    impl::FfsHelpController::getInstance().openSection(section, parent);
}


inline
void displayHelpEntry(wxWindow* parent)
{
    impl::FfsHelpController::getInstance().openSection(wxString(), parent);
}

inline
void uninitializeHelp()
{
    impl::FfsHelpController::getInstance().uninitialize();

}
}

#endif //HELPPROVIDER_H_85930427583421563126
