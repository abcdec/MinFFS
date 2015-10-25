// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ZSTRING_H_INCLUDED_73425873425789
#define ZSTRING_H_INCLUDED_73425873425789

#include "string_base.h"

#ifdef ZEN_LINUX
    #include <cstring> //strcmp
#endif


#ifndef NDEBUG
namespace z_impl
{
void leakCheckerInsert(const void* ptr, size_t size);
void leakCheckerRemove(const void* ptr);
}
#endif //NDEBUG

class AllocatorFreeStoreChecked
{
public:
    static void* allocate(size_t size) //throw std::bad_alloc
    {
        void* ptr = zen::AllocatorOptimalSpeed::allocate(size);
#ifndef NDEBUG
        z_impl::leakCheckerInsert(ptr, size); //test Zbase for memory leaks
#endif
        return ptr;
    }

    static void deallocate(void* ptr)
    {
#ifndef NDEBUG
        z_impl::leakCheckerRemove(ptr); //check for memory leaks
#endif
        zen::AllocatorOptimalSpeed::deallocate(ptr);
    }

    static size_t calcCapacity(size_t length) { return zen::AllocatorOptimalSpeed::calcCapacity(length); }
};


//############################## helper functions #############################################

#ifdef ZEN_WIN //Windows encodes Unicode as UTF-16 wchar_t
    typedef wchar_t Zchar;
    #define Zstr(x) L ## x
    const Zchar FILE_NAME_SEPARATOR = L'\\';

#elif defined ZEN_LINUX || defined ZEN_MAC //Linux uses UTF-8
    typedef char Zchar;
    #define Zstr(x) x
    const Zchar FILE_NAME_SEPARATOR = '/';
#endif

//"The reason for all the fuss above" - Loki/SmartPtr
//a high-performance string for interfacing with native OS APIs and multithreaded contexts
typedef zen::Zbase<Zchar, zen::StorageRefCountThreadSafe, AllocatorFreeStoreChecked> Zstring;



//Compare filepaths: Windows does NOT distinguish between upper/lower-case, while Linux DOES
int cmpFileName(const Zstring& lhs, const Zstring& rhs);

struct LessFilename //case-insensitive on Windows, case-sensitive on Linux
{
    bool operator()(const Zstring& lhs, const Zstring& rhs) const { return cmpFileName(lhs, rhs) < 0; }
};

struct EqualFilename //case-insensitive on Windows, case-sensitive on Linux
{
    bool operator()(const Zstring& lhs, const Zstring& rhs) const { return cmpFileName(lhs, rhs) == 0; }
};

#if defined ZEN_WIN || defined ZEN_MAC
    Zstring makeUpperCopy(const Zstring& str);
#endif

inline
Zstring appendSeparator(Zstring path) //support rvalue references!
{
    return zen::endsWith(path, FILE_NAME_SEPARATOR) ? path : (path += FILE_NAME_SEPARATOR);
}






//################################# inline implementation ########################################

#ifdef ZEN_LINUX
inline
int cmpFileName(const Zstring& lhs, const Zstring& rhs)
{
    return std::strcmp(lhs.c_str(), rhs.c_str()); //POSIX filepaths don't have embedded 0
}
#endif

#endif //ZSTRING_H_INCLUDED_73425873425789
