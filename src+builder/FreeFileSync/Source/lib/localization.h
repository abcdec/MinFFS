// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef LOCALIZATION_H_8917342083178321534
#define LOCALIZATION_H_8917342083178321534

#include <vector>
#include <zen/file_error.h>

namespace zen
{
class ExistingTranslations
{
public:
    struct Entry
    {
        int languageID;
        std::wstring languageName;
        std::wstring languageFile;
        std::wstring translatorName;
        std::wstring languageFlag;
    };
    static const std::vector<Entry>& get();

private:
    ExistingTranslations();
    ExistingTranslations           (const ExistingTranslations&) = delete;
    ExistingTranslations& operator=(const ExistingTranslations&) = delete;
    std::vector<Entry> locMapping;
};


void setLanguage(int language); //throw FileError
int getLanguage();
int retrieveSystemLanguage();

void releaseWxLocale(); //wxLocale crashes miserably on wxGTK when destructor runs during global cleanup => call in wxApp::OnExit
//"You should delete all wxWidgets object that you created by the time OnExit finishes. In particular, do not destroy them from application class' destructor!"
}

#endif //LOCALIZATION_H_8917342083178321534
