// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PARSE_LNG_HEADER_INCLUDED
#define PARSE_LNG_HEADER_INCLUDED

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <list>
#include <zen/utf.h>
#include <zen/string_tools.h>
#include "parse_plural.h"

namespace lngfile
{
//singular forms
typedef std::map <std::string, std::string> TranslationMap;  //orig |-> translation

//plural forms
typedef std::pair<std::string, std::string> SingularPluralPair; //1 house| n houses
typedef std::vector<std::string> PluralForms; //1 dom | 2 domy | 5 domów
typedef std::map<SingularPluralPair, PluralForms> TranslationPluralMap; //(sing/plu) |-> pluralforms

struct TransHeader
{
    TransHeader() : pluralCount(0) {}
    std::string languageName;     //display name: "English (UK)"
    std::string translatorName;   //"Zenju"
    std::string localeName;       //ISO 639 language code + ISO 3166 country code, e.g. "en_GB", or "en_US"
    std::string flagFile;         //"england.png"
    int pluralCount;              //2
    std::string pluralDefinition; //"n == 1 ? 0 : 1"
};


struct ParsingError
{
    ParsingError(const std::wstring& msg, size_t row, size_t col) : msg_(msg), row_(row), col_(col) {}
    std::wstring msg_; //parser error message
    size_t row_; //starting with 0
    size_t col_; //
};
void parseLng(const std::string& fileStream, TransHeader& header, TranslationMap& out, TranslationPluralMap& pluralOut); //throw ParsingError
void parseHeader(const std::string& fileStream, TransHeader& header); //throw ParsingError

class TranslationUnorderedList; //unordered list of unique translation items
std::string generateLng(const TranslationUnorderedList& in, const TransHeader& header);



















//--------------------------- implementation ---------------------------
enum class TranslationNewItemPos
{
    REL,
    TOP
};

class TranslationUnorderedList //unordered list of unique translation items
{
public:
    TranslationUnorderedList(TranslationNewItemPos newItemPos, TranslationMap&& transOld, TranslationPluralMap&& transPluralOld) :
        newItemPos_(newItemPos), transOld_(std::move(transOld)), transPluralOld_(std::move(transPluralOld)) {}

    void addItem(const std::string& orig)
    {
        if (!transUnique.insert(orig).second) return;
        auto it = transOld_.find(orig);
        if (it != transOld_.end() && !it->second.empty()) //preserve old translation from .lng file if existing
            sequence.push_back(std::make_shared<RegularItem>(std::make_pair(orig, it->second)));
        else
            switch (newItemPos_)
            {
                case TranslationNewItemPos::REL:
                    sequence.push_back(std::make_shared<RegularItem>(std::make_pair(orig, std::string())));
                    break;
                case TranslationNewItemPos::TOP:
                    sequence.push_front(std::make_shared<RegularItem>(std::make_pair(orig, std::string()))); //put untranslated items to the front of the .lng filebreak;
                    break;
            }
    }

    void addItem(const SingularPluralPair& orig)
    {
        if (!pluralUnique.insert(orig).second) return;
        auto it = transPluralOld_.find(orig);
        if (it != transPluralOld_.end() && !it->second.empty()) //preserve old translation from .lng file if existing
            sequence.push_back(std::make_shared<PluralItem>(std::make_pair(orig, it->second)));
        else
            switch (newItemPos_)
            {
                case TranslationNewItemPos::REL:
                    sequence.push_back(std::make_shared<PluralItem>(std::make_pair(orig, PluralForms())));
                    break;
                case TranslationNewItemPos::TOP:
                    sequence.push_front(std::make_shared<PluralItem>(std::make_pair(orig, PluralForms()))); //put untranslated items to the front of the .lng file
                    break;
            }
    }

    bool untranslatedTextExists() const { return std::any_of(sequence.begin(), sequence.end(), [](const std::shared_ptr<Item>& item) { return !item->hasTranslation(); }); }

