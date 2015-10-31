// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SYS_ERROR_H_3284791347018951324534
#define SYS_ERROR_H_3284791347018951324534

#include <string>
#include "utf.h"
#include "i18n.h"
#include "scope_guard.h"

#ifdef ZEN_WIN
    #include "win.h" //includes "windows.h"

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <cstring>
    #include <cerrno>
#endif


namespace zen
{
//evaluate GetLastError()/errno and assemble specific error message
#ifdef ZEN_WIN
    typedef DWORD ErrorCode;
#elif defined ZEN_LINUX || defined ZEN_MAC
    typedef int ErrorCode;
#endif

ErrorCode getLastError();

std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec);
std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec, const std::wstring& errorMsg);

//A low-level exception class giving (non-translated) detail information only - same conceptional level like "GetLastError()"!
class SysError
{
public:
    explicit SysError(const std::wstring& msg) : msg_(msg) {}
    const std::wstring& toString() const { return msg_; }

private:
    std::wstring msg_;
};

#define DEFINE_NEW_SYS_ERROR(X) struct X : public SysError { X(const std::wstring& msg) : SysError(msg) {} };







//######################## implementation ########################
inline
ErrorCode getLastError()
{
#ifdef ZEN_WIN
    return ::GetLastError();
#elif defined ZEN_LINUX || defined ZEN_MAC
    return errno; //don't use "::", errno is a macro!
#endif
}


std::wstring formatSystemErrorRaw(long long) = delete; //intentional overload ambiguity to catch usage errors

inline
std::wstring formatSystemErrorRaw(ErrorCode ec) //return empty string on error
{
    const ErrorCode currentError = getLastError(); //not necessarily == lastError

    std::wstring errorMsg;
#ifdef ZEN_WIN
    ZEN_ON_SCOPE_EXIT(::SetLastError(currentError)); //this function must not change active system error variable!

    LPWSTR buffer = nullptr;
    if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM    |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK |
                        FORMAT_MESSAGE_IGNORE_INSERTS | //important: without this flag ::FormatMessage() will fail if message contains placeholders
                        FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, ec, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr) != 0)
        if (buffer) //"don't trust nobody"
        {
            ZEN_ON_SCOPE_EXIT(::LocalFree(buffer));
            errorMsg = buffer;
        }

#elif defined ZEN_LINUX || defined ZEN_MAC
    ZEN_ON_SCOPE_EXIT(errno = currentError);

    errorMsg = utfCvrtTo<std::wstring>(::strerror(ec));
#endif
    trim(errorMsg); //Windows messages seem to end with a blank...

    return errorMsg;
}


std::wstring formatSystemError(const std::wstring& functionName, long long lastError) = delete; //intentional overload ambiguity to catch usage errors with HRESULT!

inline
std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec) { return formatSystemError(functionName, ec, formatSystemErrorRaw(ec)); }


inline
std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec, const std::wstring& errorMsg)
{
    std::wstring output = replaceCpy(_("Error Code %x:"), L"%x", numberTo<std::wstring>(ec));

    if (!errorMsg.empty())
    {
        output += L" ";
        output += errorMsg;
    }

    output += L" (" + functionName + L")";

    return output;
}

}

#endif //SYS_ERROR_H_3284791347018951324534
