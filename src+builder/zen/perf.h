// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PERF_H_83947184145342652456
#define PERF_H_83947184145342652456

#include "deprecate.h"
#include "tick_count.h"
#include "scope_guard.h"

#ifdef ZEN_WIN
    #include <sstream>
#else
    #include <iostream>
#endif


//############# two macros for quick performance measurements ###############
#define PERF_START zen::PerfTimer perfTest;
#define PERF_STOP  perfTest.showResult();
//###########################################################################

namespace zen
{
class PerfTimer
{
public:
    class TimerError {};

    ZEN_DEPRECATE
    PerfTimer() : startTime(getTicksNow()) //throw TimerError
    {
        //std::clock() - "counts CPU time in Linux GCC and wall time in VC++" - WTF!???
        if (ticksPerSec_ == 0)
            throw TimerError();
    }

    ~PerfTimer() { if (!resultShown) try { showResult(); } catch (TimerError&) { assert(false); } }

    void pause()
    {
        if (!paused)
        {
            paused = true;
            elapsedUntilPause += dist(startTime, getTicksNow());
        }
    }

    void resume()
    {
        if (paused)
        {
            paused = false;
            startTime = getTicksNow();
        }
    }

    void restart()
    {
        startTime = getTicksNow();
        paused = false;
        elapsedUntilPause = 0;
    }

    int64_t timeMs() const
    {
        int64_t ticksTotal = elapsedUntilPause;
        if (!paused)
            ticksTotal += dist(startTime, getTicksNow());
        return 1000 * ticksTotal / ticksPerSec_;
    }

    void showResult()
    {
        const bool wasRunning = !paused;
        if (wasRunning) pause(); //don't include call to MessageBox()!
        ZEN_ON_SCOPE_EXIT(if (wasRunning) resume());

#ifdef ZEN_WIN
        std::wostringstream ss;
        ss << timeMs() << L" ms";
        ::MessageBox(nullptr, ss.str().c_str(), L"Timer", MB_OK);
#else
        std::clog << "Perf: duration: " << timeMs() << " ms\n";
#endif
        resultShown = true;
    }

private:
    TickVal getTicksNow() const
    {
        const TickVal now = getTicks();
        if (!now.isValid())
            throw TimerError();
        return now;
    }

    const std::int64_t ticksPerSec_ = ticksPerSec(); //return 0 on error
    bool resultShown = false;
    TickVal startTime;
    bool paused = false;
    int64_t elapsedUntilPause = 0;
};
}

#endif //PERF_H_83947184145342652456