    template <class Function, class Function2>
    void visitItems(Function onTrans, Function2 onPluralTrans) const //onTrans takes (const TranslationMap::value_type&), onPluralTrans takes (const TranslationPluralMap::value_type&)
    {
        for (const auto& item : sequence)
            if (auto regular = dynamic_cast<const RegularItem*>(item.get()))
                onTrans(regular->value);
            else if (auto plural = dynamic_cast<const PluralItem*>(item.get()))
                onPluralTrans(plural->value);
            else assert(false);
    }

private:
    struct Item { virtual ~Item() {} virtual bool hasTranslation() const = 0; };
    struct RegularItem : public Item { RegularItem(const TranslationMap      ::value_type& val) : value(val) {} bool hasTranslation() const override { return !value.second.empty(); } TranslationMap      ::value_type value; };
    struct PluralItem  : public Item { PluralItem (const TranslationPluralMap::value_type& val) : value(val) {} bool hasTranslation() const override { return !value.second.empty(); } TranslationPluralMap::value_type value; };

    const TranslationNewItemPos newItemPos_;
    std::list<std::shared_ptr<Item>> sequence; //ordered list of translation elements

    std::set<TranslationMap      ::key_type> transUnique;  //check uniqueness
    std::set<TranslationPluralMap::key_type> pluralUnique; //

    const TranslationMap transOld_;             //reuse existing translation
    const TranslationPluralMap transPluralOld_; //
};


struct Token
{
    enum Type
    {
        //header information
        TK_HEADER_BEGIN,
        TK_HEADER_END,
        TK_LANG_NAME_BEGIN,
        TK_LANG_NAME_END,
        TK_TRANS_NAME_BEGIN,
        TK_TRANS_NAME_END,
        TK_LOCALE_NAME_BEGIN,
        TK_LOCALE_NAME_END,
        TK_FLAG_FILE_BEGIN,
        TK_FLAG_FILE_END,
        TK_PLURAL_COUNT_BEGIN,
        TK_PLURAL_COUNT_END,
        TK_PLURAL_DEF_BEGIN,
        TK_PLURAL_DEF_END,

        //item level
        TK_SRC_BEGIN,
        TK_SRC_END,
        TK_TRG_BEGIN,
        TK_TRG_END,
        TK_TEXT,
        TK_PLURAL_BEGIN,
        TK_PLURAL_END,
        TK_END
    };

    Token(Type t) : type(t) {}
    Type type;

    std::string text;
};


class KnownTokens
{
public:
    typedef std::map<Token::Type, std::string> TokenMap;

    static const TokenMap& getList()
    {
        static KnownTokens inst;
        return inst.tokens;
    }

    static std::string text(Token::Type t)
    {
        auto it = getList().find(t);
        return it != getList().end() ? it->second : std::string();
    }

private:
    KnownTokens()
    {
        //header information
        tokens.emplace(Token::TK_HEADER_BEGIN,       "<header>");
        tokens.emplace(Token::TK_HEADER_END,         "</header>");
        tokens.emplace(Token::TK_LANG_NAME_BEGIN,    "<language>");
        tokens.emplace(Token::TK_LANG_NAME_END,      "</language>");
        tokens.emplace(Token::TK_TRANS_NAME_BEGIN,   "<translator>");
        tokens.emplace(Token::TK_TRANS_NAME_END,     "</translator>");
        tokens.emplace(Token::TK_LOCALE_NAME_BEGIN,  "<locale>");
        tokens.emplace(Token::TK_LOCALE_NAME_END,    "</locale>");
        tokens.emplace(Token::TK_FLAG_FILE_BEGIN,    "<image>");
        tokens.emplace(Token::TK_FLAG_FILE_END,      "</image>");
        tokens.emplace(Token::TK_PLURAL_COUNT_BEGIN, "<plural_count>");
        tokens.emplace(Token::TK_PLURAL_COUNT_END,   "</plural_count>");
        tokens.emplace(Token::TK_PLURAL_DEF_BEGIN,   "<plural_definition>");
        tokens.emplace(Token::TK_PLURAL_DEF_END,     "</plural_definition>");

        //item level
        tokens.emplace(Token::TK_SRC_BEGIN,    "<source>");
        tokens.emplace(Token::TK_SRC_END,      "</source>");
        tokens.emplace(Token::TK_TRG_BEGIN,    "<target>");
        tokens.emplace(Token::TK_TRG_END,      "</target>");
        tokens.emplace(Token::TK_PLURAL_BEGIN, "<pluralform>");
        tokens.emplace(Token::TK_PLURAL_END,   "</pluralform>");
    }
    TokenMap tokens;
};


class Scanner
{
public:
    Scanner(const std::string& fileStream) : stream(fileStream), pos(stream.begin())
    {
        if (zen::startsWith(stream, zen::BYTE_ORDER_MARK_UTF8))
            pos += zen::strLength(zen::BYTE_ORDER_MARK_UTF8);
    }

