// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "localization.h"
#include <unordered_map>
#include <map>
#include <list>
#include <iterator>
#include <zen/string_tools.h>
#include <zen/file_traverser.h>
#include <zen/serialize.h>
#include <zen/i18n.h>
#include <zen/format_unit.h>
#include <wx/intl.h>
#include <wx/log.h>
#include "parse_plural.h"
#include "parse_lng.h"
#include "ffs_paths.h"

#ifdef ZEN_LINUX
    #include <wchar.h> //wcscasecmp

#elif defined ZEN_MAC
    #include <zen/osx_string.h>
    #include <CoreServices/CoreServices.h>
#endif

using namespace zen;


namespace
{
class FFSTranslation : public TranslationHandler
{
public:
    FFSTranslation(const Zstring& filepath, wxLanguage languageId); //throw lngfile::ParsingError, parse_plural::ParsingError

    wxLanguage langId() const { return langId_; }

    std::wstring translate(const std::wstring& text) const override
    {
        //look for translation in buffer table
        auto it = transMapping.find(text);
        if (it != transMapping.end() && !it->second.empty())
            return it->second;
        return text; //fallback
    }

    std::wstring translate(const std::wstring& singular, const std::wstring& plural, std::int64_t n) const override
    {
        auto it = transMappingPl.find(std::make_pair(singular, plural));
        if (it != transMappingPl.end())
        {
            const size_t formNo = pluralParser->getForm(n);
            if (formNo < it->second.size())
                return replaceCpy(it->second[formNo], L"%x", toGuiString(n));
        }
        return replaceCpy(std::abs(n) == 1 ? singular : plural, L"%x", toGuiString(n)); //fallback
    }

private:
    typedef std::unordered_map<std::wstring, std::wstring> Translation; //hash_map is 15% faster than std::map on GCC
    typedef std::map<std::pair<std::wstring, std::wstring>, std::vector<std::wstring>> TranslationPlural;

