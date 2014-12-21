// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef WARN_STATIC_HEADER_08724567834560832745
#define WARN_STATIC_HEADER_08724567834560832745

/*
Portable Compile-Time Warning
-----------------------------
Usage:
    warn_static("my message")
*/

#ifdef _MSC_VER
#define STATIC_WARNING_MAKE_STRINGIZE_SUB(NUM)   #NUM
#define STATIC_WARNING_MAKE_STRINGIZE(NUM) STATIC_WARNING_MAKE_STRINGIZE_SUB(NUM)

#define warn_static(TXT) \
    __pragma(message (__FILE__ "(" STATIC_WARNING_MAKE_STRINGIZE(__LINE__) "): Warning: " ## TXT))

#elif defined __GNUC__
#define STATIC_WARNING_CONCAT_SUB(X, Y) X ## Y
#define STATIC_WARNING_CONCAT(X, Y) STATIC_WARNING_CONCAT_SUB(X, Y)

#define warn_static(TXT) \
    typedef int STATIC_WARNING_87903124 __attribute__ ((deprecated)); \
    enum { STATIC_WARNING_CONCAT(warn_static_dummy_value, __LINE__) = sizeof(STATIC_WARNING_87903124) };
#endif

#endif //WARN_STATIC_HEADER_08724567834560832745
