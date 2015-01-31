// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.13        *
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

#ifndef FILEERROR_H_INCLUDED_839567308565656789
#define FILEERROR_H_INCLUDED_839567308565656789

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


//CAVEAT: evalulate global error code *before* "throw" statement which may overwrite error code
//due to a memory allocation before it creates the thrown instance! (e.g. affects MinGW + Win XP!!!)
template <class FE = FileError> inline
void throwFileError(const std::wstring& msg, const std::wstring& functionName, const ErrorCode ec) //throw FileError
{
    throw FE(msg, formatSystemError(functionName, ec));
}


//----------- facilitate usage of std::wstring for error messages --------------------

//allow implicit UTF8 conversion: since std::wstring models a GUI string, convenience is more important than performance
inline
std::wstring operator+(const std::wstring& lhs, const Zstring& rhs) { return std::wstring(lhs) += utfCvrtTo<std::wstring>(rhs); }

//we musn't put our overloads in namespace std, but namespace zen (+ using directive) is sufficient


    
inline
std::wstring fmtFileName(const Zstring& filepath)
{
    std::wstring output;
    output += L'\"';
    output += utfCvrtTo<std::wstring>(filepath);
    output += L'\"';
    return output;
}

#ifdef TODO_MinFFS_openDir_DLL_PROTO
#else//TODO_MinFFS_openDir_DLL_PROTO
inline
std::wstring fmtFileName(const std::wstring& filepath)
{
    Zstring zstringFilePath(filepath);
    return fmtFileName(zstringFilePath);
}    
#endif//TODO_MinFFS_openDir_DLL_PROTO
}

#endif //FILEERROR_H_INCLUDED_839567308565656789
