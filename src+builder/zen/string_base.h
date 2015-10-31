// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef Z_BASE_H_INCLUDED_08321745456
#define Z_BASE_H_INCLUDED_08321745456

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <atomic>
#include "string_tools.h"

//Zbase - a policy based string class optimizing performance and flexibility

namespace zen
{
/*
Allocator Policy:
-----------------
    void* allocate(size_t size) //throw std::bad_alloc
    void deallocate(void* ptr)
    size_t calcCapacity(size_t length)
*/
class AllocatorOptimalSpeed //exponential growth + min size
{
public:
    //::operator new/ ::operator delete show same performance characterisics like malloc()/free()!
    static void* allocate(size_t size) { return ::operator new(size); } //throw std::bad_alloc
    static void  deallocate(void* ptr) { ::operator delete(ptr); }
    static size_t calcCapacity(size_t length) { return std::max<size_t>(16, std::max(length + length / 2, length)); }
    //- size_t might overflow! => better catch here than return a too small size covering up the real error: a way too large length!
    //- any growth rate should not exceed golden ratio: 1.618033989
};


class AllocatorOptimalMemory //no wasted memory, but more reallocations required when manipulating string
{
public:
    static void* allocate(size_t size) { return ::operator new(size); } //throw std::bad_alloc
    static void  deallocate(void* ptr) { ::operator delete(ptr); }
    static size_t calcCapacity(size_t length) { return length; }
};

/*
Storage Policy:
---------------
template <typename Char, //Character Type
         class AP>       //Allocator Policy

    Char* create(size_t size)
    Char* create(size_t size, size_t minCapacity)
    Char* clone(Char* ptr)
    void destroy(Char* ptr) //must handle "destroy(nullptr)"!
    bool canWrite(const Char* ptr, size_t minCapacity) //needs to be checked before writing to "ptr"
    size_t length(const Char* ptr)
    void setLength(Char* ptr, size_t newLength)
*/

template <class Char, //Character Type
          class AP>   //Allocator Policy
class StorageDeepCopy : public AP
{
protected:
    ~StorageDeepCopy() {}

    Char* create(size_t size) { return create(size, size); }
    Char* create(size_t size, size_t minCapacity)
    {
        assert(size <= minCapacity);
        const size_t newCapacity = AP::calcCapacity(minCapacity);
        assert(newCapacity >= minCapacity);

        Descriptor* const newDescr = static_cast<Descriptor*>(this->allocate(sizeof(Descriptor) + (newCapacity + 1) * sizeof(Char))); //throw std::bad_alloc
        new (newDescr) Descriptor(size, newCapacity);

        return reinterpret_cast<Char*>(newDescr + 1); //alignment note: "newDescr + 1" is Descriptor-aligned, which is larger than alignment for Char-array! => no problem!
    }

    Char* clone(Char* ptr)
    {
        Char* newData = create(length(ptr)); //throw std::bad_alloc
        std::copy(ptr, ptr + length(ptr) + 1, newData);
        return newData;
    }

    void destroy(Char* ptr)
    {
        if (!ptr) return; //support "destroy(nullptr)"

        Descriptor* const d = descr(ptr);
        d->~Descriptor();
        this->deallocate(d);
    }

    //this needs to be checked before writing to "ptr"
    static bool canWrite(const Char* ptr, size_t minCapacity) { return minCapacity <= descr(ptr)->capacity; }
    static size_t length(const Char* ptr) { return descr(ptr)->length; }

    static void setLength(Char* ptr, size_t newLength)
    {
        assert(canWrite(ptr, newLength));
        descr(ptr)->length = newLength;
    }

private:
    struct Descriptor
    {
        Descriptor(size_t len, size_t cap) :
            length  (static_cast<std::uint32_t>(len)),
            capacity(static_cast<std::uint32_t>(cap)) {}

        std::uint32_t length;
        std::uint32_t capacity; //allocated size without null-termination
    };

    static       Descriptor* descr(      Char* ptr) { return reinterpret_cast<      Descriptor*>(ptr) - 1; }
    static const Descriptor* descr(const Char* ptr) { return reinterpret_cast<const Descriptor*>(ptr) - 1; }
};


template <class Char, //Character Type
          class AP>   //Allocator Policy
class StorageRefCountThreadSafe : public AP
{
protected:
    ~StorageRefCountThreadSafe() {}

