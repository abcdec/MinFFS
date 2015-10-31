// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.15        *
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

#ifndef ZEN_TIME_HEADER_845709281432434
#define ZEN_TIME_HEADER_845709281432434

#include <ctime>
#include "string_tools.h"

namespace zen
{
struct TimeComp //replaces "struct std::tm" and SYSTEMTIME
{
    TimeComp() : year(0), month(0), day(0), hour(0), minute(0), second(0) {}

    int year;   // -
    int month;  //1-12
    int day;    //1-31
    int hour;   //0-23
    int minute; //0-59
    int second; //0-61
};

TimeComp localTime   (time_t utc = std::time(nullptr)); //convert time_t (UTC) to local time components
time_t   localToTimeT(const TimeComp& comp);            //convert local time components to time_t (UTC), returns -1 on error

//----------------------------------------------------------------------------------------------------------------------------------

/*
format (current) date and time; example:
        formatTime<std::wstring>(L"%Y*%m*%d");     -> "2011*10*29"
        formatTime<std::wstring>(FORMAT_DATE);     -> "2011-10-29"
        formatTime<std::wstring>(FORMAT_TIME);     -> "17:55:34"
*/
template <class String, class String2>
String formatTime(const String2& format, const TimeComp& comp = localTime()); //format as specified by "std::strftime", returns empty string on failure

//the "format" parameter of formatTime() is partially specialized with the following type tags:
const struct FormatDateTag     {} FORMAT_DATE      = {}; //%x - locale dependent date representation: e.g. 08/23/01
const struct FormatTimeTag     {} FORMAT_TIME      = {}; //%X - locale dependent time representation: e.g. 14:55:02
const struct FormatDateTimeTag {} FORMAT_DATE_TIME = {}; //%c - locale dependent date and time:       e.g. Thu Aug 23 14:55:02 2001

const struct FormatIsoDateTag     {} FORMAT_ISO_DATE      = {}; //%Y-%m-%d          - e.g. 2001-08-23
const struct FormatIsoTimeTag     {} FORMAT_ISO_TIME      = {}; //%H:%M:%S          - e.g. 14:55:02
const struct FormatIsoDateTimeTag {} FORMAT_ISO_DATE_TIME = {}; //%Y-%m-%d %H:%M:%S - e.g. 2001-08-23 14:55:02

//----------------------------------------------------------------------------------------------------------------------------------

template <class String, class String2>
bool parseTime(const String& format, const String2& str, TimeComp& comp); //similar to ::strptime(), return true on success

//----------------------------------------------------------------------------------------------------------------------------------
















//############################ implementation ##############################
namespace implementation
{
inline
struct std::tm toClibTimeComponents(const TimeComp& comp)
{
    assert(1 <= comp.month  && comp.month  <= 12 &&
           1 <= comp.day    && comp.day    <= 31 &&
           0 <= comp.hour   && comp.hour   <= 23 &&
           0 <= comp.minute && comp.minute <= 59 &&
           0 <= comp.second && comp.second <= 61);

