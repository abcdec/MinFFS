// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 7.5         *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#ifndef FILE_ERROR_H_839567308565656789
#define FILE_ERROR_H_839567308565656789

#include <string>
#include "zstring.h"
#include "utf.h"
#include "sys_error.h" //we'll need this later anyway!

namespace zen
{
//A high-level exception class giving detailed context information for end users
class FileError
{
public:
    explicit FileError(const std::wstring& msg) : msg_(msg) {}
    FileError(const std::wstring& msg, const std::wstring& details) : msg_(msg + L"\n\n" + details) {}
    virtual ~FileError() {}

    const std::wstring& toString() const { return msg_; }

private:
    std::wstring msg_;
};

#define DEFINE_NEW_FILE_ERROR(X) struct X : public FileError { X(const std::wstring& msg) : FileError(msg) {} X(const std::wstring& msg, const std::wstring& descr) : FileError(msg, descr) {} };

DEFINE_NEW_FILE_ERROR(ErrorTargetExisting);
DEFINE_NEW_FILE_ERROR(ErrorTargetPathMissing);
DEFINE_NEW_FILE_ERROR(ErrorFileLocked);
DEFINE_NEW_FILE_ERROR(ErrorDifferentVolume);


//CAVEAT: thread-local Win32 error code is easily overwritten => evaluate *before* making any (indirect) system calls:
//-> MinGW + Win XP: "throw" statement allocates memory to hold the exception object => error code is cleared
//-> VC 2015, Debug: std::wstring allocator internally calls ::FlsGetValue()         => error code is cleared
#ifdef _MSC_VER
#define THROW_LAST_FILE_ERROR(msg, functionName)                           \
    do                                                                     \
    {                                                                      \
        const ErrorCode ecInternal = getLastError();                       \
        throw FileError(msg, formatSystemError(functionName, ecInternal)); \
        \
        __pragma(warning(suppress: 4127)) /*"conditional expression is constant"*/ \
    } while (false)

#else //same thing witout "__pragma":
#define THROW_LAST_FILE_ERROR(msg, functionName)                           \
    do { const ErrorCode ecInternal = getLastError(); throw FileError(msg, formatSystemError(functionName, ecInternal)); } while (false)
#endif

//----------- facilitate usage of std::wstring for error messages --------------------

inline
std::wstring fmtPath(const std::wstring& displayPath)
{
    return L'\"' + displayPath + L'\"';
}

inline std::wstring fmtPath(const Zstring& displayPath) { return fmtPath(utfCvrtTo<std::wstring>(displayPath)); }
inline std::wstring fmtPath(const wchar_t* displayPath) { return fmtPath(std::wstring(displayPath)); }
}

#endif //FILE_ERROR_H_839567308565656789
