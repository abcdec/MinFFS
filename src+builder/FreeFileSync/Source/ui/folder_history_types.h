// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FOLDER_HIST_TYPES_32481457137432143214
#define FOLDER_HIST_TYPES_32481457137432143214

//#include <ctime>
#include <zen/zstring.h>


namespace zen
{
struct ConfigHistoryItem
{
    explicit ConfigHistoryItem(const Zstring& name) : configFile(name) {}
    ConfigHistoryItem() {}
    Zstring configFile;
    //time_t lastSyncTime;
};
}

#endif //FOLDER_HIST_TYPES_32481457137432143214
