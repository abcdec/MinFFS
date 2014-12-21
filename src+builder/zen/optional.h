// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef OPTIONAL_H_2857428578342203589
#define OPTIONAL_H_2857428578342203589

#include <cassert>

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
    Opt()             : value()   , valid(false) {}
    Opt(NoValue)      : value()   , valid(false) {}
    Opt(const T& val) : value(val), valid(true ) {}

    Opt(const Opt& tmp) : value(tmp.valid ? tmp.value : T()), valid(tmp.valid) {}

    Opt& operator=(const Opt& tmp)
    {
        if (tmp.valid)
            value = tmp.value;
        valid = tmp.valid;
        return *this;
    }

    ////rvalue optimization: only basic exception safety:
    //   Opt(Opt&& tmp) : value(std::move(tmp.value)), valid(tmp.valid) {}

    explicit operator bool() const { return valid; } //thank you C++11!!!

    const T& operator*() const { assert(valid); return value; }
    /**/  T& operator*()       { assert(valid); return value; }

    const T* operator->() const { assert(valid); return &value; }
    /**/  T* operator->()       { assert(valid); return &value; }

    void reset() { valid = false; }

private:
    T value;
    bool valid;
};

}

#endif //OPTIONAL_H_2857428578342203589
