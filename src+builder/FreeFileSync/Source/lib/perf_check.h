// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef STATISTICS_H_INCLUDED
#define STATISTICS_H_INCLUDED

#include <cstdint>
#include <map>
#include <string>
#include <zen/optional.h>

class PerfCheck
{
public:
    PerfCheck(unsigned int windowSizeRemainingTime, //unit: [ms]
              unsigned int windowSizeSpeed);        //
    ~PerfCheck();

    void addSample(int itemsCurrent, double dataCurrent, int64_t timeMs); //timeMs must be ascending!

    zen::Opt<double> getRemainingTimeSec(double dataRemaining) const;
    zen::Opt<std::wstring> getBytesPerSecond() const; //for window
    zen::Opt<std::wstring> getItemsPerSecond() const; //for window

private:
    struct Record
    {
        Record(int itemCount, double data) : itemCount_(itemCount), data_(data) {}
        int itemCount_;
        double data_; //unit: [bytes]
    };

    std::pair<const std::multimap<int64_t, Record>::value_type*,
        const std::multimap<int64_t, Record>::value_type*> getBlockFromEnd(int64_t windowSize) const;

    const int64_t windowSizeRemTime; //unit: [ms]
    const int64_t windowSizeSpeed_;  //
    const int64_t windowMax;

    std::map<int64_t, Record> samples; //time, unit: [ms]
};

#endif // STATISTICS_H_INCLUDED