    Translation       transMapping; //map original text |-> translation
    TranslationPlural transMappingPl;
    std::unique_ptr<parse_plural::PluralForm> pluralParser; //bound!
    wxLanguage langId_;
};


FFSTranslation::FFSTranslation(const Zstring& filepath, wxLanguage languageId) : langId_(languageId) //throw lngfile::ParsingError, parse_plural::ParsingError
{
    std::string inputStream;
    try
    {
        inputStream = loadBinStream<std::string>(filepath,  nullptr); //throw FileError
    }
    catch (const FileError& e)
    {
        throw lngfile::ParsingError(e.toString(), 0, 0);
        //passing FileError is too high a level for Parsing error, OTOH user is unlikely to see this since file I/O issues are sorted out by ExistingTranslations()!
    }

    lngfile::TransHeader          header;
    lngfile::TranslationMap       transInput;
    lngfile::TranslationPluralMap transPluralInput;
    lngfile::parseLng(inputStream, header, transInput, transPluralInput); //throw ParsingError

    for (const auto& item : transInput)
    {
        const std::wstring original    = utfCvrtTo<std::wstring>(item.first);
        const std::wstring translation = utfCvrtTo<std::wstring>(item.second);
        transMapping.emplace(original, translation);
    }

    for (const auto& item : transPluralInput)
    {
        const std::wstring engSingular = utfCvrtTo<std::wstring>(item.first.first);
        const std::wstring engPlural   = utfCvrtTo<std::wstring>(item.first.second);

        std::vector<std::wstring> plFormsWide;
        for (const std::string& pf : item.second)
            plFormsWide.push_back(utfCvrtTo<std::wstring>(pf));

        transMappingPl.emplace(std::make_pair(engSingular, engPlural), plFormsWide);
    }

    pluralParser = std::make_unique<parse_plural::PluralForm>(header.pluralDefinition); //throw parse_plural::ParsingError
}


struct LessTranslation
{
    bool operator()(const ExistingTranslations::Entry& lhs, const ExistingTranslations::Entry& rhs) const
    {
        //use a more "natural" sort: ignore case and diacritics
#ifdef ZEN_WIN
        const int rv = ::CompareString(LOCALE_USER_DEFAULT,      //__in  LCID Locale,
                                       NORM_IGNORECASE,          //__in  DWORD dwCmpFlags,
                                       lhs.languageName.c_str(), //__in  LPCTSTR lpString1,
                                       static_cast<int>(lhs.languageName.size()), //__in  int cchCount1,
                                       rhs.languageName.c_str(), //__in  LPCTSTR lpString2,
                                       static_cast<int>(rhs.languageName.size())); //__in  int cchCount2
        if (rv == 0)
            throw std::runtime_error("Error comparing strings (CompareString). " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        else
            return rv == CSTR_LESS_THAN; //convert to C-style string compare result

#elif defined ZEN_LINUX
        return ::wcscasecmp(lhs.languageName.c_str(), rhs.languageName.c_str()) < 0; //ignores case; locale-dependent!

#elif defined ZEN_MAC
        CFStringRef langL = osx::createCFString(utfCvrtTo<std::string>(lhs.languageName).c_str());
        ZEN_ON_SCOPE_EXIT(::CFRelease(langL));

        CFStringRef langR = osx::createCFString(utfCvrtTo<std::string>(rhs.languageName).c_str());
        ZEN_ON_SCOPE_EXIT(::CFRelease(langR));

        //correctly position "czech" unlike wcscasecmp():
        return::CFStringCompare(langL, langR, kCFCompareLocalized | kCFCompareCaseInsensitive) == kCFCompareLessThan; //no-fail
#endif
    }
};
}


ExistingTranslations::ExistingTranslations()
{
    {
        //default entry:
        ExistingTranslations::Entry newEntry;
        newEntry.languageID     = wxLANGUAGE_ENGLISH_US;
        newEntry.languageName   = L"English (US)";
        newEntry.languageFile   = L"";
        newEntry.translatorName = L"Zenju";
        newEntry.languageFlag   = L"flag_usa.png";
        locMapping.push_back(newEntry);
    }

    //search language files available
    std::vector<Zstring> lngFiles;

    traverseFolder(zen::getResourceDir() + Zstr("Languages"), [&](const FileInfo& fi)
    {
        if (pathEndsWith(fi.fullPath, Zstr(".lng")))
            lngFiles.push_back(fi.fullPath);
    }, nullptr, nullptr, [&](const std::wstring& errorMsg) { assert(false); }); //errors are not really critical in this context

    for (const Zstring& filepath : lngFiles)
    {
        try
        {
            const std::string stream = loadBinStream<std::string>(filepath,  nullptr); //throw FileError

            lngfile::TransHeader lngHeader;
            lngfile::parseHeader(stream, lngHeader); //throw ParsingError

            assert(!lngHeader.languageName  .empty());
            assert(!lngHeader.translatorName.empty());
            assert(!lngHeader.localeName    .empty());
            assert(!lngHeader.flagFile      .empty());
            /*
            There is some buggy behavior in wxWidgets which maps "zh_TW" to simplified chinese.
            Fortunately locales can be also entered as description. => use "Chinese (Traditional)" which works fine.
            */
            if (const wxLanguageInfo* locInfo = wxLocale::FindLanguageInfo(utfCvrtTo<wxString>(lngHeader.localeName)))
            {
                ExistingTranslations::Entry newEntry;
                newEntry.languageID     = locInfo->Language;
                newEntry.languageName   = utfCvrtTo<std::wstring>(lngHeader.languageName);
                newEntry.languageFile   = utfCvrtTo<std::wstring>(filepath);
                newEntry.translatorName = utfCvrtTo<std::wstring>(lngHeader.translatorName);
                newEntry.languageFlag   = utfCvrtTo<std::wstring>(lngHeader.flagFile);
                locMapping.push_back(newEntry);
            }
            else assert(false);
        }
        catch (FileError&) { assert(false); }
        catch (lngfile::ParsingError&) { assert(false); } //better not show an error message here; scenario: batch jobs
    }

    std::sort(locMapping.begin(), locMapping.end(), LessTranslation());
}


const std::vector<ExistingTranslations::Entry>& ExistingTranslations::get()
{
    static ExistingTranslations instance;
    return instance.locMapping;
}


namespace
{
wxLanguage mapLanguageDialect(wxLanguage language)
{
    switch (static_cast<int>(language)) //avoid enumeration value wxLANGUAGE_*' not handled in switch [-Wswitch-enum]
    {
        //variants of wxLANGUAGE_ARABIC
        case wxLANGUAGE_ARABIC_ALGERIA:
        case wxLANGUAGE_ARABIC_BAHRAIN:
        case wxLANGUAGE_ARABIC_EGYPT:
        case wxLANGUAGE_ARABIC_IRAQ:
        case wxLANGUAGE_ARABIC_JORDAN:
        case wxLANGUAGE_ARABIC_KUWAIT:
        case wxLANGUAGE_ARABIC_LEBANON:
        case wxLANGUAGE_ARABIC_LIBYA:
        case wxLANGUAGE_ARABIC_MOROCCO:
        case wxLANGUAGE_ARABIC_OMAN:
        case wxLANGUAGE_ARABIC_QATAR:
        case wxLANGUAGE_ARABIC_SAUDI_ARABIA:
        case wxLANGUAGE_ARABIC_SUDAN:
        case wxLANGUAGE_ARABIC_SYRIA:
        case wxLANGUAGE_ARABIC_TUNISIA:
        case wxLANGUAGE_ARABIC_UAE:
        case wxLANGUAGE_ARABIC_YEMEN:
            return wxLANGUAGE_ARABIC;

        //variants of wxLANGUAGE_CHINESE_SIMPLIFIED
        case wxLANGUAGE_CHINESE:
        case wxLANGUAGE_CHINESE_SINGAPORE:
            return wxLANGUAGE_CHINESE_SIMPLIFIED;

        //variants of wxLANGUAGE_CHINESE_TRADITIONAL
        case wxLANGUAGE_CHINESE_TAIWAN:
        case wxLANGUAGE_CHINESE_HONGKONG:
        case wxLANGUAGE_CHINESE_MACAU:
            return wxLANGUAGE_CHINESE_TRADITIONAL;

        //variants of wxLANGUAGE_DUTCH
        case wxLANGUAGE_DUTCH_BELGIAN:
            return wxLANGUAGE_DUTCH;

        //variants of wxLANGUAGE_ENGLISH_UK
        case wxLANGUAGE_ENGLISH_AUSTRALIA:
        case wxLANGUAGE_ENGLISH_NEW_ZEALAND:
        case wxLANGUAGE_ENGLISH_TRINIDAD:
        case wxLANGUAGE_ENGLISH_CARIBBEAN:
        case wxLANGUAGE_ENGLISH_JAMAICA:
        case wxLANGUAGE_ENGLISH_BELIZE:
        case wxLANGUAGE_ENGLISH_EIRE:
        case wxLANGUAGE_ENGLISH_SOUTH_AFRICA:
        case wxLANGUAGE_ENGLISH_ZIMBABWE:
        case wxLANGUAGE_ENGLISH_BOTSWANA:
        case wxLANGUAGE_ENGLISH_DENMARK:
            return wxLANGUAGE_ENGLISH_UK;

        //variants of wxLANGUAGE_ENGLISH_US
        case wxLANGUAGE_ENGLISH:
        case wxLANGUAGE_ENGLISH_CANADA:
        case wxLANGUAGE_ENGLISH_PHILIPPINES:
            return wxLANGUAGE_ENGLISH_US;

        //variants of wxLANGUAGE_FRENCH
        case wxLANGUAGE_FRENCH_BELGIAN:
        case wxLANGUAGE_FRENCH_CANADIAN:
        case wxLANGUAGE_FRENCH_LUXEMBOURG:
        case wxLANGUAGE_FRENCH_MONACO:
        case wxLANGUAGE_FRENCH_SWISS:
            return wxLANGUAGE_FRENCH;

        //variants of wxLANGUAGE_GERMAN
        case wxLANGUAGE_GERMAN_AUSTRIAN:
        case wxLANGUAGE_GERMAN_BELGIUM:
        case wxLANGUAGE_GERMAN_LIECHTENSTEIN:
        case wxLANGUAGE_GERMAN_LUXEMBOURG:
        case wxLANGUAGE_GERMAN_SWISS:
            return wxLANGUAGE_GERMAN;

        //variants of wxLANGUAGE_ITALIAN
        case wxLANGUAGE_ITALIAN_SWISS:
            return wxLANGUAGE_ITALIAN;

        //variants of wxLANGUAGE_NORWEGIAN_BOKMAL
        case wxLANGUAGE_NORWEGIAN_NYNORSK:
            return wxLANGUAGE_NORWEGIAN_BOKMAL;

        //variants of wxLANGUAGE_ROMANIAN
        case wxLANGUAGE_MOLDAVIAN:
            return wxLANGUAGE_ROMANIAN;

        //variants of wxLANGUAGE_RUSSIAN
        case wxLANGUAGE_RUSSIAN_UKRAINE:
            return wxLANGUAGE_RUSSIAN;

        //variants of wxLANGUAGE_SERBIAN
        case wxLANGUAGE_SERBIAN_CYRILLIC:
        case wxLANGUAGE_SERBIAN_LATIN:
        case wxLANGUAGE_SERBO_CROATIAN:
            return wxLANGUAGE_SERBIAN;

        //variants of wxLANGUAGE_SPANISH
        case wxLANGUAGE_SPANISH_ARGENTINA:
        case wxLANGUAGE_SPANISH_BOLIVIA:
        case wxLANGUAGE_SPANISH_CHILE:
        case wxLANGUAGE_SPANISH_COLOMBIA:
        case wxLANGUAGE_SPANISH_COSTA_RICA:
        case wxLANGUAGE_SPANISH_DOMINICAN_REPUBLIC:
        case wxLANGUAGE_SPANISH_ECUADOR:
        case wxLANGUAGE_SPANISH_EL_SALVADOR:
        case wxLANGUAGE_SPANISH_GUATEMALA:
        case wxLANGUAGE_SPANISH_HONDURAS:
        case wxLANGUAGE_SPANISH_MEXICAN:
        case wxLANGUAGE_SPANISH_MODERN:
        case wxLANGUAGE_SPANISH_NICARAGUA:
        case wxLANGUAGE_SPANISH_PANAMA:
        case wxLANGUAGE_SPANISH_PARAGUAY:
        case wxLANGUAGE_SPANISH_PERU:
        case wxLANGUAGE_SPANISH_PUERTO_RICO:
        case wxLANGUAGE_SPANISH_URUGUAY:
        case wxLANGUAGE_SPANISH_US:
        case wxLANGUAGE_SPANISH_VENEZUELA:
            return wxLANGUAGE_SPANISH;

        //variants of wxLANGUAGE_SWEDISH
        case wxLANGUAGE_SWEDISH_FINLAND:
            return wxLANGUAGE_SWEDISH;

        //languages without variants:
        //case wxLANGUAGE_BULGARIAN:
        //case wxLANGUAGE_CROATIAN:
        //case wxLANGUAGE_CZECH:
        //case wxLANGUAGE_DANISH:
        //case wxLANGUAGE_FINNISH:
        //case wxLANGUAGE_GREEK:
        //case wxLANGUAGE_HINDI:
        //case wxLANGUAGE_HEBREW:
        //case wxLANGUAGE_HUNGARIAN:
        //case wxLANGUAGE_JAPANESE:
        //case wxLANGUAGE_KOREAN:
        //case wxLANGUAGE_LITHUANIAN:
        //case wxLANGUAGE_POLISH:
        //case wxLANGUAGE_PORTUGUESE:
        //case wxLANGUAGE_PORTUGUESE_BRAZILIAN:
        //case wxLANGUAGE_SCOTS_GAELIC:
        //case wxLANGUAGE_SLOVENIAN:
        //case wxLANGUAGE_TURKISH:
        //case wxLANGUAGE_UKRAINIAN:
        default:
            return language;
    }
}


//global wxWidgets localization: sets up C localization runtime as well!
class wxWidgetsLocale
{
public:
    static wxWidgetsLocale& getInstance()
    {
        static wxWidgetsLocale inst;
        return inst;
    }