    Token nextToken()
    {
        //skip whitespace
        pos = std::find_if(pos, stream.end(), [](char c) { return !zen::isWhiteSpace(c); });

        if (pos == stream.end())
            return Token(Token::TK_END);

        for (const auto& token : KnownTokens::getList())
            if (startsWith(token.second))
            {
                pos += token.second.size();
                return Token(token.first);
            }

        //rest must be "text"
        auto itBegin = pos;
        while (pos != stream.end() && !startsWithKnownTag())
            pos = std::find(pos + 1, stream.end(), '<');

        std::string text(itBegin, pos);

        normalize(text); //remove whitespace from end ect.

        if (text.empty() && pos == stream.end())
            return Token(Token::TK_END);

        Token out(Token::TK_TEXT);
        out.text = text;
        return out;
    }

    size_t posRow() const //current row beginning with 0
    {
        //count line endings
        const size_t crSum = std::count(stream.begin(), pos, '\r'); //carriage returns
        const size_t nlSum = std::count(stream.begin(), pos, '\n'); //new lines
        assert(crSum == 0 || nlSum == 0 || crSum == nlSum);
        return std::max(crSum, nlSum); //be compatible with Linux/Mac/Win
    }

    size_t posCol() const //current col beginning with 0
    {
        //seek beginning of line
        for (auto it = pos; it != stream.begin(); )
        {
            --it;
            if (*it == '\r' || *it == '\n')
                return pos - it - 1;
        }
        return pos - stream.begin();
    }

private:
    bool startsWithKnownTag() const
    {
        return std::any_of(KnownTokens::getList().begin(), KnownTokens::getList().end(),
        [&](const KnownTokens::TokenMap::value_type& p) { return startsWith(p.second); });
    }

    bool startsWith(const std::string& prefix) const
    {
        if (stream.end() - pos < static_cast<ptrdiff_t>(prefix.size()))
            return false;
        return std::equal(prefix.begin(), prefix.end(), pos);
    }

    static void normalize(std::string& text)
    {
        zen::trim(text); //remove whitespace from both ends

        //Delimiter:
        //----------
        //Linux: 0xA        \n
        //Mac:   0xD        \r
        //Win:   0xD 0xA    \r\n    <- language files are in Windows format
        zen::replace(text, "\r\n", '\n'); //
        zen::replace(text, "\r",   '\n'); //ensure c-style line breaks
    }

    const std::string stream;
    std::string::const_iterator pos;
};


class LngParser
{
public:
    LngParser(const std::string& fileStream) : scn(fileStream), tk(scn.nextToken()) {}

    void parse(TranslationMap& out, TranslationPluralMap& pluralOut, TransHeader& header)
    {
        parseHeader(header);

        try
        {
            parse_plural::PluralFormInfo pi(header.pluralDefinition, header.pluralCount);

            //items
            while (token().type != Token::TK_END)
                parseRegular(out, pluralOut, pi);
        }
        catch (const parse_plural::InvalidPluralForm&)
        {
            throw ParsingError(L"Invalid plural form definition", scn.posRow(), scn.posCol());
        }
    }

    void parseHeader(TransHeader& header)
    {
        consumeToken(Token::TK_HEADER_BEGIN);

        consumeToken(Token::TK_LANG_NAME_BEGIN);
        header.languageName = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_LANG_NAME_END);

        consumeToken(Token::TK_TRANS_NAME_BEGIN);
        header.translatorName = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_TRANS_NAME_END);

        consumeToken(Token::TK_LOCALE_NAME_BEGIN);
        header.localeName = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_LOCALE_NAME_END);

        consumeToken(Token::TK_FLAG_FILE_BEGIN);
        header.flagFile = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_FLAG_FILE_END);

        consumeToken(Token::TK_PLURAL_COUNT_BEGIN);
        header.pluralCount = zen::stringTo<int>(token().text);
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_PLURAL_COUNT_END);

        consumeToken(Token::TK_PLURAL_DEF_BEGIN);
        header.pluralDefinition = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_PLURAL_DEF_END);

        consumeToken(Token::TK_HEADER_END);
    }

