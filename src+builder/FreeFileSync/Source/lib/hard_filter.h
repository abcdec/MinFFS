// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef HARD_FILTER_H_825780275842758345
#define HARD_FILTER_H_825780275842758345

#include <vector>
#include <memory>
#include <zen/zstring.h>

namespace zen
{
//------------------------------------------------------------------
/*
Semantics of HardFilter:
1. using it creates a NEW folder hierarchy! -> must be considered by <Automatic>-mode!
2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!

    class hierarchy:

          HardFilter (interface)
               /|\
       _________|_____________
      |         |             |
NullFilter  NameFilter  CombinedFilter
*/

class HardFilter //interface for filtering
{
public:
    virtual ~HardFilter() {}

    //filtering
    virtual bool passFileFilter(const Zstring& relFilename) const = 0;
    virtual bool passDirFilter (const Zstring& relDirname, bool* subObjMightMatch) const = 0;
    //subObjMightMatch: file/dir in subdirectories could(!) match
    //note: variable is only set if passDirFilter returns false!

    virtual bool isNull() const = 0; //filter is equivalent to NullFilter, but may be technically slower

    typedef std::shared_ptr<const HardFilter> FilterRef; //always bound by design!

    //serialization
    //    void saveFilter(ZstreamOut& stream) const; //serialize derived object
    //    static FilterRef loadFilter(ZstreamIn& stream); //throw UnexpectedEndOfStreamError; CAVEAT!!! adapt this method for each new derivation!!!

private:
    friend bool operator<(const HardFilter& lhs, const HardFilter& rhs);

    //    virtual void save(ZstreamOut& stream) const = 0; //serialization
    virtual std::string uniqueClassIdentifier() const = 0;   //get identifier, used for serialization
    virtual bool cmpLessSameType(const HardFilter& other) const = 0; //typeid(*this) == typeid(other) in this context!
};

inline bool operator==(const HardFilter& lhs, const HardFilter& rhs) { return !(lhs < rhs) && !(rhs < lhs); }
inline bool operator!=(const HardFilter& lhs, const HardFilter& rhs) { return !(lhs == rhs); }


//small helper method: merge two hard filters (thereby remove Null-filters)
HardFilter::FilterRef combineFilters(const HardFilter::FilterRef& first,
                                     const HardFilter::FilterRef& second);


class NullFilter : public HardFilter  //no filtering at all
{
public:
    bool passFileFilter(const Zstring& relFilename) const override;
    bool passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const override;
    bool isNull() const override;

private:
    friend class HardFilter;
    //    void save(ZstreamOut& stream) const override {}
    std::string uniqueClassIdentifier() const override { return "NullFilter"; }
    //    static FilterRef load(ZstreamIn& stream); //throw UnexpectedEndOfStreamError
    bool cmpLessSameType(const HardFilter& other) const override;
};


class NameFilter : public HardFilter  //standard filter by filepath
{
public:
    NameFilter(const Zstring& includeFilter, const Zstring& excludeFilter);

    bool passFileFilter(const Zstring& relFilename) const override;
    bool passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const override;

    bool isNull() const override;
    static bool isNull(const Zstring& includeFilter, const Zstring& excludeFilter); //*fast* check without expensively constructing NameFilter instance!

private:
    friend class HardFilter;
    //    void save(ZstreamOut& stream) const override;
    std::string uniqueClassIdentifier() const override { return "NameFilter"; }
    //    static FilterRef load(ZstreamIn& stream); //throw UnexpectedEndOfStreamError
    bool cmpLessSameType(const HardFilter& other) const override;

    std::vector<Zstring> filterFileIn;   //
    std::vector<Zstring> filterFolderIn; //upper case (windows) + unique items by construction
    std::vector<Zstring> filterFileEx;   //
    std::vector<Zstring> filterFolderEx; //

    const Zstring includeFilterTmp; //save constructor arguments for serialization
    const Zstring excludeFilterTmp; //
};


class CombinedFilter : public HardFilter  //combine two filters to match if and only if both match
{
public:
    CombinedFilter(const FilterRef& first, const FilterRef& second) : first_(first), second_(second) {}

    bool passFileFilter(const Zstring& relFilename) const override;
    bool passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const override;
    bool isNull() const override;

private:
    friend class HardFilter;
    //    void save(ZstreamOut& stream) const override;
    std::string uniqueClassIdentifier() const override { return "CombinedFilter"; }
    //    static FilterRef load(ZstreamIn& stream); //throw UnexpectedEndOfStreamError
    bool cmpLessSameType(const HardFilter& other) const override;

    const FilterRef first_;
    const FilterRef second_;
};


















//---------------Inline Implementation---------------------------------------------------
//inline
//HardFilter::FilterRef NullFilter::load(ZstreamIn& stream)
//{
//    return FilterRef(new NullFilter);
//}


inline
bool NullFilter::passFileFilter(const Zstring& relFilename) const
{
    return true;
}


inline
bool NullFilter::passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const
{
    assert(!subObjMightMatch || *subObjMightMatch == true); //check correct usage
    return true;
}


inline
bool NullFilter::isNull() const
{
    return true;
}


inline
bool NullFilter::cmpLessSameType(const HardFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!
    return false;
}


inline
bool CombinedFilter::passFileFilter(const Zstring& relFilename) const
{
    return first_ ->passFileFilter(relFilename) && //short-circuit behavior
           second_->passFileFilter(relFilename);
}


inline
bool CombinedFilter::passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const
{
    if (first_->passDirFilter(relDirname, subObjMightMatch))
        return second_->passDirFilter(relDirname, subObjMightMatch);
    else
    {
        if (subObjMightMatch && *subObjMightMatch)
            second_->passDirFilter(relDirname, subObjMightMatch);
        return false;
    }
}


inline
bool CombinedFilter::isNull() const
{
    return first_->isNull() && second_->isNull();
}


inline
bool CombinedFilter::cmpLessSameType(const HardFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const CombinedFilter& otherCombFilt = static_cast<const CombinedFilter&>(other);

    if (*first_ != *otherCombFilt.first_)
        return *first_ < *otherCombFilt.first_;

    return *second_ < *otherCombFilt.second_;
}


//inline
//void CombinedFilter::save(ZstreamOut& stream) const
//{
//    first_ ->saveFilter(stream);
//    second_->saveFilter(stream);
//}


//inline
//HardFilter::FilterRef CombinedFilter::load(ZstreamIn& stream) //throw UnexpectedEndOfStreamError
//{
//    FilterRef first  = loadFilter(stream); //throw UnexpectedEndOfStreamError
//    FilterRef second = loadFilter(stream); //
//
//    return combineFilters(first, second);
//}


inline
HardFilter::FilterRef combineFilters(const HardFilter::FilterRef& first,
                                     const HardFilter::FilterRef& second)
{
    if (first->isNull())
    {
        if (second->isNull())
            return std::make_shared<NullFilter>();
        else
            return second;
    }
    else
    {
        if (second->isNull())
            return first;
        else
            return std::make_shared<CombinedFilter>(first, second);
    }
}
}


#endif //HARD_FILTER_H_825780275842758345