    void init(wxLanguage lng)
    {
        locale.reset(); //avoid global locale lifetime overlap! wxWidgets cannot handle this and will crash!
        locale = std::make_unique<wxLocale>();

        const wxLanguageInfo* sysLngInfo = wxLocale::GetLanguageInfo(wxLocale::GetSystemLanguage());
        const wxLanguageInfo* selLngInfo = wxLocale::GetLanguageInfo(lng);

        const bool sysLangIsRTL      = sysLngInfo ? sysLngInfo->LayoutDirection == wxLayout_RightToLeft : false;
        const bool selectedLangIsRTL = selLngInfo ? selLngInfo->LayoutDirection == wxLayout_RightToLeft : false;

#ifdef NDEBUG
        wxLogNull dummy; //rather than implementing a reasonable error handling wxWidgets decides to shows a modal dialog in wxLocale::Init -> at least we can shut it up!
#endif
        if (sysLangIsRTL == selectedLangIsRTL)
            locale->Init(wxLANGUAGE_DEFAULT); //use sys-lang to preserve sub-language specific rules (e.g. german swiss number punctation)
        else
            locale->Init(lng); //have to use the supplied language to enable RTL layout different than user settings
        locLng = lng;
    }

    void release() { locale.reset(); locLng = wxLANGUAGE_UNKNOWN; }