private:
    void parseRegular(TranslationMap& out, TranslationPluralMap& pluralOut, const parse_plural::PluralFormInfo& pluralInfo)
    {
        consumeToken(Token::TK_SRC_BEGIN);

        if (token().type == Token::TK_PLURAL_BEGIN)
            return parsePlural(pluralOut, pluralInfo);

        std::string original = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_SRC_END);

        consumeToken(Token::TK_TRG_BEGIN);
        std::string translation;
        if (token().type == Token::TK_TEXT)
        {
            translation = token().text;
            nextToken();
        }
        validateTranslation(original, translation); //throw throw ParsingError
        consumeToken(Token::TK_TRG_END);

        out.emplace(original, translation);
    }

    void parsePlural(TranslationPluralMap& pluralOut, const parse_plural::PluralFormInfo& pluralInfo)
    {
        //Token::TK_SRC_BEGIN already consumed

        consumeToken(Token::TK_PLURAL_BEGIN);
        std::string engSingular = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_PLURAL_END);

        consumeToken(Token::TK_PLURAL_BEGIN);
        std::string engPlural = token().text;
        consumeToken(Token::TK_TEXT);
        consumeToken(Token::TK_PLURAL_END);

        consumeToken(Token::TK_SRC_END);
        const SingularPluralPair original(engSingular, engPlural);

        consumeToken(Token::TK_TRG_BEGIN);

        PluralForms pluralList;
        while (token().type == Token::TK_PLURAL_BEGIN)
        {
            consumeToken(Token::TK_PLURAL_BEGIN);
            std::string pluralForm = token().text;
            consumeToken(Token::TK_TEXT);
            consumeToken(Token::TK_PLURAL_END);
            pluralList.push_back(pluralForm);
        }
        validateTranslation(original, pluralList, pluralInfo);
        consumeToken(Token::TK_TRG_END);

        pluralOut.emplace(original, pluralList);
    }

    void validateTranslation(const std::string& original, const std::string& translation) //throw ParsingError
    {
        if (original.empty())
            throw ParsingError(L"Source translation is empty", scn.posRow(), scn.posCol());

        if (!translation.empty())
        {
            //if original contains placeholder, so must translation!
            auto checkPlaceholder = [&](const std::string& placeholder)
            {
                if (zen::contains(original, placeholder) &&
                    !zen::contains(translation, placeholder))
                    throw ParsingError(zen::replaceCpy<std::wstring>(L"Placeholder %x missing in translation", L"%x", zen::utfCvrtTo<std::wstring>(placeholder)), scn.posRow(), scn.posCol());
            };
            checkPlaceholder("%x");
            checkPlaceholder("%y");
            checkPlaceholder("%z");

            auto ampersandTokenCount = [](const std::string& str) -> size_t
            {
                const std::string tmp = zen::replaceCpy(str, "&&", ""); //make sure to not catch && which windows resolves as just one & for display!
                return std::count(tmp.begin(), tmp.end(), '&');
            };

            //if source contains ampersand to mark menu accellerator key, so must translation
            const size_t ampCountOrig = ampersandTokenCount(original);
            if (ampCountOrig != ampersandTokenCount(translation) ||
                ampCountOrig > 1)
                throw ParsingError(L"Source and translation both need exactly one & character to mark a menu item access key or none at all", scn.posRow(), scn.posCol());

            //ampersand at the end makes buggy wxWidgets crash miserably
            if (ampCountOrig > 0)
                if ((zen::endsWith(original,    "&") && !zen::endsWith(original,    "&&")) ||
                    (zen::endsWith(translation, "&") && !zen::endsWith(translation, "&&")))
                    throw ParsingError(L"The & character to mark a menu item access key must not occur at the end of a string", scn.posRow(), scn.posCol());

#if 0
            //if source ends with colon, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (zen::endsWith(original, ":") &&
                !zen::endsWith(translation, ":") &&
                !zen::endsWith(translation, "\xef\xbc\x9a")) //chinese colon
                throw ParsingError(L"Source text ends with a colon character \":\", but translation does not", scn.posRow(), scn.posCol());

            auto endsWithSingleDot = [](const std::string& s) { return zen::endsWith(s, ".") && !zen::endsWith(s, ".."); };

            //if source ends with a period, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWithSingleDot(original) &&
                !endsWithSingleDot(translation) &&
                !zen::endsWith(translation, "\xe3\x80\x82")) //chinese period
                throw ParsingError(L"Source text ends with a punctuation mark character \".\", but translation does not", scn.posRow(), scn.posCol());

            //if source ends with an ellipsis, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (zen::endsWith(original, "...") &&
                !zen::endsWith(translation, "...") &&
                !zen::endsWith(translation, "\xe2\x80\xa6")) //narrow ellipsis (spanish?)
                throw ParsingError(L"Source text ends with an ellipsis \"...\", but translation does not", scn.posRow(), scn.posCol());
