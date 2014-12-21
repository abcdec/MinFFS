// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PARSE_PLURAL_H_INCLUDED
#define PARSE_PLURAL_H_INCLUDED

#include <memory>
#include <cstdint>
#include <functional>
#include <zen/string_base.h>

namespace parse_plural
{
//expression interface
struct Expression { virtual ~Expression() {} };

template <class T>
struct Expr : public Expression
{
    typedef T ValueType;
    virtual ValueType eval() const = 0;
};


class ParsingError {};

class PluralForm
{
public:
    PluralForm(const std::string& stream); //throw ParsingError
    int getForm(std::int64_t n) const { n_ = std::abs(n) ; return static_cast<int>(expr->eval()); }

private:
    std::shared_ptr<Expr<std::int64_t>> expr;
    mutable std::int64_t n_;
};


//validate plural form
class InvalidPluralForm {};

class PluralFormInfo
{
public:
    PluralFormInfo(const std::string& definition, int pluralCount); //throw InvalidPluralForm

    int getCount() const { return static_cast<int>(forms.size()); }
    bool isSingleNumberForm(int index) const { return 0 <= index && index < static_cast<int>(forms.size()) ? forms[index].count == 1 : false; }
    int getFirstNumber     (int index) const { return 0 <= index && index < static_cast<int>(forms.size()) ? forms[index].firstNumber : -1; }

private:
    struct FormInfo
    {
        FormInfo() : count(0), firstNumber(0) {}
        int count;
        int firstNumber; //which maps to the plural form index position
    };
    std::vector<FormInfo> forms;
};





//--------------------------- implementation ---------------------------

//http://www.gnu.org/software/hello/manual/gettext/Plural-forms.html
//http://translate.sourceforge.net/wiki/l10n/pluralforms
/*
Grammar for Plural forms parser
-------------------------------
expression:
    conditional-expression

conditional-expression:
    logical-or-expression
    logical-or-expression ? expression : expression

logical-or-expression:
    logical-and-expression
    logical-or-expression || logical-and-expression

logical-and-expression:
    equality-expression
    logical-and-expression && equality-expression

equality-expression:
    relational-expression
    relational-expression == relational-expression
    relational-expression != relational-expression

relational-expression:
    multiplicative-expression
    multiplicative-expression >  multiplicative-expression
    multiplicative-expression <  multiplicative-expression
    multiplicative-expression >= multiplicative-expression
    multiplicative-expression <= multiplicative-expression

multiplicative-expression:
    pm-expression
    multiplicative-expression % pm-expression

pm-expression:
    variable-number-n-expression
    constant-number-expression
    ( expression )


.po format,e.g.: (n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)
*/

namespace implementation
{
//specific binary expression based on STL function objects
template <class StlOp>
struct BinaryExp : public Expr<typename StlOp::result_type>
{
    typedef std::shared_ptr<Expr<typename StlOp::first_argument_type >> ExpLhs;
    typedef std::shared_ptr<Expr<typename StlOp::second_argument_type>> ExpRhs;

    BinaryExp(const ExpLhs& lhs, const ExpRhs& rhs, StlOp biop) : lhs_(lhs), rhs_(rhs), biop_(biop) { assert(lhs && rhs); }
    typename StlOp::result_type eval() const override { return biop_(lhs_->eval(), rhs_->eval()); }
private:
    ExpLhs lhs_;
    ExpRhs rhs_;
    StlOp biop_;
};

template <class StlOp> inline
std::shared_ptr<BinaryExp<StlOp>> makeBiExp(const std::shared_ptr<Expression>& lhs, const std::shared_ptr<Expression>& rhs, StlOp biop) //throw ParsingError
{
    auto exLeft  = std::dynamic_pointer_cast<Expr<typename StlOp::first_argument_type >>(lhs);
    auto exRight = std::dynamic_pointer_cast<Expr<typename StlOp::second_argument_type>>(rhs);
    if (!exLeft || !exRight)
        throw ParsingError();
    return std::make_shared<BinaryExp<StlOp>>(exLeft, exRight, biop);
}

template <class T>
struct ConditionalExp : public Expr<T>
{
    ConditionalExp(const std::shared_ptr<Expr<bool>>& ifExp,
                   const std::shared_ptr<Expr<T>>& thenExp,
                   const std::shared_ptr<Expr<T>>& elseExp) : ifExp_(ifExp), thenExp_(thenExp), elseExp_(elseExp) { assert(ifExp && thenExp && elseExp); }

