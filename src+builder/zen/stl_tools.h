// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef STL_TOOLS_HEADER_84567184321434
#define STL_TOOLS_HEADER_84567184321434

#include <set>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include "type_tools.h"
#include "build_info.h"

//enhancements for <algorithm>
namespace zen
{
//erase selected elements from any container:
template <class T, class Alloc, class Predicate>
void erase_if(std::vector<T, Alloc>& v, Predicate p);

template <class T, class LessType, class Alloc, class Predicate>
void erase_if(std::set<T, LessType, Alloc>& s, Predicate p);

template <class KeyType, class ValueType, class LessType, class Alloc, class Predicate>
void erase_if(std::map<KeyType, ValueType, LessType, Alloc>& m, Predicate p);

//append STL containers
template <class T, class Alloc, class C>
void append(std::vector<T, Alloc>& v, const C& c);

template <class T, class LessType, class Alloc, class C>
void append(std::set<T, LessType, Alloc>& s, const C& c);

template <class KeyType, class ValueType, class LessType, class Alloc, class C>
void append(std::map<KeyType, ValueType, LessType, Alloc>& m, const C& c);

template <class M, class K, class V>
V& map_add_or_update(M& map, const K& key, const V& value); //efficient add or update without "default-constructible" requirement (Effective STL, item 24)

template <class T, class Alloc>
void removeDuplicates(std::vector<T, Alloc>& v);

//binary search returning an iterator
template <class ForwardIterator, class T, typename CompLess>
ForwardIterator binary_search(ForwardIterator first, ForwardIterator last, const T& value, CompLess less);

template <class BidirectionalIterator, class T>
BidirectionalIterator find_last(BidirectionalIterator first, BidirectionalIterator last, const T& value);

//replacement for std::find_end taking advantage of bidirectional iterators (and giving the algorithm a reasonable name)
template <class BidirectionalIterator1, class BidirectionalIterator2>
BidirectionalIterator1 search_last(BidirectionalIterator1 first1, BidirectionalIterator1 last1,
                                   BidirectionalIterator2 first2, BidirectionalIterator2 last2);

template <class InputIterator1, class InputIterator2>
bool equal(InputIterator1 first1, InputIterator1 last1,
           InputIterator2 first2, InputIterator2 last2);

size_t hashBytes(const unsigned char* ptr, size_t len);


//support for custom string classes in std::unordered_set/map
struct StringHash
{
    template <class String>
    size_t operator()(const String& str) const
    {
        const auto* strFirst = strBegin(str);
        return hashBytes(reinterpret_cast<const unsigned char*>(strFirst), strLength(str) * sizeof(strFirst[0]));
    }
};






//######################## implementation ########################

template <class T, class Alloc, class Predicate> inline
void erase_if(std::vector<T, Alloc>& v, Predicate p)
{
    v.erase(std::remove_if(v.begin(), v.end(), p), v.end());
}


namespace impl
{
template <class S, class Predicate> inline
void set_or_map_erase_if(S& s, Predicate p)
{
    for (auto iter = s.begin(); iter != s.end();)
        if (p(*iter))
            s.erase(iter++);
        else
            ++iter;
}
}


template <class T, class LessType, class Alloc, class Predicate> inline
void erase_if(std::set<T, LessType, Alloc>& s, Predicate p) { impl::set_or_map_erase_if(s, p); } //don't make this any more generic! e.g. must not compile for std::vector!!!


template <class KeyType, class ValueType, class LessType, class Alloc, class Predicate> inline
void erase_if(std::map<KeyType, ValueType, LessType, Alloc>& m, Predicate p) { impl::set_or_map_erase_if(m, p); }


template <class T, class Alloc, class C> inline
void append(std::vector<T, Alloc>& v, const C& c) { v.insert(v.end(), c.begin(), c.end()); }


template <class T, class LessType, class Alloc, class C> inline
void append(std::set<T, LessType, Alloc>& s, const C& c) { s.insert(c.begin(), c.end()); }


template <class KeyType, class ValueType, class LessType, class Alloc, class C> inline
void append(std::map<KeyType, ValueType, LessType, Alloc>& m, const C& c) { m.insert(c.begin(), c.end()); }


template <class M, class K, class V> inline
V& map_add_or_update(M& map, const K& key, const V& value) //efficient add or update without "default-constructible" requirement (Effective STL, item 24)
{
    auto iter = map.lower_bound(key);
    if (iter != map.end() && !(map.key_comp()(key, iter->first)))
    {
        iter->second = value;
        return iter->second;
    }
    else
        return map.insert(iter, typename M::value_type(key, value))->second;
}


template <class T, class Alloc> inline
void removeDuplicates(std::vector<T, Alloc>& v)
{
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}


template <class ForwardIterator, class T, typename CompLess> inline
ForwardIterator binary_search(ForwardIterator first, ForwardIterator last, const T& value, CompLess less)
{
    first = std::lower_bound(first, last, value, less);
    if (first != last && !less(value, *first))
        return first;
    else
        return last;
}


template <class BidirectionalIterator, class T> inline
BidirectionalIterator find_last(const BidirectionalIterator first, const BidirectionalIterator last, const T& value)
{
    for (BidirectionalIterator iter = last; iter != first;) //reverse iteration: 1. check 2. decrement 3. evaluate
    {
        --iter; //

        if (*iter == value)
            return iter;
    }
    return last;
}


template <class BidirectionalIterator1, class BidirectionalIterator2> inline
BidirectionalIterator1 search_last(const BidirectionalIterator1 first1,       BidirectionalIterator1 last1,
                                   const BidirectionalIterator2 first2, const BidirectionalIterator2 last2)
{
    const BidirectionalIterator1 iterNotFound = last1;

    //reverse iteration: 1. check 2. decrement 3. evaluate
    for (;;)
    {
        BidirectionalIterator1 it1 = last1;
        BidirectionalIterator2 it2 = last2;

        for (;;)
        {
            if (it2 == first2) return it1;
            if (it1 == first1) return iterNotFound;

            --it1;
            --it2;

            if (*it1 != *it2) break;
        }
        --last1;
    }
}


template <class InputIterator1, class InputIterator2> inline
bool equal(InputIterator1 first1, InputIterator1 last1,
           InputIterator2 first2, InputIterator2 last2)
{
    return last1 - first1 == last2 - first2 && std::equal(first1, last1, first2);
}


#if defined _MSC_VER && _MSC_VER <= 1600
    static_assert(false, "VS2010 performance bug in std::unordered_set<>: http://drdobbs.com/blogs/cpp/232200410 -> should be fixed in VS11");
#endif


inline
size_t hashBytes(const unsigned char* ptr, size_t len)
{
    //http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
#ifdef ZEN_BUILD_32BIT
    const size_t basis = 2166136261U;
    const size_t prime = 16777619U;
#elif defined ZEN_BUILD_64BIT
    const size_t basis = 14695981039346656037ULL;
    const size_t prime = 1099511628211ULL;
#endif

    size_t val = basis;
    for (size_t i = 0; i < len; ++i)
    {
        val ^= static_cast<size_t>(ptr[i]);
        val *= prime;
    }
    return val;
}
}

#endif //STL_TOOLS_HEADER_84567184321434
