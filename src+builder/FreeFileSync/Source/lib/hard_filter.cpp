// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "hard_filter.h"
#include <set>
#include <stdexcept>
#include <vector>
#include <typeinfo>
#include <iterator>

using namespace zen;

//inline bool operator<(const std::type_info& lhs, const std::type_info& rhs) { return lhs.before(rhs) != 0; } -> not working on GCC :(


//--------------------------------------------------------------------------------------------------
bool zen::operator<(const HardFilter& lhs, const HardFilter& rhs)
{
    if (typeid(lhs) != typeid(rhs))
        return typeid(lhs).before(typeid(rhs)); //in worst case, order is guaranteed to be stable only during each program run

    //this and other are of same type:
    return lhs.cmpLessSameType(rhs);
}


//void HardFilter::saveFilter(ZstreamOut& stream) const //serialize derived object
//{
//    //save type information
//    writeString(stream, uniqueClassIdentifier());
//
//    //save actual object
//    save(stream);
//}


//HardFilter::FilterRef HardFilter::loadFilter(ZstreamIn& stream) //throw UnexpectedEndOfStreamError
//{
//    //read type information
//    const std::string uniqueClassId = readString<std::string>(stream); //throw UnexpectedEndOfStreamError
//
//    //read actual object
//    if (uniqueClassId == "NullFilter")
//        return NullFilter::load(stream);
//    else if (uniqueClassId == "NameFilter")
//        return NameFilter::load(stream);
//    else if (uniqueClassId == "CombinedFilter")
//        return CombinedFilter::load(stream);
//    else
//        throw std::logic_error("Programming Error: Unknown filter!");
//}


namespace
{
//constructing them in addFilterEntry becomes perf issue for large filter lists
const Zstring asterisk(Zstr("*"));
const Zstring sepAsterisk         = FILE_NAME_SEPARATOR + asterisk;
const Zstring asteriskSep         = asterisk + FILE_NAME_SEPARATOR;
const Zstring asteriskSepAsterisk = asteriskSep + asterisk;
}


void addFilterEntry(const Zstring& filterPhrase, std::vector<Zstring>& fileFilter, std::vector<Zstring>& directoryFilter)
{
#if defined ZEN_WIN || defined ZEN_MAC
    //Windows does NOT distinguish between upper/lower-case
    const Zstring& filterFmt= makeUpperCopy(filterPhrase);
#elif defined ZEN_LINUX
    const Zstring& filterFmt = filterPhrase;
    //Linux DOES distinguish between upper/lower-case: nothing to do here
#endif
    /*
      phrase  | action
    +---------+--------
    | \blah   | remove \
    | \*blah  | remove \
    | \*\blah | remove \
    | \*\*    | remove \
    +---------+--------
    | *blah   |
    | *\blah  |	-> add blah
    | *\*blah | -> add *blah
    +---------+--------
    | blah\   | remove \; directory only
    | blah*\  | remove \; directory only
    | blah\*\ | remove \; directory only
    +---------+--------
    | blah*   |
    | blah\*  | add blah for directory only
    | blah*\* | add blah* for directory only
    +---------+--------
    */
    auto processTail = [&fileFilter, &directoryFilter](const Zstring& phrase)
    {
        if (endsWith(phrase, FILE_NAME_SEPARATOR)) //only relevant for directory filtering
        {
            const Zstring dirPhrase = beforeLast(phrase, FILE_NAME_SEPARATOR);
            if (!dirPhrase.empty())
                directoryFilter.push_back(dirPhrase);
        }
        else if (!phrase.empty())
        {
            fileFilter     .push_back(phrase);
            directoryFilter.push_back(phrase);
            if (endsWith(phrase, sepAsterisk)) // abc\*
            {
                const Zstring dirPhrase = beforeLast(phrase, sepAsterisk);
                if (!dirPhrase.empty())
                    directoryFilter.push_back(dirPhrase);
            }
        }
    };

    if (startsWith(filterFmt, FILE_NAME_SEPARATOR)) // \abc
        processTail(afterFirst(filterFmt, FILE_NAME_SEPARATOR));
    else
    {
        processTail(filterFmt);
        if (startsWith(filterFmt, asteriskSep)) // *\abc
            processTail(afterFirst(filterFmt, asteriskSep));
    }
}