    typename Expr<T>::ValueType eval() const override { return ifExp_->eval() ? thenExp_->eval() : elseExp_->eval(); }
private:
    std::shared_ptr<Expr<bool>> ifExp_;
    std::shared_ptr<Expr<T>> thenExp_;
    std::shared_ptr<Expr<T>> elseExp_;
};

struct ConstNumberExp : public Expr<std::int64_t>
{
    ConstNumberExp(std::int64_t n) : n_(n) {}
    std::int64_t eval() const override { return n_; }
private:
    std::int64_t n_;
};

struct VariableNumberNExp : public Expr<std::int64_t>
{
    VariableNumberNExp(std::int64_t& n) : n_(n) {}
    std::int64_t eval() const override { return n_; }
private:
    std::int64_t& n_;
};

//-------------------------------------------------------------------------------

struct Token
{
    enum Type
    {
        TK_TERNARY_QUEST,
        TK_TERNARY_COLON,
        TK_OR,
        TK_AND,
        TK_EQUAL,
        TK_NOT_EQUAL,
        TK_LESS,
        TK_LESS_EQUAL,
        TK_GREATER,
        TK_GREATER_EQUAL,
        TK_MODULUS,
        TK_VARIABLE_N,
        TK_CONST_NUMBER,
        TK_BRACKET_LEFT,
        TK_BRACKET_RIGHT,
        TK_END
    };

    Token(Type t) : type(t), number(0) {}
    Token(std::int64_t num) : type(TK_CONST_NUMBER), number(num) {}

    Type type;
    std::int64_t number; //if type == TK_CONST_NUMBER
};

class Scanner
{
public:
    Scanner(const std::string& stream) : stream_(stream), pos(stream_.begin())
    {
        tokens.emplace_back("?" , Token::TK_TERNARY_QUEST);
        tokens.emplace_back(":" , Token::TK_TERNARY_COLON);
        tokens.emplace_back("||", Token::TK_OR           );
        tokens.emplace_back("&&", Token::TK_AND          );
        tokens.emplace_back("==", Token::TK_EQUAL        );
        tokens.emplace_back("!=", Token::TK_NOT_EQUAL    );
        tokens.emplace_back("<=", Token::TK_LESS_EQUAL   );
        tokens.emplace_back("<" , Token::TK_LESS         );
        tokens.emplace_back(">=", Token::TK_GREATER_EQUAL);
        tokens.emplace_back(">" , Token::TK_GREATER      );
        tokens.emplace_back("%" , Token::TK_MODULUS      );
        tokens.emplace_back("n" , Token::TK_VARIABLE_N   );
        tokens.emplace_back("N" , Token::TK_VARIABLE_N   );
        tokens.emplace_back("(" , Token::TK_BRACKET_LEFT );
        tokens.emplace_back(")" , Token::TK_BRACKET_RIGHT);
    }

    Token nextToken()
    {
        //skip whitespace
        pos = std::find_if(pos, stream_.end(), [](char c) { return !zen::isWhiteSpace(c); });

        if (pos == stream_.end())
            return Token::TK_END;

        for (auto iter = tokens.begin(); iter != tokens.end(); ++iter)
            if (startsWith(iter->first))
            {
                pos += iter->first.size();
                return Token(iter->second);
            }

        auto digitEnd = std::find_if(pos, stream_.end(), [](char c) { return !zen::isDigit(c); });

        if (digitEnd != pos)
        {
            auto number = zen::stringTo<std::int64_t>(std::string(pos, digitEnd));
            pos = digitEnd;
            return number;
        }

        throw ParsingError(); //unknown token
    }

private:
    bool startsWith(const std::string& prefix) const
    {
        if (stream_.end() - pos < static_cast<ptrdiff_t>(prefix.size()))
            return false;
        return std::equal(prefix.begin(), prefix.end(), pos);
    }

    typedef std::vector<std::pair<std::string, Token::Type>> TokenList;
    TokenList tokens;

    const std::string stream_;
    std::string::const_iterator pos;
};

//-------------------------------------------------------------------------------

class Parser
{
public:
    Parser(const std::string& stream, std::int64_t& n) :
        scn(stream),
        tk(scn.nextToken()),
        n_(n) {}

    std::shared_ptr<Expr<std::int64_t>> parse() //throw ParsingError; return value always bound!
    {
        auto e = std::dynamic_pointer_cast<Expr<std::int64_t>>(parseExpression()); //throw ParsingError
        if (!e)
            throw ParsingError();
        expectToken(Token::TK_END);
        return e;
    }

private:
    std::shared_ptr<Expression> parseExpression() { return parseConditional(); }//throw ParsingError

    std::shared_ptr<Expression> parseConditional() //throw ParsingError
    {
        std::shared_ptr<Expression> e = parseLogicalOr();

        if (token().type == Token::TK_TERNARY_QUEST)
        {
            nextToken();

            auto ifExp   = std::dynamic_pointer_cast<Expr<bool>>(e);
            auto thenExp = std::dynamic_pointer_cast<Expr<std::int64_t>>(parseExpression()); //associativity: <-

            expectToken(Token::TK_TERNARY_COLON);
            nextToken();

            auto elseExp = std::dynamic_pointer_cast<Expr<std::int64_t>>(parseExpression()); //
            if (!ifExp || !thenExp || !elseExp)
                throw ParsingError();
            return std::make_shared<ConditionalExp<std::int64_t>>(ifExp, thenExp, elseExp);
        }
        return e;
    }

