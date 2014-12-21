// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef I18_N_HEADER_3843489325045
#define I18_N_HEADER_3843489325045

#include <string>
#include <memory>
#include <cstdint>
#include "string_tools.h"
#include "format_unit.h"

//minimal layer enabling text translation - without platform/library dependencies!
#ifdef __WXMSW__ //we have wxWidgets
    #ifndef WXINTL_NO_GETTEXT_MACRO
        #error WXINTL_NO_GETTEXT_MACRO must be defined to deactivate wxWidgets underscore macro
    #endif
#endif

#define ZEN_TRANS_CONCAT_SUB(X, Y) X ## Y
#define _(s) zen::implementation::translate(ZEN_TRANS_CONCAT_SUB(L, s))
#define _P(s, p, n) zen::implementation::translate(ZEN_TRANS_CONCAT_SUB(L, s), ZEN_TRANS_CONCAT_SUB(L, p), n)
//source and translation are required to use %x as number placeholder
//for plural form, which will be substituted automatically!!!

namespace zen
{
//implement handler to enable program wide localizations: implement THREAD-SAFE ACCESS!
struct TranslationHandler
{
    virtual ~TranslationHandler() {}

    virtual std::wstring translate(const std::wstring& text) = 0; //simple translation
    virtual std::wstring translate(const std::wstring& singular, const std::wstring& plural, std::int64_t n) = 0;
};

void setTranslator(std::unique_ptr<TranslationHandler>&& newHandler = nullptr); //take ownership
TranslationHandler* getTranslator();













//######################## implementation ##############################
namespace implementation
{
inline
std::wstring translate(const std::wstring& text)
{
    if (TranslationHandler* t = getTranslator())
        return t->translate(text);

    return text;
}

//translate plural forms: "%x day" "%x days"
//returns "1 day" if n == 1; "123 days" if n == 123 for english language
inline
std::wstring translate(const std::wstring& singular, const std::wstring& plural, std::int64_t n)
{
    assert(contains(plural, L"%x"));

    if (TranslationHandler* t = getTranslator())
    {
        std::wstring translation = t->translate(singular, plural, n);
        assert(!contains(translation, L"%x"));
        return translation;
    }

    return replaceCpy(std::abs(n) == 1 ? singular : plural, L"%x", toGuiString(n));
}

template <class T> inline
std::wstring translate(const std::wstring& singular, const std::wstring& plural, T n)
{
    static_assert(sizeof(n) <= sizeof(std::int64_t), "");
    return translate(singular, plural, static_cast<std::int64_t>(n));
}

inline
std::unique_ptr<TranslationHandler>& globalHandler()
{
    static std::unique_ptr<TranslationHandler> inst; //external linkage even in header!
    return inst;
}
}

inline
void setTranslator(std::unique_ptr<TranslationHandler>&& newHandler) { implementation::globalHandler() = std::move(newHandler); }

inline
TranslationHandler* getTranslator() { return implementation::globalHandler().get(); }
}

#endif //I18_N_HEADER_3843489325045
