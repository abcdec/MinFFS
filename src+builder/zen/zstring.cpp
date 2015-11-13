// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "zstring.h"
#include <stdexcept>

#ifdef ZEN_WIN
    #include "dll.h"
    //#include "win_ver.h"
#endif

using namespace zen;

/*
Perf test: compare strings 10 mio times; 64 bit build
-----------------------------------------------------
    string a = "Fjk84$%kgfj$%T\\\\Gffg\\gsdgf\\fgsx----------d-"
    string b = "fjK84$%kgfj$%T\\\\gfFg\\gsdgf\\fgSy----------dfdf"

Windows (UTF16 wchar_t)
  4 ns | wcscmp
 67 ns | CompareStringOrdinalFunc+ + bIgnoreCase
314 ns | LCMapString + wmemcmp

OS X (UTF8 char)
   6 ns | strcmp
  98 ns | strcasecmp
 120 ns | strncasecmp + std::min(sizeLhs, sizeRhs);
 856 ns | CFStringCreateWithCString       + CFStringCompare(kCFCompareCaseInsensitive)
1110 ns | CFStringCreateWithCStringNoCopy + CFStringCompare(kCFCompareCaseInsensitive)
________________________
time per call | function
*/


#ifdef ZEN_WIN
namespace
{
//try to call "CompareStringOrdinal" for low-level string comparison: unfortunately available not before Windows Vista!
//by a factor ~3 faster than old string comparison using "LCMapString"
typedef int (WINAPI* CompareStringOrdinalFunc)(LPCWSTR lpString1, int cchCount1,
                                               LPCWSTR lpString2, int cchCount2, BOOL bIgnoreCase);
const SysDllFun<CompareStringOrdinalFunc> compareStringOrdinal = SysDllFun<CompareStringOrdinalFunc>(L"kernel32.dll", "CompareStringOrdinal");
//watch for dependencies in global namespace!!!
}


int cmpStringNoCase(const wchar_t* lhs, size_t lhsLen, const wchar_t* rhs, size_t rhsLen)
{
    assert(std::find(lhs, lhs + lhsLen, 0) == lhs + lhsLen); //don't expect embedded nulls!
    assert(std::find(rhs, rhs + rhsLen, 0) == rhs + rhsLen); //

    if (compareStringOrdinal) //this additional test has no noticeable performance impact
    {
        const int rv = compareStringOrdinal(lhs,                      //__in  LPCWSTR lpString1,
                                            static_cast<int>(lhsLen), //__in  int cchCount1,
                                            rhs,                      //__in  LPCWSTR lpString2,
                                            static_cast<int>(rhsLen), //__in  int cchCount2,
                                            true);                    //__in  BOOL bIgnoreCase
        if (rv <= 0)
            throw std::runtime_error("Error comparing strings (CompareStringOrdinal). " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        else
            return rv - 2; //convert to C-style string compare result
    }
    else //fallback
    {
        //do NOT use "CompareString"; this function is NOT accurate (even with LOCALE_INVARIANT and SORT_STRINGSORT): for example "weiﬂ" == "weiss"!!!
        //the only reliable way to compare filepaths (with XP) is to call "CharUpper" or "LCMapString":

        const auto minSize = std::min(lhsLen, rhsLen);

        if (minSize == 0) //LCMapString does not allow input sizes of 0!
            return static_cast<int>(lhsLen) - static_cast<int>(rhsLen);

        auto copyToUpperCase = [minSize](const wchar_t* strIn, wchar_t* strOut)
        {
            //faster than CharUpperBuff + wmemcpy or CharUpper + wmemcpy and same speed like ::CompareString()
            if (::LCMapString(LOCALE_INVARIANT,          //__in   LCID Locale,
                              LCMAP_UPPERCASE,           //__in   DWORD dwMapFlags,
                              strIn,                     //__in   LPCTSTR lpSrcStr,
                              static_cast<int>(minSize), //__in   int cchSrc,
                              strOut,                    //__out  LPTSTR lpDestStr,
                              static_cast<int>(minSize)) == 0) //__in   int cchDest
                throw std::runtime_error("Error comparing strings (LCMapString). " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        };

        auto eval = [&](wchar_t* bufL, wchar_t* bufR)
        {
            copyToUpperCase(lhs, bufL);
            copyToUpperCase(rhs, bufR);

            const int rv = ::wcsncmp(bufL, bufR, minSize);
            if (rv != 0)
                return rv;

            return static_cast<int>(lhsLen) - static_cast<int>(rhsLen);
        };

        if (minSize <= MAX_PATH) //performance optimization: stack
        {
            wchar_t bufferL[MAX_PATH] = {};
            wchar_t bufferR[MAX_PATH] = {};
            return eval(bufferL, bufferR);
        }
        else //use freestore
        {
            std::vector<wchar_t> buffer(2 * minSize);
            return eval(&buffer[0], &buffer[minSize]);
        }
    }
}


void makeUpperInPlace(wchar_t* str, size_t strLen)
{
    //- use Windows' upper case conversion: faster than ::CharUpper()
    //- LOCALE_INVARIANT is NOT available with Windows 2000 -> ok
    //- MSDN: "The destination string can be the same as the source string only if LCMAP_UPPERCASE or LCMAP_LOWERCASE is set"
    if (strLen != 0) //LCMapString does not allow input sizes of 0!
        if (::LCMapString(LOCALE_INVARIANT, //__in   LCID Locale,
                          LCMAP_UPPERCASE,  //__in   DWORD dwMapFlags,
                          str,              //__in   LPCTSTR lpSrcStr,
                          static_cast<int>(strLen), //__in   int cchSrc,
                          str,              //__out  LPTSTR lpDestStr,
                          static_cast<int>(strLen)) == 0) //__in   int cchDest
            throw std::runtime_error("Error comparing strings (LCMapString). " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}
#endif
