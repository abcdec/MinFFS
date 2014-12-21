// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef LONGPATHPREFIX_H_INCLUDED
#define LONGPATHPREFIX_H_INCLUDED

#include "win.h"
#include "zstring.h"

namespace zen
{
//handle filepaths longer-equal 260 (== MAX_PATH) characters by applying \\?\-prefix; see: http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
/*
1. path must be absolute
2. if path is smaller than MAX_PATH nothing is changed! caveat: FindFirstFile() "Prepending the string "\\?\" does not allow access to the root directory."
3. path may already contain \\?\-prefix
*/
Zstring applyLongPathPrefix(const Zstring& path); //noexcept
Zstring applyLongPathPrefixCreateDir(const Zstring& path); //noexcept -> special rule for ::CreateDirectory()/::CreateDirectoryEx(): MAX_PATH - 12(=^ 8.3 filepath) is threshold

Zstring removeLongPathPrefix(const Zstring& path); //noexcept


Zstring ntPathToWin32Path(const Zstring& path); //noexcept
/*
http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx#NT_Namespaces

As used by GetModuleFileNameEx() and symlinks (FSCTL_GET_REPARSE_POINT):
	E.g.:
	\??\C:\folder -> C:\folder
	\SystemRoot   -> C:\Windows
*/
}






















//################## implementation ##################

//there are two flavors of long path prefix: one for UNC paths, one for regular paths
const wchar_t LONG_PATH_PREFIX    [] = L"\\\\?\\";    //don't use Zstring as global constant: avoid static initialization order problem in global namespace!
const wchar_t LONG_PATH_PREFIX_UNC[] = L"\\\\?\\UNC"; //

template <size_t maxPath> inline
Zstring applyLongPathPrefixImpl(const Zstring& path)
{
    assert(!path.empty()); //nicely check almost all WinAPI accesses!
    assert(!zen::isWhiteSpace(path[0]));

    //http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx#naming_conventions))
    /*
      - special names like ".NUL" create all kinds of trouble (e.g. CreateDirectory() reports success, but does nothing)
        unless prefix is supplied => accept as limitation
    */
    if (path.length() >= maxPath || //maximum allowed path length without prefix is (MAX_PATH - 1)
        endsWith(path, L' ') || //by default all Win32 APIs trim trailing spaces and period, unless long path prefix is supplied!
        endsWith(path, L'.'))   //note: adding long path prefix might screw up relative paths "." and ".."!
        if (!startsWith(path, LONG_PATH_PREFIX))
        {
            if (startsWith(path, L"\\\\")) //UNC-name, e.g. \\zenju-pc\Users
                return LONG_PATH_PREFIX_UNC + afterFirst(path, L'\\'); //convert to \\?\UNC\zenju-pc\Users
            else
                return LONG_PATH_PREFIX + path; //prepend \\?\ prefix
        }
    return path; //fallback
}


inline
Zstring zen::applyLongPathPrefix(const Zstring& path)
{
    return applyLongPathPrefixImpl<MAX_PATH>(path);
}


inline
Zstring zen::applyLongPathPrefixCreateDir(const Zstring& path) //noexcept
{
    //special rule for ::CreateDirectoryEx(): MAX_PATH - 12(=^ 8.3 filepath) is threshold
    return applyLongPathPrefixImpl<MAX_PATH - 12> (path);
}


inline
Zstring zen::removeLongPathPrefix(const Zstring& path) //noexcept
{
    if (zen::startsWith(path, LONG_PATH_PREFIX))
    {
        if (zen::startsWith(path, LONG_PATH_PREFIX_UNC)) //UNC-name
            return replaceCpy(path, LONG_PATH_PREFIX_UNC, Zstr("\\"), false);
        else
            return replaceCpy(path, LONG_PATH_PREFIX, Zstr(""), false);
    }
    return path; //fallback
}


inline
Zstring zen::ntPathToWin32Path(const Zstring& path) //noexcept
{
    if (startsWith(path, L"\\??\\"))
        return Zstring(path.c_str() + 4, path.length() - 4);

    if (startsWith(path, L"\\SystemRoot\\"))
    {
        DWORD bufSize = ::GetEnvironmentVariable(L"SystemRoot", nullptr, 0);
        if (bufSize > 0)
        {
            std::vector<wchar_t> buf(bufSize);
            DWORD charsWritten = ::GetEnvironmentVariable(L"SystemRoot", //_In_opt_   LPCTSTR lpName,
                                                          &buf[0],       //_Out_opt_  LPTSTR lpBuffer,
                                                          bufSize);      //_In_       DWORD nSize

            if (charsWritten != 0 && charsWritten < bufSize)
                return replaceCpy(path, L"\\SystemRoot\\", appendSeparator(Zstring(&buf[0], charsWritten)), false);
        }
    }

    return path;
}

#endif //LONGPATHPREFIX_H_INCLUDED
