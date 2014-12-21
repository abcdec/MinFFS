// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ERRORLOGGING_H_INCLUDED
#define ERRORLOGGING_H_INCLUDED

#include <cassert>
#include <algorithm>
#include <vector>
#include <string>
#include "time.h"
#include "i18n.h"
#include "string_base.h"

namespace zen
{
enum MessageType
{
    TYPE_INFO        = 0x1,
    TYPE_WARNING     = 0x2,
    TYPE_ERROR       = 0x4,
    TYPE_FATAL_ERROR = 0x8,
};

typedef Zbase<wchar_t> MsgString; //std::wstring may employ small string optimization: we cannot accept bloating the "ErrorLog::entries" memory block below (think 1 million items)

struct LogEntry
{
    time_t      time;
    MessageType type;
    MsgString   message;
};

template <class String>
String formatMessage(const LogEntry& entry);


class ErrorLog
{
public:
    template <class String> //a wchar_t-based string!
    void logMsg(const String& text, MessageType type);

    int getItemCount(int typeFilter = TYPE_INFO | TYPE_WARNING | TYPE_ERROR | TYPE_FATAL_ERROR) const;

    //subset of std::vector<> interface:
    typedef std::vector<LogEntry>::const_iterator const_iterator;
    const_iterator begin() const { return entries.begin(); }
    const_iterator end  () const { return entries.end  (); }
    bool empty() const { return entries.empty(); }

private:
    std::vector<LogEntry> entries; //list of non-resolved errors and warnings
};









//######################## implementation ##########################
template <class String> inline
void ErrorLog::logMsg(const String& text, zen::MessageType type)
{
    const LogEntry newEntry = { std::time(nullptr), type, copyStringTo<MsgString>(text) };
    entries.push_back(newEntry);
}


inline
int ErrorLog::getItemCount(int typeFilter) const
{
    return static_cast<int>(std::count_if(entries.begin(), entries.end(), [&](const LogEntry& e) { return e.type & typeFilter; }));
}


namespace
{
template <class String>
String formatMessageImpl(const LogEntry& entry) //internal linkage
{
    auto getTypeName = [&]() -> std::wstring
    {
        switch (entry.type)
        {
            case TYPE_INFO:
                return _("Info");
            case TYPE_WARNING:
                return _("Warning");
            case TYPE_ERROR:
                return _("Error");
            case TYPE_FATAL_ERROR:
                return _("Serious Error");
        }
        assert(false);
        return std::wstring();
    };

    String formattedText = L"[" + formatTime<String>(FORMAT_TIME, localTime(entry.time)) + L"] " + copyStringTo<String>(getTypeName()) + L": ";
    const size_t prefixLen = formattedText.size();

    for (auto it = entry.message.begin(); it != entry.message.end(); )
        if (*it == L'\n')
        {
            formattedText += L'\n';

            String blanks;
            blanks.resize(prefixLen, L' ');
            formattedText += blanks;

            do //skip duplicate newlines
            {
                ++it;
            }
            while (it != entry.message.end() && *it == L'\n');
        }
        else
            formattedText += *it++;

    return formattedText;
}
}

template <class String> inline
String formatMessage(const LogEntry& entry) { return formatMessageImpl<String>(entry); }
}

#endif //ERRORLOGGING_H_INCLUDED