namespace
{
template <class Char> inline
const Char* cStringFind(const Char* str, Char ch) //strchr()
{
    for (;;)
    {
        const Char s = *str;
        if (s == ch) //ch is allowed to be 0 by contract! must return end of string in this case
            return str;

        if (s == 0)
            return nullptr;
        ++str;
    }
}


bool matchesMask(const Zchar* str, const Zchar* mask)
{
    for (;; ++mask, ++str)
    {
        Zchar m = *mask;
        if (m == 0)
            return *str == 0;

        switch (m)
        {
            case Zstr('?'):
                if (*str == 0)
                    return false;
                break;

            case Zstr('*'):
                //advance mask to next non-* char
                do
                {
                    m = *++mask;
                }
                while (m == Zstr('*'));

                if (m == 0) //mask ends with '*':
                    return true;

                //*? - pattern
                if (m == Zstr('?'))
                {
                    ++mask;
                    while (*str++ != 0)
                        if (matchesMask(str, mask))
                            return true;
                    return false;
                }

                //*[letter] - pattern
                ++mask;
                for (;;)
                {
                    str = cStringFind(str, m);
                    if (!str)
                        return false;

                    ++str;
                    if (matchesMask(str, mask))
                        return true;
                }

            default:
                if (*str != m)
                    return false;
        }
    }
}


//returns true if string matches at least the beginning of mask
inline
bool matchesMaskBegin(const Zchar* str, const Zchar* mask)
{
    for (;; ++mask, ++str)
    {
        const Zchar m = *mask;
        if (m == 0)
            return *str == 0;

        switch (m)
        {
            case Zstr('?'):
                if (*str == 0)
                    return true;
                break;

            case Zstr('*'):
                return true;

            default:
                if (*str != m)
                    return *str == 0;
        }
    }
}
}


inline
bool matchesFilter(const Zstring& name, const std::vector<Zstring>& filter)
{
    return std::any_of(filter.begin(), filter.end(), [&](const Zstring& mask) { return matchesMask(name.c_str(), mask.c_str()); });
}


inline
bool matchesFilterBegin(const Zstring& name, const std::vector<Zstring>& filter)
{
    return std::any_of(filter.begin(), filter.end(), [&](const Zstring& mask) { return matchesMaskBegin(name.c_str(), mask.c_str()); });
}


std::vector<Zstring> splitByDelimiter(const Zstring& filterString)
{
    //delimiters may be ';' or '\n'
    std::vector<Zstring> output;

    const std::vector<Zstring> blocks = split(filterString, Zchar(';')); //split by less common delimiter first
    std::for_each(blocks.begin(), blocks.end(),
                  [&](const Zstring& item)
    {
        const std::vector<Zstring> blocks2 = split(item, Zchar('\n'));

        std::for_each(blocks2.begin(), blocks2.end(),
                      [&](Zstring entry)
        {
            trim(entry);
            if (!entry.empty())
                output.push_back(entry);
        });
    });

    return output;
}

//#################################################################################################
NameFilter::NameFilter(const Zstring& includeFilter, const Zstring& excludeFilter) :
    includeFilterTmp(includeFilter), //save constructor arguments for serialization
    excludeFilterTmp(excludeFilter)
{
    //no need for regular expressions: In tests wxRegex was by factor of 10 slower than wxString::Matches()

    //load filter into vectors of strings
    //delimiters may be ';' or '\n'
    const std::vector<Zstring>& includeList = splitByDelimiter(includeFilter);
    const std::vector<Zstring>& excludeList = splitByDelimiter(excludeFilter);

    //setup include/exclude filters for files and directories
    std::for_each(includeList.begin(), includeList.end(), [&](const Zstring& entry) { addFilterEntry(entry, filterFileIn, filterFolderIn); });
    std::for_each(excludeList.begin(), excludeList.end(), [&](const Zstring& entry) { addFilterEntry(entry, filterFileEx, filterFolderEx); });

    auto removeDuplicates = [](std::vector<Zstring>& cont)
    {
        std::vector<Zstring> output;
        std::set<Zstring> used;
        std::copy_if(cont.begin(), cont.end(), std::back_inserter(output), [&](const Zstring& item) { return used.insert(item).second; });
        output.swap(cont);
    };

    removeDuplicates(filterFileIn);
    removeDuplicates(filterFolderIn);
    removeDuplicates(filterFileEx);
    removeDuplicates(filterFolderEx);
}


