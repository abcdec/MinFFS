// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "zstring.h"
#include <stdexcept>
#include <unordered_map>

#ifdef ZEN_WIN
    #include "dll.h"
    #include "win_ver.h"

#elif defined ZEN_MAC
    #include <ctype.h> //toupper()
#endif

#ifndef NDEBUG
    #include <mutex>
    #include <iostream>
#endif

using namespace zen;


#ifndef NDEBUG
namespace
{
class LeakChecker //small test for memory leaks
{
public:
    static LeakChecker& get()
    {
        //meyers singleton: avoid static initialization order problem in global namespace!
        static LeakChecker inst;
        return inst;
    }

    void insert(const void* ptr, size_t size)
    {
        std::lock_guard<std::mutex> dummy(lockActStrings);
        if (!activeStrings.emplace(ptr, size).second)
            reportProblem("Serious Error: New memory points into occupied space: " + rawMemToString(ptr, size));
    }

    void remove(const void* ptr)
    {
        std::lock_guard<std::mutex> dummy(lockActStrings);
        if (activeStrings.erase(ptr) != 1)
            reportProblem("Serious Error: No memory available for deallocation at this location!");
    }

private:
    LeakChecker() {}

    ~LeakChecker()
    {
        if (!activeStrings.empty())
        {
            std::string leakingStrings;

            int items = 0;
            for (auto it = activeStrings.begin(); it != activeStrings.end() && items < 20; ++it, ++items)
                leakingStrings += "\"" + rawMemToString(it->first, it->second) + "\"\n";

            const std::string message = std::string("Memory leak detected!") + "\n\n"
                                        + "Candidates:\n" + leakingStrings;
#ifdef ZEN_WIN
            MessageBoxA(nullptr, message.c_str(), "Error", MB_SERVICE_NOTIFICATION | MB_ICONERROR);
#else
            std::cerr << message;
            std::abort();
#endif
        }
    }

    LeakChecker           (const LeakChecker&) = delete;
    LeakChecker& operator=(const LeakChecker&) = delete;

    static std::string rawMemToString(const void* ptr, size_t size)
    {
        std::string output(reinterpret_cast<const char*>(ptr), std::min<size_t>(size, 100));
        replace(output, '\0', ' '); //don't stop at 0-termination
        return output;
    }

    void reportProblem(const std::string& message) //throw std::logic_error
    {
#ifdef ZEN_WIN
        ::MessageBoxA(nullptr, message.c_str(), "Error", MB_SERVICE_NOTIFICATION | MB_ICONERROR);
#else
        std::cerr << message;
#endif
        throw std::logic_error("Memory leak! " + message);
    }

    std::mutex lockActStrings;
    std::unordered_map<const void*, size_t> activeStrings;
};

//caveat: function scope static initialization is not thread-safe in VS 2010!
auto& dummy = LeakChecker::get();
}

void z_impl::leakCheckerInsert(const void* ptr, size_t size) { LeakChecker::get().insert(ptr, size); }
void z_impl::leakCheckerRemove(const void* ptr             ) { LeakChecker::get().remove(ptr); }
#endif //NDEBUG


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
#ifdef MinFFS_PATCH // Define LOCALE_INVARIANT if not defined
// LOCALE_INVARIANT not defined when compiling for OS 5.0 (Windows 2000) so compilation fails with original code.
// Value is defined http://msdn.microsoft.com/en-us/library/windows/desktop/dd373795%28v=vs.85%29.aspx
#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT 0x007f
#endif//LOCALE_INVARIANT
#endif//MinFFS_PATCH
//warning: LOCALE_INVARIANT is NOT available with Windows 2000, so we have to make yet another distinction...
const LCID ZSTRING_INVARIANT_LOCALE = zen::winXpOrLater() ?
                                      LOCALE_INVARIANT :
                                      MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT); //see: http://msdn.microsoft.com/en-us/goglobal/bb688122.aspx

//try to call "CompareStringOrdinal" for low-level string comparison: unfortunately available not before Windows Vista!
//by a factor ~3 faster than old string comparison using "LCMapString"
typedef int (WINAPI* CompareStringOrdinalFunc)(LPCWSTR lpString1, int cchCount1,
                                               LPCWSTR lpString2, int cchCount2, BOOL bIgnoreCase);
