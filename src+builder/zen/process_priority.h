// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
#ifndef PREVENTSTANDBY_H_83421759082143245
#define PREVENTSTANDBY_H_83421759082143245

#include <memory>
#include "file_error.h"

namespace zen
{
//signal a "busy" state to the operating system
class PreventStandby
{
public:
    PreventStandby(); //throw FileError
    ~PreventStandby();
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;
};

//lower CPU and file I/O priorities
class ScheduleForBackgroundProcessing
{
public:
    ScheduleForBackgroundProcessing(); //throw FileError
    ~ScheduleForBackgroundProcessing();
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;
};
}

#endif //PREVENTSTANDBY_H_83421759082143245