    std::shared_ptr<Expression> parseLogicalOr()
    {
        std::shared_ptr<Expression> e = parseLogicalAnd();
        while (token().type == Token::TK_OR) //associativity: ->
        {
            nextToken();

            std::shared_ptr<Expression> rhs = parseLogicalAnd();
            e = makeBiExp(e, rhs, std::logical_or<bool>()); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parseLogicalAnd()
    {
        std::shared_ptr<Expression> e = parseEquality();
        while (token().type == Token::TK_AND) //associativity: ->
        {
            nextToken();
            std::shared_ptr<Expression> rhs = parseEquality();

            e = makeBiExp(e, rhs, std::logical_and<bool>()); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parseEquality()
    {
        std::shared_ptr<Expression> e = parseRelational();

        Token::Type t = token().type;
        if (t == Token::TK_EQUAL || //associativity: n/a
            t == Token::TK_NOT_EQUAL)
        {
            nextToken();
            std::shared_ptr<Expression> rhs = parseRelational();

            if (t == Token::TK_EQUAL)     return makeBiExp(e, rhs, std::equal_to    <std::int64_t>()); //throw ParsingError
            if (t == Token::TK_NOT_EQUAL) return makeBiExp(e, rhs, std::not_equal_to<std::int64_t>()); //
        }
        return e;
    }

    std::shared_ptr<Expression> parseRelational()
    {
        std::shared_ptr<Expression> e = parseMultiplicative();

        Token::Type t = token().type;
        if (t == Token::TK_LESS       || //associativity: n/a
            t == Token::TK_LESS_EQUAL ||
            t == Token::TK_GREATER    ||
            t == Token::TK_GREATER_EQUAL)
        {
            nextToken();
            std::shared_ptr<Expression> rhs = parseMultiplicative();

            if (t == Token::TK_LESS)          return makeBiExp(e, rhs, std::less         <std::int64_t>()); //
            if (t == Token::TK_LESS_EQUAL)    return makeBiExp(e, rhs, std::less_equal   <std::int64_t>()); //throw ParsingError
            if (t == Token::TK_GREATER)       return makeBiExp(e, rhs, std::greater      <std::int64_t>()); //
            if (t == Token::TK_GREATER_EQUAL) return makeBiExp(e, rhs, std::greater_equal<std::int64_t>()); //
        }
        return e;
    }

    std::shared_ptr<Expression> parseMultiplicative()
    {
        std::shared_ptr<Expression> e = parsePrimary();

        while (token().type == Token::TK_MODULUS) //associativity: ->
        {
            nextToken();
            std::shared_ptr<Expression> rhs = parsePrimary();

            //"compile-time" check: n % 0
            if (auto literal = std::dynamic_pointer_cast<ConstNumberExp>(rhs))
                if (literal->eval() == 0)
                    throw ParsingError();

            e = makeBiExp(e, rhs, std::modulus<std::int64_t>()); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parsePrimary()
    {
        if (token().type == Token::TK_VARIABLE_N)
        {
            nextToken();
            return std::make_shared<VariableNumberNExp>(n_);
        }
        else if (token().type == Token::TK_CONST_NUMBER)
        {
            const std::int64_t number = token().number;
            nextToken();
            return std::make_shared<ConstNumberExp>(number);
        }
        else if (token().type == Token::TK_BRACKET_LEFT)
        {
            nextToken();
            std::shared_ptr<Expression> e = parseExpression();

            expectToken(Token::TK_BRACKET_RIGHT);
            nextToken();
            return e;
        }
        else
            throw ParsingError();
    }

    void nextToken() { tk = scn.nextToken(); }
    const Token& token() const { return tk; }

    void expectToken(Token::Type t) //throw ParsingError
    {
        if (token().type != t)
            throw ParsingError();
    }

    Scanner scn;
    Token tk;
    std::int64_t& n_;
};
}


inline
PluralFormInfo::PluralFormInfo(const std::string& definition, int pluralCount) //throw InvalidPluralForm
{
    if (pluralCount < 1)
        throw InvalidPluralForm();

    forms.resize(pluralCount);
    try
    {
        parse_plural::PluralForm pf(definition); //throw parse_plural::ParsingError
        //PERF_START

        //perf: 80ns per iteration max (for arabic)
        //=> 1000 iterations should be fast enough and still detect all "single number forms"
        for (int j = 0; j < 1000; ++j)
        {
            int form = pf.getForm(j);
            if (0 <= form && form < static_cast<int>(forms.size()))
            {
                if (forms[form].count == 0)
                    forms[form].firstNumber = j;
                ++forms[form].count;
            }
            else
                throw InvalidPluralForm();
        }
    }
    catch (const parse_plural::ParsingError&)
    {
        throw InvalidPluralForm();
    }

    //ensure each form is used at least once:
    if (!std::all_of(forms.begin(), forms.end(), [](const FormInfo& fi) { return fi.count >= 1; }))
    throw InvalidPluralForm();
}


inline
PluralForm::PluralForm(const std::string& stream) : expr(implementation::Parser(stream, n_).parse()) {} //throw ParsingError
}

#endif // PARSE_PLURAL_H_INCLUDED