#endif
        }
    }

    void validateTranslation(const SingularPluralPair& original, const PluralForms& translation, const parse_plural::PluralFormInfo& pluralInfo) //throw ParsingError
    {
        using namespace zen;
        //check the primary placeholder is existing at least for the second english text
        if (!contains(original.second, "%x"))
            throw ParsingError(L"Plural form source does not contain %x placeholder", scn.posRow(), scn.posCol());

        if (!translation.empty())
        {
            //check for invalid number of plural forms
            if (pluralInfo.getCount() != static_cast<int>(translation.size()))
                throw ParsingError(replaceCpy(replaceCpy<std::wstring>(L"Invalid number of plural forms; actual: %x, expected: %y", L"%x", numberTo<std::wstring>(translation.size())), L"%y", numberTo<std::wstring>(pluralInfo.getCount())), scn.posRow(), scn.posCol());

            //check for duplicate plural form translations (catch copy & paste errors for single-number form translations)
            for (auto it = translation.begin(); it != translation.end(); ++it)
                if (!contains(*it, "%x"))
                {
                    auto it2 = std::find(it + 1, translation.end(), *it);
                    if (it2 != translation.end())
                        throw ParsingError(replaceCpy<std::wstring>(L"Duplicate plural form translation at index position %x", L"%x", numberTo<std::wstring>(it2 - translation.begin())), scn.posRow(), scn.posCol());
                }

            for (int pos = 0; pos < static_cast<int>(translation.size()); ++pos)
                if (pluralInfo.isSingleNumberForm(pos))
                {
                    //translation needs to use decimal number if english source does so (e.g. frequently changing text like statistics)
                    if (contains(original.first, "%x") ||
                        contains(original.first, "1"))
                    {
                        const int firstNumber = pluralInfo.getFirstNumber(pos);
                        if (!(contains(translation[pos], "%x") ||
                              contains(translation[pos], numberTo<std::string>(firstNumber))))
                            throw ParsingError(replaceCpy<std::wstring>(replaceCpy<std::wstring>(L"Plural form translation at index position %y needs to use the decimal number %z or the %x placeholder",
                                                                                                 L"%y", numberTo<std::wstring>(pos)), L"%z", numberTo<std::wstring>(firstNumber)), scn.posRow(), scn.posCol());
                    }
                }
                else
                {
                    //ensure the placeholder is used when needed
                    if (!contains(translation[pos], "%x"))
                        throw ParsingError(replaceCpy<std::wstring>(L"Plural form at index position %y is missing the %x placeholder", L"%y", numberTo<std::wstring>(pos)), scn.posRow(), scn.posCol());
                }

            auto checkSecondaryPlaceholder = [&](const std::string& placeholder)
            {
                //make sure secondary placeholder is used in both source texts (or none)
                if (zen::contains(original.first,  placeholder) ||
                    zen::contains(original.second, placeholder))
                {
                    if (!zen::contains(original.first,  placeholder) ||
                        !zen::contains(original.second, placeholder))
                        throw ParsingError(zen::replaceCpy<std::wstring>(L"Placeholder %x missing in plural form source", L"%x", zen::utfCvrtTo<std::wstring>(placeholder)), scn.posRow(), scn.posCol());

                    //secondary placeholder is required for all plural forms
                    if (!std::all_of(translation.begin(), translation.end(), [&](const std::string& pform) { return zen::contains(pform, placeholder); }))
                    throw ParsingError(zen::replaceCpy<std::wstring>(L"Placeholder %x missing in plural form translation", L"%x", zen::utfCvrtTo<std::wstring>(placeholder)), scn.posRow(), scn.posCol());
                }
            };

            checkSecondaryPlaceholder("%y");
            checkSecondaryPlaceholder("%z");
        }
    }

    void nextToken() { tk = scn.nextToken(); }
    const Token& token() const { return tk; }

    void consumeToken(Token::Type t) //throw ParsingError
    {
        expectToken(t); //throw ParsingError
        nextToken();
    }

    void expectToken(Token::Type t) //throw ParsingError
    {
        if (token().type != t)
            throw ParsingError(L"Unexpected token", scn.posRow(), scn.posCol());
    }

    Scanner scn;
    Token tk;
};


