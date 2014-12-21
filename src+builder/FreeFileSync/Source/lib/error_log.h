// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ERROR_LOG_89734181783491324134
#define ERROR_LOG_89734181783491324134

#include <cassert>
#include <zen/serialize.h>
#include <zen/time.h>
#include "ffs_paths.h"


namespace zen
{
//write error message to a file (even with corrupted stack)- call in desperate situations when no other means of error handling is available
void logError(const std::string& msg); //throw()









//##################### implementation ############################
inline
void logError(const std::string& msg) //throw()
{
    assert(false); //this is stuff we like to debug
    const std::string logEntry = "[" + formatTime<std::string>(FORMAT_DATE) + " "+ formatTime<std::string>(FORMAT_TIME) + "] " + msg;
    try
    {
        saveBinStream(getConfigDir() + Zstr("LastError.log"), logEntry, nullptr); //throw FileError
    }
    catch (const FileError&) {}
}
}

#endif //ERROR_LOG_89734181783491324134