    wxLanguage getLanguage() const { return locLng; }


private:
    wxWidgetsLocale() {}
    ~wxWidgetsLocale() { assert(!locale); }

    std::unique_ptr<wxLocale> locale;
    wxLanguage locLng = wxLANGUAGE_UNKNOWN;
};
}


void zen::releaseWxLocale()
{
    wxWidgetsLocale::getInstance().release();
}


void zen::setLanguage(int language) //throw FileError
{
    if (language == getLanguage() && wxWidgetsLocale::getInstance().getLanguage() == language)
        return; //support polling

    //(try to) retrieve language file
    std::wstring languageFile;

    for (const ExistingTranslations::Entry& e : ExistingTranslations::get())
        if (e.languageID == language)
        {
            languageFile = e.languageFile;
            break;
        }

    //load language file into buffer
    if (languageFile.empty()) //if languageFile is empty, texts will be english by default
        zen::setTranslator();
    else
        try
        {
            zen::setTranslator(std::make_unique<FFSTranslation>(utfCvrtTo<Zstring>(languageFile), static_cast<wxLanguage>(language))); //throw lngfile::ParsingError, parse_plural::ParsingError
        }
        catch (lngfile::ParsingError& e)
        {
            throw FileError(replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                                             L"%x", fmtPath(utfCvrtTo<Zstring>(languageFile))),
                                                  L"%y", numberTo<std::wstring>(e.row_ + 1)),
                                       L"%z", numberTo<std::wstring>(e.col_ + 1))
                            + L"\n\n" + e.msg_);
        }
        catch (parse_plural::ParsingError&)
        {
            throw FileError(L"Invalid plural form definition"); //user should never see this!
        }

    //handle RTL swapping: we need wxWidgets to do this
    wxWidgetsLocale::getInstance().init(languageFile.empty() ? wxLANGUAGE_ENGLISH : static_cast<wxLanguage>(language));
}


int zen::getLanguage()
{
    const FFSTranslation* loc = dynamic_cast<const FFSTranslation*>(zen::getTranslator());
    return loc ? loc->langId() : wxLANGUAGE_ENGLISH_US;
}


int zen::retrieveSystemLanguage()
{
    return mapLanguageDialect(static_cast<wxLanguage>(wxLocale::GetSystemLanguage()));
}