    Char* create(size_t size) { return create(size, size); }
    Char* create(size_t size, size_t minCapacity)
    {
        assert(size <= minCapacity);

        const size_t newCapacity = AP::calcCapacity(minCapacity);
        assert(newCapacity >= minCapacity);

        Descriptor* const newDescr = static_cast<Descriptor*>(this->allocate(sizeof(Descriptor) + (newCapacity + 1) * sizeof(Char))); //throw std::bad_alloc
        new (newDescr) Descriptor(size, newCapacity);

        return reinterpret_cast<Char*>(newDescr + 1);
    }

    static Char* clone(Char* ptr)
    {
        ++descr(ptr)->refCount;
        return ptr;
    }

    void destroy(Char* ptr)
    {
        if (!ptr) return; //support "destroy(nullptr)"

        Descriptor* const d = descr(ptr);

        if (--(d->refCount) == 0) //operator--() is overloaded to decrement and evaluate in a single atomic operation!
        {
            d->~Descriptor();
            this->deallocate(d);
        }
    }

    static bool canWrite(const Char* ptr, size_t minCapacity) //needs to be checked before writing to "ptr"
    {
        const Descriptor* const d = descr(ptr);
        assert(d->refCount > 0);
        return d->refCount == 1 && minCapacity <= d->capacity;
    }

    static size_t length(const Char* ptr) { return descr(ptr)->length; }

    static void setLength(Char* ptr, size_t newLength)
    {
        assert(canWrite(ptr, newLength));
        descr(ptr)->length = static_cast<std::uint32_t>(newLength);
    }

private:
    struct Descriptor
    {
        Descriptor(size_t len, size_t cap) :
            refCount(1),
            length  (static_cast<std::uint32_t>(len)),
            capacity(static_cast<std::uint32_t>(cap)) { static_assert(ATOMIC_INT_LOCK_FREE == 2, ""); } //2: "the types are always lock-free"

        std::atomic<unsigned int> refCount;
        std::uint32_t length;
        std::uint32_t capacity; //allocated size without null-termination
    };

    static       Descriptor* descr(      Char* ptr) { return reinterpret_cast<      Descriptor*>(ptr) - 1; }
    static const Descriptor* descr(const Char* ptr) { return reinterpret_cast<const Descriptor*>(ptr) - 1; }
};

//################################################################################################################################################################

//perf note: interestingly StorageDeepCopy and StorageRefCountThreadSafe show same performance in FFS comparison

template <class Char,  							                        //Character Type
          template <class, class> class SP = StorageRefCountThreadSafe, //Storage Policy
          class AP = AllocatorOptimalSpeed>				                //Allocator Policy
class Zbase : public SP<Char, AP>
{
public:
    Zbase();
    Zbase(const Char* source); //implicit conversion from a C-string
    Zbase(const Char* source, size_t length);
    Zbase(const Zbase& source);
    Zbase(Zbase&& tmp); //make noexcept in C++11
    explicit Zbase(Char source); //dangerous if implicit: Char buffer[]; return buffer[0]; ups... forgot &, but not a compiler error!
    //allow explicit construction from different string type, prevent ambiguity via SFINAE
    template <class S> explicit Zbase(const S& other, typename S::value_type = 0);
    ~Zbase(); //make noexcept in C++11

    //operator const Char* () const; //NO implicit conversion to a C-string!! Many problems... one of them: if we forget to provide operator overloads, it'll just work with a Char*...

    //STL accessors
    typedef       Char*       iterator;
    typedef const Char* const_iterator;
    typedef       Char&       reference;
    typedef const Char& const_reference;
    typedef       Char value_type;

    Zbase(const_iterator first, const_iterator last);
    Char*       begin();
    Char*       end  ();
    const Char* begin() const;
    const Char* end  () const;
    const Char* cbegin() const { return begin(); }
    const Char* cend  () const { return end(); }

