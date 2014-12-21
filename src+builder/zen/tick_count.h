// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ZEN_TICK_COUNT_HEADER_3807326
#define ZEN_TICK_COUNT_HEADER_3807326

#include <cstdint>
#include "type_traits.h"
#include "basic_math.h"

#ifdef ZEN_WIN
    #include "win.h" //includes "windows.h"

#elif defined ZEN_LINUX
    #include <time.h> //Posix ::clock_gettime()

#elif defined ZEN_MAC
    #include <mach/mach_time.h>
#endif
//template <class T> inline
//T dist(T a, T b)
//{
//    return a > b ? a - b : b - a;
//}

namespace zen
{
//a portable "GetTickCount()" using "wall time equivalent" - e.g. no jumps due to ntp time corrections
class TickVal;
int64_t dist(const TickVal& lhs, const TickVal& rhs); //use absolute difference for paranoid security: even QueryPerformanceCounter "wraps-around" at *some* time

int64_t ticksPerSec(); //return 0 on error
TickVal getTicks();    //return invalid value on error: !TickVal::isValid()









//############################ implementation ##############################
class TickVal
{
public:
#ifdef ZEN_WIN
    typedef LARGE_INTEGER NativeVal;
#elif defined ZEN_LINUX
    typedef timespec NativeVal;
#elif defined ZEN_MAC
    typedef uint64_t NativeVal;
#endif

    TickVal() : val_() {}
    explicit TickVal(const NativeVal& val) : val_(val) {}

    inline friend
    int64_t dist(const TickVal& lhs, const TickVal& rhs)
    {
#ifdef ZEN_WIN
        return numeric::dist(lhs.val_.QuadPart, rhs.val_.QuadPart); //std::abs(a - b) can lead to overflow!
#elif defined ZEN_LINUX
        //structure timespec documented with members:
        //	time_t  tv_sec    seconds
        //	long    tv_nsec   nanoseconds
        const int64_t deltaSec  = lhs.val_.tv_sec  - rhs.val_.tv_sec;
        const int64_t deltaNsec = lhs.val_.tv_nsec - rhs.val_.tv_nsec;
        return numeric::abs(deltaSec * 1000000000 + deltaNsec);
#elif defined ZEN_MAC
        return numeric::dist(lhs.val_, rhs.val_);
#endif
    }

    inline friend
    bool operator<(const TickVal& lhs, const TickVal& rhs)
    {
#ifdef ZEN_WIN
        return lhs.val_.QuadPart < rhs.val_.QuadPart;
#elif defined ZEN_LINUX
        if (lhs.val_.tv_sec != rhs.val_.tv_sec)
            return lhs.val_.tv_sec < rhs.val_.tv_sec;
        return lhs.val_.tv_nsec < rhs.val_.tv_nsec;
#elif defined ZEN_MAC
        return lhs.val_ < rhs.val_;
#endif
    }

    bool isValid() const { return dist(*this, TickVal()) != 0; }

private:
    NativeVal val_;
};


inline
int64_t ticksPerSec() //return 0 on error
{
#ifdef ZEN_WIN
    LARGE_INTEGER frequency = {};
    if (!::QueryPerformanceFrequency(&frequency)) //MSDN promises: "The frequency cannot change while the system is running."
        return 0; //MSDN: "This won't occur on any system that runs Windows XP or later."
    static_assert(sizeof(int64_t) >= sizeof(frequency.QuadPart), "");
    return frequency.QuadPart;

#elif defined ZEN_LINUX
    return 1000000000; //precision: nanoseconds

#elif defined ZEN_MAC
    mach_timebase_info_data_t tbi = {};
    if (::mach_timebase_info(&tbi) != KERN_SUCCESS)
        return 0;
    //structure mach_timebase_info_data_t documented with members:
    //		uint32_t	numer;
    //		uint32_t	denom;
    return static_cast<int64_t>(1000000000) * tbi.denom / tbi.numer;
#endif
}


inline
TickVal getTicks() //return !isValid() on error
{
#ifdef ZEN_WIN
    LARGE_INTEGER now = {};
    if (!::QueryPerformanceCounter(&now))
        return TickVal();
    //detailed info about QPC: http://msdn.microsoft.com/en-us/library/windows/desktop/dn553408%28v=vs.85%29.aspx
    //- MSDN: "No need to set the thread affinity"

#elif defined ZEN_LINUX
    //gettimeofday() seems fine but is deprecated
    timespec now = {};
    if (::clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0) //CLOCK_MONOTONIC measures time reliably across processors!
        return TickVal();

#elif defined ZEN_MAC
    uint64_t now = ::mach_absolute_time(); //can this call fail???
#endif
    return TickVal(now);
}
}

#endif //ZEN_TICK_COUNT_HEADER_3807326