bool NameFilter::passFileFilter(const Zstring& relFilename) const
{
#if defined ZEN_WIN || defined ZEN_MAC //Windows does NOT distinguish between upper/lower-case
    const Zstring& nameFmt = makeUpperCopy(relFilename);
#elif defined ZEN_LINUX //Linux DOES distinguish between upper/lower-case
    const Zstring& nameFmt = relFilename; //nothing to do here
#endif

    return matchesFilter(nameFmt, filterFileIn) && //process include filters
           !matchesFilter(nameFmt, filterFileEx);  //process exclude filters
}


bool NameFilter::passDirFilter(const Zstring& relDirname, bool* subObjMightMatch) const
{
    assert(!subObjMightMatch || *subObjMightMatch == true); //check correct usage

#if defined ZEN_WIN || defined ZEN_MAC //Windows does NOT distinguish between upper/lower-case
    const Zstring& nameFmt = makeUpperCopy(relDirname);
#elif defined ZEN_LINUX //Linux DOES distinguish between upper/lower-case
    const Zstring& nameFmt = relDirname; //nothing to do here
#endif

    if (matchesFilter(nameFmt, filterFolderEx)) //process exclude filters
    {
        if (subObjMightMatch)
            *subObjMightMatch = false; //exclude subfolders/subfiles as well
        return false;
    }

    if (!matchesFilter(nameFmt, filterFolderIn)) //process include filters
    {
        if (subObjMightMatch)
        {
            const Zstring& subNameBegin = nameFmt + FILE_NAME_SEPARATOR; //const-ref optimization

            *subObjMightMatch = matchesFilterBegin(subNameBegin, filterFileIn) || //might match a file in subdirectory
                                matchesFilterBegin(subNameBegin, filterFolderIn); //or another subdirectory
        }
        return false;
    }

    return true;
}


bool NameFilter::isNull(const Zstring& includeFilter, const Zstring& excludeFilter)
{
    Zstring include = includeFilter;
    Zstring exclude = excludeFilter;
    trim(include);
    trim(exclude);

    return include == Zstr("*") && exclude.empty();
    //return NameFilter(includeFilter, excludeFilter).isNull(); -> very expensive for huge lists
}

bool NameFilter::isNull() const
{
    static NameFilter output(Zstr("*"), Zstring());
    return *this == output;
}


bool NameFilter::cmpLessSameType(const HardFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const NameFilter& otherNameFilt = static_cast<const NameFilter&>(other);

    if (filterFileIn != otherNameFilt.filterFileIn)
        return filterFileIn < otherNameFilt.filterFileIn;

    if (filterFolderIn != otherNameFilt.filterFolderIn)
        return filterFolderIn < otherNameFilt.filterFolderIn;

    if (filterFileEx != otherNameFilt.filterFileEx)
        return filterFileEx < otherNameFilt.filterFileEx;

    if (filterFolderEx != otherNameFilt.filterFolderEx)
        return filterFolderEx < otherNameFilt.filterFolderEx;

    return false; //vectors equal
}


//void NameFilter::save(ZstreamOut& stream) const
//{
//    writeString(stream, includeFilterTmp);
//    writeString(stream, excludeFilterTmp);
//}
//
//
//HardFilter::FilterRef NameFilter::load(ZstreamIn& stream) //throw UnexpectedEndOfStreamError
//{
//    const Zstring include = readString<Zstring>(stream); //throw UnexpectedEndOfStreamError
//    const Zstring exclude = readString<Zstring>(stream); //
//
//    return FilterRef(new NameFilter(include, exclude));
//}
