// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef OPTIONAL_H_2857428578342203589
#define OPTIONAL_H_2857428578342203589

#include <cassert>
#include <type_traits>

namespace zen
{
/*
Optional return value without heap memory allocation!
 -> interface like a pointer, performance like a value

 Usage:
 ------
 Opt<MyEnum> someFunction();
{
   if (allIsWell)
       return enumVal;
   else
       return NoValue();
}

 Opt<MyEnum> optValue = someFunction();
 if (optValue)
       ... use *optValue ...
*/

struct NoValue {};

template <class T>
class Opt
{
public:
    Opt()        {}
    Opt(NoValue) {}
    Opt(const T& val) : valid(true) { new (&rawMem) T(val); } //throw X

    Opt(const Opt& other) : valid(other.valid)
    {
        if (const T* val = other.get())
            new (&rawMem) T(*val); //throw X
    }

    ~Opt() { if (T* val = get()) val->~T(); }

    Opt& operator=(const Opt& other) //strong exception-safety iff T::operator=() is strongly exception-safe
    {
        if (T* val = get())
        {
            if (const T* valOther = other.get())
                *val = *valOther; //throw X
            else
            {
                valid = false;
                val->~T();
            }
        }
        else if (const T* valOther = other.get())
        {
            new (&rawMem) T(*valOther); //throw X
            valid = true;
        }
        return *this;
    }

    explicit operator bool() const { return valid; } //thank you C++11!!!

    const T* get() const { return valid ? reinterpret_cast<const T*>(&rawMem) : nullptr; }
    T*       get()       { return valid ? reinterpret_cast<      T*>(&rawMem) : nullptr; }

    const T& operator*() const { return *get(); }
    /**/  T& operator*()       { return *get(); }

    const T* operator->() const { return get(); }
    /**/  T* operator->()       { return get(); }

private:
    std::aligned_storage_t<sizeof(T), alignof(T)> rawMem; //don't require T to be default-constructible!
    bool valid = false;
};

}

#endif //OPTIONAL_H_2857428578342203589
