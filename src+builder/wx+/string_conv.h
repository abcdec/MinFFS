// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef STRINGCONV_H_INCLUDED
#define STRINGCONV_H_INCLUDED

#include <zen/utf.h>
#include <wx/string.h>
#include <zen/zstring.h>

namespace zen
{
//conversion between Zstring and wxString
inline wxString toWx(const Zstring&  str) { return utfCvrtTo<wxString>(str); }
inline Zstring   toZ(const wxString& str) { return utfCvrtTo<Zstring>(str); }

inline std::vector<Zstring> toZ(const std::vector<wxString>& strList)
{
    std::vector<Zstring> tmp;
    std::transform(strList.begin(), strList.end(), std::back_inserter(tmp), [](const wxString& str) { return toZ(str); });
    return tmp;
}
}

#endif // STRINGCONV_H_INCLUDED
