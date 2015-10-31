// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef VERSION_ID_HEADER_2348769284769242
#define VERSION_ID_HEADER_2348769284769242

#include "../version/version.h"


namespace zen
{
/*constexpr*/ long getInactiveCheckId()
{
    //use current version to calculate a changing number for the inactive state near UTC begin, in order to always check for updates after installing a new version
    //=> convert version into 11-based *unique* number (this breaks lexicographical version ordering, but that's irrelevant!)
    long id = 0;
    const wchar_t* first = zen::ffsVersion;
    const wchar_t* last = first + zen::strLength(ffsVersion);
    std::for_each(first, last, [&](wchar_t c)
    {
        id *= 11;
        if (L'0' <= c && c <= L'9')
            id += c - L'0';
        else
        {
            assert(c == FFS_VERSION_SEPARATOR);
            id += 10;
        }
    });
    assert(0 < id && id < 3600 * 24 * 365); //as long as value is within a year after UTC begin (1970) there's no risk to clash with *current* time
    return id;
}
}

#endif //VERSION_ID_HEADER_2348769284769242