inline
void parseLng(const std::string& fileStream, TransHeader& header, TranslationMap& out, TranslationPluralMap& pluralOut) //throw ParsingError
{
    out.clear();
    pluralOut.clear();

    LngParser(fileStream).parse(out, pluralOut, header);
}


inline
void parseHeader(const std::string& fileStream, TransHeader& header) //throw ParsingError
{
    LngParser(fileStream).parseHeader(header);
}


inline
void formatMultiLineText(std::string& text)
{
    assert(!zen::contains(text, "\r\n"));

    if (text.find('\n') != std::string::npos) //multiple lines
    {
        if (*text.begin() != '\n')
            text = '\n' + text;
        if (*text.rbegin() != '\n')
            text += '\n';
    }
}


std::string generateLng(const TranslationUnorderedList& in, const TransHeader& header)
{
    std::string out;
    //header
    out += KnownTokens::text(Token::TK_HEADER_BEGIN) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_LANG_NAME_BEGIN);
    out += header.languageName;
    out += KnownTokens::text(Token::TK_LANG_NAME_END) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_TRANS_NAME_BEGIN);
    out += header.translatorName;
    out += KnownTokens::text(Token::TK_TRANS_NAME_END) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_LOCALE_NAME_BEGIN);
    out += header.localeName;
    out += KnownTokens::text(Token::TK_LOCALE_NAME_END) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_FLAG_FILE_BEGIN);
    out += header.flagFile;
    out += KnownTokens::text(Token::TK_FLAG_FILE_END) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_PLURAL_COUNT_BEGIN);
    out += zen::numberTo<std::string>(header.pluralCount);
    out += KnownTokens::text(Token::TK_PLURAL_COUNT_END) + '\n';

    out += '\t' + KnownTokens::text(Token::TK_PLURAL_DEF_BEGIN);
    out += header.pluralDefinition;
    out += KnownTokens::text(Token::TK_PLURAL_DEF_END) + '\n';

    out += KnownTokens::text(Token::TK_HEADER_END) + '\n';

    out += '\n';


    in.visitItems([&](const TranslationMap::value_type& trans)
    {
        std::string original    = trans.first;
        std::string translation = trans.second;

        formatMultiLineText(original);
        formatMultiLineText(translation);

        out += KnownTokens::text(Token::TK_SRC_BEGIN);
        out += original;
        out += KnownTokens::text(Token::TK_SRC_END) + '\n';

        out += KnownTokens::text(Token::TK_TRG_BEGIN);
        out += translation;
        out += KnownTokens::text(Token::TK_TRG_END) + '\n' + '\n';
    },
    [&](const TranslationPluralMap::value_type& transPlural)
    {
        std::string engSingular  = transPlural.first.first;
        std::string engPlural    = transPlural.first.second;
        const PluralForms& forms = transPlural.second;

        formatMultiLineText(engSingular);
        formatMultiLineText(engPlural);

        out += KnownTokens::text(Token::TK_SRC_BEGIN) + '\n';
        out += KnownTokens::text(Token::TK_PLURAL_BEGIN);
        out += engSingular;
        out += KnownTokens::text(Token::TK_PLURAL_END) + '\n';
        out += KnownTokens::text(Token::TK_PLURAL_BEGIN);
        out += engPlural;
        out += KnownTokens::text(Token::TK_PLURAL_END) + '\n';
        out += KnownTokens::text(Token::TK_SRC_END) + '\n';

        out += KnownTokens::text(Token::TK_TRG_BEGIN);
        if (!forms.empty())
            out += '\n';
        for (std::string plForm : forms)
        {
            formatMultiLineText(plForm);

            out += KnownTokens::text(Token::TK_PLURAL_BEGIN);
            out += plForm;
            out += KnownTokens::text(Token::TK_PLURAL_END) + '\n';
        }
        out += KnownTokens::text(Token::TK_TRG_END) + '\n' + '\n';
    });

    assert(!zen::contains(out, "\r\n") && !zen::contains(out, "\r"));
    return zen::replaceCpy(out, '\n', "\r\n"); //back to win line endings
}
}

#endif //PARSE_LNG_HEADER_INCLUDED