    //std::string functions
    size_t length() const;
    size_t size  () const { return length(); }
    const Char* c_str() const { return rawStr; }; //C-string format with 0-termination
    const Char* data()  const { return rawStr; }; //internal representation, 0-termination not guaranteed
    const Char operator[](size_t pos) const;
    bool empty() const { return length() == 0; }
    void clear();
    size_t find (const Zbase& str, size_t pos = 0)    const; //
    size_t find (const Char* str,  size_t pos = 0)    const; //
    size_t find (Char  ch,         size_t pos = 0)    const; //returns "npos" if not found
    size_t rfind(Char  ch,         size_t pos = npos) const; //
    size_t rfind(const Char* str,  size_t pos = npos) const; //
    //Zbase& replace(size_t pos1, size_t n1, const Zbase& str);
    void reserve(size_t minCapacity);
    Zbase& assign(const Char* source, size_t len);
    Zbase& append(const Char* source, size_t len);
    void resize(size_t newSize, Char fillChar = 0);
    void swap(Zbase& other); //make noexcept in C++11
    void push_back(Char val) { operator+=(val); } //STL access

    Zbase& operator=(const Zbase& source);
    Zbase& operator=(Zbase&& tmp); //make noexcept in C++11
    Zbase& operator=(const Char* source);
    Zbase& operator=(Char source);
    Zbase& operator+=(const Zbase& other);
    Zbase& operator+=(const Char* other);
    Zbase& operator+=(Char ch);

    static const size_t	npos = static_cast<size_t>(-1);

private:
    Zbase            (int) = delete; //
    Zbase& operator= (int) = delete; //detect usage errors by creating an intentional ambiguity with "Char"
    Zbase& operator+=(int) = delete; //
    void   push_back (int) = delete; //

    Char* rawStr;
};

template <class Char, template <class, class> class SP, class AP>        bool operator==(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs);
template <class Char, template <class, class> class SP, class AP>        bool operator==(const Zbase<Char, SP, AP>& lhs, const Char*                rhs);
template <class Char, template <class, class> class SP, class AP> inline bool operator==(const Char*                lhs, const Zbase<Char, SP, AP>& rhs) { return operator==(rhs, lhs); }

template <class Char, template <class, class> class SP, class AP> inline bool operator!=(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs) { return !operator==(lhs, rhs); }
template <class Char, template <class, class> class SP, class AP> inline bool operator!=(const Zbase<Char, SP, AP>& lhs, const Char*                rhs) { return !operator==(lhs, rhs); }
template <class Char, template <class, class> class SP, class AP> inline bool operator!=(const Char*                lhs, const Zbase<Char, SP, AP>& rhs) { return !operator==(lhs, rhs); }

template <class Char, template <class, class> class SP, class AP> bool operator<(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs);
template <class Char, template <class, class> class SP, class AP> bool operator<(const Zbase<Char, SP, AP>& lhs, const Char*                rhs);
template <class Char, template <class, class> class SP, class AP> bool operator<(const Char*                lhs, const Zbase<Char, SP, AP>& rhs);

template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs) { return Zbase<Char, SP, AP>(lhs) += rhs; }
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(const Zbase<Char, SP, AP>& lhs, const Char*                rhs) { return Zbase<Char, SP, AP>(lhs) += rhs; }
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(const Zbase<Char, SP, AP>& lhs,       Char                 rhs) { return Zbase<Char, SP, AP>(lhs) += rhs; }

//don't use unified first argument but save one move-construction in the r-value case instead!
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(Zbase<Char, SP, AP>&& lhs, const Zbase<Char, SP, AP>& rhs) { return std::move(lhs += rhs); } //is the move really needed?
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(Zbase<Char, SP, AP>&& lhs, const Char*                rhs) { return std::move(lhs += rhs); } //lhs, is an l-vlaue in the function body...
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(Zbase<Char, SP, AP>&& lhs,       Char                 rhs) { return std::move(lhs += rhs); } //and not a local variable => no copy elision

template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(      Char          lhs, const Zbase<Char, SP, AP>& rhs) { return Zbase<Char, SP, AP>(lhs) += rhs; }
template <class Char, template <class, class> class SP, class AP> inline Zbase<Char, SP, AP> operator+(const Char*         lhs, const Zbase<Char, SP, AP>& rhs) { return Zbase<Char, SP, AP>(lhs) += rhs; }













