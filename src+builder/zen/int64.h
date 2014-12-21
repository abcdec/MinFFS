// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FFS_LARGE_64_BIT_INTEGER_H_INCLUDED
#define FFS_LARGE_64_BIT_INTEGER_H_INCLUDED

#include <cstdint>

#ifdef ZEN_WIN
    #include "win.h"
#endif


namespace zen
{
#ifdef ZEN_WIN
inline
std::int64_t get64BitInt(DWORD low, LONG high)
{
    static_assert(sizeof(low) + sizeof(high) == sizeof(std::int64_t), "");

    LARGE_INTEGER cvt = {};
    cvt.LowPart  = low;
    cvt.HighPart = high;
    return cvt.QuadPart;
}

std::int64_t get64BitInt(std::uint64_t low, std::int64_t high) = delete;


inline
std::uint64_t get64BitUInt(DWORD low, DWORD high)
{
    static_assert(sizeof(low) + sizeof(high) == sizeof(std::uint64_t), "");

    ULARGE_INTEGER cvt = {};
    cvt.LowPart  = low;
    cvt.HighPart = high;
    return cvt.QuadPart;
}

std::int64_t get64BitUInt(std::uint64_t low, std::uint64_t high) = delete;


//convert FILETIME (number of 100-nanosecond intervals since January 1, 1601 UTC)
//       to time_t (number of seconds since Jan. 1st 1970 UTC)
//
//FAT32 time is preserved exactly: FAT32 -> toTimeT -> tofiletime -> FAT32
inline
std::int64_t filetimeToTimeT(const FILETIME& ft)
{
    return static_cast<std::int64_t>(get64BitUInt(ft.dwLowDateTime, ft.dwHighDateTime) / 10000000U) - get64BitInt(3054539008UL, 2); //caveat: signed/unsigned arithmetics!
    //timeshift between ansi C time and FILETIME in seconds == 11644473600s
}

inline
FILETIME timetToFileTime(std::int64_t utcTime)
{
    ULARGE_INTEGER cvt = {};
    cvt.QuadPart = (utcTime + get64BitInt(3054539008UL, 2)) * 10000000U; //caveat: signed/unsigned arithmetics!

    const FILETIME output = { cvt.LowPart, cvt.HighPart };
    return output;
}
#endif
}

#endif //FFS_LARGE_64_BIT_INTEGER_H_INCLUDED