    struct std::tm ctc = {};
    ctc.tm_year  = comp.year - 1900; //years since 1900
    ctc.tm_mon   = comp.month - 1;   //0-11
    ctc.tm_mday  = comp.day;         //1-31
    ctc.tm_hour  = comp.hour;        //0-23
    ctc.tm_min   = comp.minute;      //0-59
    ctc.tm_sec   = comp.second;      //0-61
    ctc.tm_isdst = -1;               //> 0 if DST is active, == 0 if DST is not active, < 0 if the information is not available
    return ctc;
}

inline
TimeComp toZenTimeComponents(const struct ::tm& ctc)
{
    TimeComp comp;
    comp.year   = ctc.tm_year + 1900;
    comp.month  = ctc.tm_mon + 1;
    comp.day    = ctc.tm_mday;
    comp.hour   = ctc.tm_hour;
    comp.minute = ctc.tm_min;
    comp.second = ctc.tm_sec;
    return comp;
}


template <class T>
struct GetFormat; //get default time formats as char* or wchar_t*

template <>
struct GetFormat<FormatDateTag> //%x - locale dependent date representation: e.g. 08/23/01
{
    const char*    format(char)    const { return  "%x"; }
    const wchar_t* format(wchar_t) const { return L"%x"; }
};

template <>
struct GetFormat<FormatTimeTag> //%X - locale dependent time representation: e.g. 14:55:02
{
    const char*    format(char)    const { return  "%X"; }
    const wchar_t* format(wchar_t) const { return L"%X"; }
};

template <>
struct GetFormat<FormatDateTimeTag> //%c - locale dependent date and time:       e.g. Thu Aug 23 14:55:02 2001
{
    const char*    format(char)    const { return  "%c"; }
    const wchar_t* format(wchar_t) const { return L"%c"; }
};

template <>
struct GetFormat<FormatIsoDateTag> //%Y-%m-%d - e.g. 2001-08-23
{
    const char*    format(char)    const { return  "%Y-%m-%d"; }
    const wchar_t* format(wchar_t) const { return L"%Y-%m-%d"; }
};

template <>
struct GetFormat<FormatIsoTimeTag> //%H:%M:%S - e.g. 14:55:02
{
    const char*    format(char)    const { return  "%H:%M:%S"; }
    const wchar_t* format(wchar_t) const { return L"%H:%M:%S"; }
};

template <>
struct GetFormat<FormatIsoDateTimeTag> //%Y-%m-%d %H:%M:%S - e.g. 2001-08-23 14:55:02
{
    const char*    format(char)    const { return  "%Y-%m-%d %H:%M:%S"; }
    const wchar_t* format(wchar_t) const { return L"%Y-%m-%d %H:%M:%S"; }
};


//strftime() craziness on invalid input:
//	VS 2010: CRASH unless "_invalid_parameter_handler" is set: http://msdn.microsoft.com/en-us/library/ksazx244.aspx
//	GCC: returns 0, apparently no crash. Still, considering some clib maintainer's comments, we should expect the worst!
inline
size_t strftimeWrap_impl(char* buffer, size_t bufferSize, const char* format, const struct std::tm* timeptr)
{
    return std::strftime(buffer, bufferSize, format, timeptr);
}


inline
size_t strftimeWrap_impl(wchar_t* buffer, size_t bufferSize, const wchar_t* format, const struct std::tm* timeptr)
{
    return std::wcsftime(buffer, bufferSize, format, timeptr);
}

/*
inline
bool isValid(const struct std::tm& t)
{
	 -> not enough! MSCRT has different limits than the C standard which even seem to change with different versions:
		_VALIDATE_RETURN((( timeptr->tm_sec >=0 ) && ( timeptr->tm_sec <= 59 ) ), EINVAL, FALSE)
		_VALIDATE_RETURN(( timeptr->tm_year >= -1900 ) && ( timeptr->tm_year <= 8099 ), EINVAL, FALSE)
	-> also std::mktime does *not* help here at all!

	auto inRange = [](int value, int minVal, int maxVal) { return minVal <= value && value <= maxVal; };

    //http://www.cplusplus.com/reference/clibrary/ctime/tm/
    return inRange(t.tm_sec , 0, 61) &&
           inRange(t.tm_min , 0, 59) &&
           inRange(t.tm_hour, 0, 23) &&
           inRange(t.tm_mday, 1, 31) &&
           inRange(t.tm_mon , 0, 11) &&
           //tm_year
           inRange(t.tm_wday, 0, 6) &&
           inRange(t.tm_yday, 0, 365);
    //tm_isdst
};
*/

template <class CharType> inline
size_t strftimeWrap(CharType* buffer, size_t bufferSize, const CharType* format, const struct std::tm* timeptr)
{
#if defined _MSC_VER && !defined NDEBUG
    //there's no way around: application init must register an invalid parameter handler that does nothing !!!
    //=> strftime will abort with 0 and set errno to EINVAL instead of CRASHING THE APPLICATION!
    _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(nullptr);
    assert(oldHandler);
    _set_invalid_parameter_handler(oldHandler);
#endif

    return strftimeWrap_impl(buffer, bufferSize, format, timeptr);
}


struct UserDefinedFormatTag {};
struct PredefinedFormatTag  {};

template <class String, class String2> inline
String formatTime(const String2& format, const TimeComp& comp, UserDefinedFormatTag) //format as specified by "std::strftime", returns empty string on failure
{
    typedef typename GetCharType<String>::Type CharType;
    struct std::tm ctc = toClibTimeComponents(comp);
    std::mktime(&ctc); // unfortunately std::strftime() needs all elements of "struct tm" filled, e.g. tm_wday, tm_yday
    //note: although std::mktime() explicitly expects "local time", calculating weekday and day of year *should* be time-zone and DST independent

	CharType buffer[256] = {};
    const size_t charsWritten = strftimeWrap(buffer, 256, strBegin(format), &ctc);
    return String(buffer, charsWritten);
}

template <class String, class FormatType> inline
String formatTime(FormatType, const TimeComp& comp, PredefinedFormatTag)
{
    typedef typename GetCharType<String>::Type CharType;
    return formatTime<String>(GetFormat<FormatType>().format(CharType()), comp, UserDefinedFormatTag());
}
}


inline
TimeComp localTime(time_t utc)
{
    struct ::tm lt = {};

	//use thread-safe variants of localtime()!
#ifdef MinFFS_PATCH
    // No localtime_s in MinGW. Use localtime() which is thread safe.
    struct ::tm *ltp = ::localtime(&utc);
    if (ltp == nullptr) {
        return TimeComp();
    }
    lt = *ltp;
#else
#ifdef ZEN_WIN
    if (::localtime_s(&lt, &utc) != 0)
#else
	if (::localtime_r(&utc, &lt) == nullptr)
#endif
        return TimeComp();
#endif//MinFFS_PATCH

    return implementation::toZenTimeComponents(lt);
}


inline
time_t localToTimeT(const TimeComp& comp) //returns -1 on error
{
    struct std::tm ctc = implementation::toClibTimeComponents(comp);
    return std::mktime(&ctc);
}


template <class String, class String2> inline
String formatTime(const String2& format, const TimeComp& comp)
{
    typedef typename SelectIf<
    IsSameType<String2, FormatDateTag       >::value ||
    IsSameType<String2, FormatTimeTag       >::value ||
    IsSameType<String2, FormatDateTimeTag   >::value ||
    IsSameType<String2, FormatIsoDateTag    >::value ||
    IsSameType<String2, FormatIsoTimeTag    >::value ||
    IsSameType<String2, FormatIsoDateTimeTag>::value, implementation::PredefinedFormatTag, implementation::UserDefinedFormatTag>::Type FormatTag;

    return implementation::formatTime<String>(format, comp, FormatTag());
}


template <class String, class String2>
bool parseTime(const String& format, const String2& str, TimeComp& comp) //return true on success
{
    typedef typename GetCharType<String>::Type CharType;
    static_assert(IsSameType<CharType, typename GetCharType<String2>::Type>::value, "");

    const CharType*       iterFmt = strBegin(format);
    const CharType* const fmtLast = iterFmt + strLength(format);

    const CharType*       iterStr = strBegin(str);
    const CharType* const strLast = iterStr + strLength(str);

    auto extractNumber = [&](int& result, size_t digitCount) -> bool
    {
        if (strLast - iterStr < makeSigned(digitCount))
            return false;

        if (std::any_of(iterStr, iterStr + digitCount, [](CharType c) { return !isDigit(c); }))
        return false;

        result = zen::stringTo<int>(StringRef<CharType>(iterStr, iterStr + digitCount));
        iterStr += digitCount;
        return true;
    };

    for (; iterFmt != fmtLast; ++iterFmt)
    {
        const CharType fmt = *iterFmt;

        if (fmt == '%')
        {
            ++iterFmt;
            if (iterFmt == fmtLast)
                return false;

            switch (*iterFmt)
            {
                case 'Y':
                    if (!extractNumber(comp.year, 4))
                        return false;
                    break;
                case 'm':
                    if (!extractNumber(comp.month, 2))
                        return false;
                    break;
                case 'd':
                    if (!extractNumber(comp.day, 2))
                        return false;
                    break;
                case 'H':
                    if (!extractNumber(comp.hour, 2))
                        return false;
                    break;
                case 'M':
                    if (!extractNumber(comp.minute, 2))
                        return false;
                    break;
                case 'S':
                    if (!extractNumber(comp.second, 2))
                        return false;
                    break;
                default:
                    return false;
            }
        }
        else if (isWhiteSpace(fmt)) //single whitespace in format => skip 0..n whitespace chars
        {
            while (iterStr != strLast && isWhiteSpace(*iterStr))
                ++iterStr;
        }
        else
        {
            if (iterStr == strLast || *iterStr != fmt)
                return false;
            ++iterStr;
        }
    }

    return iterStr == strLast;
}
}

#endif //ZEN_TIME_HEADER_845709281432434