const SysDllFun<CompareStringOrdinalFunc> compareStringOrdinal = SysDllFun<CompareStringOrdinalFunc>(L"kernel32.dll", "CompareStringOrdinal");
//caveat: function scope static initialization is not thread-safe in VS 2010!
//No global dependencies => no static initialization order problem in global namespace!
}


int cmpFileName(const Zstring& lhs, const Zstring& rhs)
{
    if (compareStringOrdinal) //this additional test has no noticeable performance impact
    {
        const int rv = compareStringOrdinal(lhs.c_str(),                  //__in  LPCWSTR lpString1,
                                            static_cast<int>(lhs.size()), //__in  int cchCount1,
                                            rhs.c_str(),                  //__in  LPCWSTR lpString2,
                                            static_cast<int>(rhs.size()), //__in  int cchCount2,
                                            true);                        //__in  BOOL bIgnoreCase
        if (rv <= 0)
            throw std::runtime_error("Error comparing strings (CompareStringOrdinal).");
        else
            return rv - 2; //convert to C-style string compare result
    }
    else //fallback
    {
        //do NOT use "CompareString"; this function is NOT accurate (even with LOCALE_INVARIANT and SORT_STRINGSORT): for example "weiß" == "weiss"!!!
        //the only reliable way to compare filepaths (with XP) is to call "CharUpper" or "LCMapString":

        const size_t sizeLhs = lhs.size();
        const size_t sizeRhs = rhs.size();

        const auto minSize = std::min(sizeLhs, sizeRhs);

        if (minSize == 0) //LCMapString does not allow input sizes of 0!
            return static_cast<int>(sizeLhs) - static_cast<int>(sizeRhs);

        auto copyToUpperCase = [&](const wchar_t* strIn, wchar_t* strOut)
        {
            //faster than CharUpperBuff + wmemcpy or CharUpper + wmemcpy and same speed like ::CompareString()
            if (::LCMapString(ZSTRING_INVARIANT_LOCALE,  //__in   LCID Locale,
                              LCMAP_UPPERCASE,           //__in   DWORD dwMapFlags,
                              strIn,                     //__in   LPCTSTR lpSrcStr,
                              static_cast<int>(minSize), //__in   int cchSrc,
                              strOut,                    //__out  LPTSTR lpDestStr,
                              static_cast<int>(minSize)) == 0) //__in   int cchDest
                throw std::runtime_error("Error comparing strings (LCMapString).");
        };

        auto eval = [&](wchar_t* bufL, wchar_t* bufR)
        {
            copyToUpperCase(lhs.c_str(), bufL);
            copyToUpperCase(rhs.c_str(), bufR);

            const int rv = ::wmemcmp(bufL, bufR, minSize);
            if (rv != 0)
                return rv;

            return static_cast<int>(sizeLhs) - static_cast<int>(sizeRhs);
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


Zstring makeUpperCopy(const Zstring& str)
{
    const int len = static_cast<int>(str.size());

    if (len == 0) //LCMapString does not allow input sizes of 0!
        return str;

    Zstring output;
    output.resize(len);

    //use Windows' upper case conversion: faster than ::CharUpper()
    if (::LCMapString(ZSTRING_INVARIANT_LOCALE, //__in   LCID Locale,
                      LCMAP_UPPERCASE,          //__in   DWORD dwMapFlags,
                      str.c_str(),              //__in   LPCTSTR lpSrcStr,
                      len,                      //__in   int cchSrc,
                      &*output.begin(),         //__out  LPTSTR lpDestStr,
                      len) == 0)                //__in   int cchDest
        throw std::runtime_error("Error comparing strings (LCMapString).");

    return output;
}


#elif defined ZEN_MAC
int cmpFileName(const Zstring& lhs, const Zstring& rhs)
{
    return ::strcasecmp(lhs.c_str(), rhs.c_str()); //locale-dependent!
}


Zstring makeUpperCopy(const Zstring& str)
{
    const size_t len = str.size();

    Zstring output;
    output.resize(len);

    std::transform(str.begin(), str.end(), output.begin(), [](char c) { return static_cast<char>(::toupper(static_cast<unsigned char>(c))); }); //locale-dependent!

    //result of toupper() is an unsigned char mapped to int range, so the char representation is in the last 8 bits and we need not care about signedness!
    //this should work for UTF-8, too: all chars >= 128 are mapped upon themselves!

    return output;
}
#endif