//################################# implementation ########################################
template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase()
{
    //resist the temptation to avoid this allocation by referening a static global: NO performance advantage, MT issues!
    rawStr    = this->create(0);
    rawStr[0] = 0;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase(Char source)
{
    rawStr    = this->create(1);
    rawStr[0] = source;
    rawStr[1] = 0;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase(const Char* source)
{
    const size_t sourceLen = strLength(source);
    rawStr = this->create(sourceLen);
    std::copy(source, source + sourceLen + 1, rawStr); //include null-termination
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase(const Char* source, size_t sourceLen)
{
    rawStr = this->create(sourceLen);
    std::copy(source, source + sourceLen, rawStr);
    rawStr[sourceLen] = 0;
}


template <class Char, template <class, class> class SP, class AP>
Zbase<Char, SP, AP>::Zbase(const_iterator first, const_iterator last)
{
    assert(first <= last);
    const size_t sourceLen = last - first;
    rawStr = this->create(sourceLen);
    std::copy(first, last, rawStr);
    rawStr[sourceLen] = 0;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase(const Zbase<Char, SP, AP>& source)
{
    rawStr = this->clone(source.rawStr);
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::Zbase(Zbase<Char, SP, AP>&& tmp)
{
    rawStr = tmp.rawStr;
    tmp.rawStr = nullptr; //usually nullptr would violate the class invarants, but it is good enough for the destructor!
    //caveat: do not increment ref-count of an unshared string! We'd lose optimization opportunity of reusing its memory!
}


template <class Char, template <class, class> class SP, class AP>
template <class S> inline
Zbase<Char, SP, AP>::Zbase(const S& other, typename S::value_type)
{
    const size_t sourceLen = other.size();
    rawStr = this->create(sourceLen);
    std::copy(other.c_str(), other.c_str() + sourceLen, rawStr);
    rawStr[sourceLen] = 0;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>::~Zbase()
{
    this->destroy(rawStr); //rawStr may be nullptr; see move constructor!
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::find(const Zbase& str, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::search(begin() + std::min(pos, len), thisEnd,
                                 str.begin(), str.end());
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::find(const Char* str, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::search(begin() + std::min(pos, len), thisEnd,
                                 str, str + strLength(str));
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::find(Char ch, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::find(begin() + std::min(pos, len), thisEnd, ch);
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::rfind(Char ch, size_t pos) const
{
    assert(pos == npos || pos <= length());
    const size_t len = length();
    const Char* currEnd = begin() + (pos == npos ? len : std::min(pos + 1, len));
    const Char* it = find_last(begin(), currEnd, ch);
    return it == currEnd ? npos : it - begin();
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::rfind(const Char* str, size_t pos) const
{
    assert(pos == npos || pos <= length());
    const size_t strLen = strLength(str);
    const size_t len = length();
    const Char* currEnd = begin() + (pos == npos ? len : std::min(pos + strLen, len));
    const Char* it = search_last(begin(), currEnd,
                                 str, str + strLen);
    return it == currEnd ? npos : it - begin();
}


template <class Char, template <class, class> class SP, class AP> inline
void Zbase<Char, SP, AP>::resize(size_t newSize, Char fillChar)
{
    const size_t oldSize = length();
    if (this->canWrite(rawStr, newSize))
    {
        if (oldSize < newSize)
            std::fill(rawStr + oldSize, rawStr + newSize, fillChar);
        rawStr[newSize] = 0;
        this->setLength(rawStr, newSize);
    }
    else
    {
        Char* newStr = this->create(newSize);
        if (oldSize < newSize)
        {
            std::copy(rawStr, rawStr + oldSize, newStr);
            std::fill(newStr + oldSize, newStr + newSize, fillChar);
        }
        else
            std::copy(rawStr, rawStr + newSize, newStr);
        newStr[newSize] = 0;

        this->destroy(rawStr);
        rawStr = newStr;
    }
}


template <class Char, template <class, class> class SP, class AP> inline
bool operator==(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs)
{
    return lhs.length() == rhs.length() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); //respect embedded 0
}


template <class Char, template <class, class> class SP, class AP> inline
bool operator==(const Zbase<Char, SP, AP>& lhs, const Char* rhs)
{
    return lhs.length() == strLength(rhs) && std::equal(lhs.begin(), lhs.end(), rhs); //respect embedded 0
}


template <class Char, template <class, class> class SP, class AP> inline
bool operator<(const Zbase<Char, SP, AP>& lhs, const Zbase<Char, SP, AP>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), //respect embedded 0
                                        rhs.begin(), rhs.end());
}


template <class Char, template <class, class> class SP, class AP> inline
bool operator<(const Zbase<Char, SP, AP>& lhs, const Char* rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), //respect embedded 0
                                        rhs, rhs + strLength(rhs));
}


template <class Char, template <class, class> class SP, class AP> inline
bool operator<(const Char* lhs, const Zbase<Char, SP, AP>& rhs)
{
    return std::lexicographical_compare(lhs, lhs + strLength(lhs), //respect embedded 0
                                        rhs.begin(), rhs.end());
}


template <class Char, template <class, class> class SP, class AP> inline
size_t Zbase<Char, SP, AP>::length() const
{
    return SP<Char, AP>::length(rawStr);
}


template <class Char, template <class, class> class SP, class AP> inline
const Char Zbase<Char, SP, AP>::operator[](size_t pos) const
{
    assert(pos < length()); //design by contract! no runtime check!
    return rawStr[pos];
}


template <class Char, template <class, class> class SP, class AP> inline
const Char* Zbase<Char, SP, AP>::begin() const
{
    return rawStr;
}


template <class Char, template <class, class> class SP, class AP> inline
const Char* Zbase<Char, SP, AP>::end() const
{
    return rawStr + length();
}


template <class Char, template <class, class> class SP, class AP> inline
Char* Zbase<Char, SP, AP>::begin()
{
    reserve(length()); //make unshared!
    return rawStr;
}


template <class Char, template <class, class> class SP, class AP> inline
Char* Zbase<Char, SP, AP>::end()
{
    return begin() + length();
}


template <class Char, template <class, class> class SP, class AP> inline
void Zbase<Char, SP, AP>::clear()
{
    if (!empty())
    {
        if (this->canWrite(rawStr, 0))
        {
            rawStr[0] = 0;              //keep allocated memory
            this->setLength(rawStr, 0); //
        }
        else
            *this = Zbase();
    }
}


template <class Char, template <class, class> class SP, class AP> inline
void Zbase<Char, SP, AP>::swap(Zbase<Char, SP, AP>& other)
{
    std::swap(rawStr, other.rawStr);
}


template <class Char, template <class, class> class SP, class AP> inline
void Zbase<Char, SP, AP>::reserve(size_t minCapacity) //make unshared and check capacity
{
    if (!this->canWrite(rawStr, minCapacity))
    {
        //allocate a new string
        const size_t len = length();
        Char* newStr = this->create(len, std::max(len, minCapacity)); //reserve() must NEVER shrink the string: logical const!
        std::copy(rawStr, rawStr + len + 1, newStr); //include 0-termination

        this->destroy(rawStr);
        rawStr = newStr;
    }
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::assign(const Char* source, size_t len)
{
    if (this->canWrite(rawStr, len))
    {
        std::copy(source, source + len, rawStr);
        rawStr[len] = 0; //include null-termination
        this->setLength(rawStr, len);
    }
    else
        *this = Zbase(source, len);

    return *this;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::append(const Char* source, size_t len)
{
    const size_t thisLen = length();
    reserve(thisLen + len); //make unshared and check capacity

    std::copy(source, source + len, rawStr + thisLen);
    rawStr[thisLen + len] = 0;
    this->setLength(rawStr, thisLen + len);
    return *this;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator=(const Zbase<Char, SP, AP>& other)
{
    Zbase<Char, SP, AP>(other).swap(*this);
    return *this;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator=(Zbase<Char, SP, AP>&& tmp)
{
    swap(tmp); //don't use unifying assignment but save one move-construction in the r-value case instead!
    return *this;
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator=(const Char* source)
{
    return assign(source, strLength(source));
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator=(Char ch)
{
    return assign(&ch, 1);
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator+=(const Zbase<Char, SP, AP>& other)
{
    return append(other.c_str(), other.length());
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator+=(const Char* other)
{
    return append(other, strLength(other));
}


template <class Char, template <class, class> class SP, class AP> inline
Zbase<Char, SP, AP>& Zbase<Char, SP, AP>::operator+=(Char ch)
{
    return append(&ch, 1);
}
}

#endif //Z_BASE_H_INCLUDED_08321745456
