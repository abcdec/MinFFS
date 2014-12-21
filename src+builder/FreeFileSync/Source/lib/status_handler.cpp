// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "status_handler.h"
#include <zen/tick_count.h>

using namespace zen;


namespace
{
const std::int64_t TICKS_UPDATE_INTERVAL = UI_UPDATE_INTERVAL* ticksPerSec() / 1000;
TickVal lastExec = getTicks();
};

bool zen::updateUiIsAllowed()
{
    const TickVal now = getTicks(); //0 on error
    if (dist(lastExec, now) >= TICKS_UPDATE_INTERVAL) //perform ui updates not more often than necessary
    {
        lastExec = now;
        return true;
    }
    return false;
}
