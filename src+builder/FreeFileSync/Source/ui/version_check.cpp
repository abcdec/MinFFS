// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "version_check.h"
#include <zen/string_tools.h>
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/scope_guard.h>
#include <zen/build_info.h>
#include <zen/basic_math.h>
#include <zen/thread.h> //std::thread::id
//#include <wx/timer.h>
//#include <wx/utils.h>
#include <wx+/popup_dlg.h>
#include "version_id.h"

#ifdef ZEN_WIN
    #include <zen/win.h> //tame wininet include
    #include <zen/win_ver.h>
#include <zen/com_tools.h>
    #include <wininet.h>

#elif defined ZEN_MAC
    #include <CoreServices/CoreServices.h> //Gestalt()
#endif

#if defined ZEN_LINUX || defined ZEN_MAC
    #include <wx/protocol/http.h>
    #include <wx/app.h>
#endif


using namespace zen;


namespace
{
#ifndef ZEN_WIN
    #ifndef NDEBUG
        const std::thread::id mainThreadId = std::this_thread::get_id();
    #endif
#endif


std::wstring getIso639Language()
{
    //respect thread-safety for WinInetAccess => don't use wxWidgets in the Windows build here!!!

#ifdef ZEN_WIN //use a more reliable function than wxWidgets:
    const int bufSize = 10;
    wchar_t buf[bufSize] = {};
    int rv = ::GetLocaleInfo(LOCALE_USER_DEFAULT,   //_In_       LCID Locale,
                             LOCALE_SISO639LANGNAME,//_In_       LCTYPE LCType,
                             buf,                   //_Out_opt_  LPTSTR lpLCData,
                             bufSize);              //_In_       int cchData
    if (0 < rv && rv < bufSize)
        return buf; //MSDN: "This can be a 3-letter code for languages that don't have a 2-letter code"!
    assert(false);
    return std::wstring();
#else
    assert(std::this_thread::get_id() == mainThreadId);

    const std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    if (localeName.empty())
        return std::wstring();

    assert(beforeLast(localeName, L"_", IF_MISSING_RETURN_ALL).size() == 2);
    return beforeLast(localeName, L"_", IF_MISSING_RETURN_ALL);
#endif
}


std::wstring getIso3166Country()
{
    //respect thread-safety for WinInetAccess => don't use wxWidgets in the Windows build here!!!

#ifdef ZEN_WIN //use a more reliable function than wxWidgets:
    const int bufSize = 10;
    wchar_t buf[bufSize] = {};
    int rv = ::GetLocaleInfo(LOCALE_USER_DEFAULT,    //_In_       LCID Locale,
                             LOCALE_SISO3166CTRYNAME,//_In_       LCTYPE LCType,
                             buf,                    //_Out_opt_  LPTSTR lpLCData,
                             bufSize);               //_In_       int cchData
    if (0 < rv && rv < bufSize)
        return buf; //MSDN: "This can also return a number, such as "029" for Caribbean."!
    assert(false);
    return std::wstring();
#else
    assert(std::this_thread::get_id() == mainThreadId);

    const std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    if (localeName.empty())
        return std::wstring();

    return afterLast(localeName, L"_", IF_MISSING_RETURN_NONE);
#endif
}


std::wstring getUserAgentName()
{
    //1. coordinate with on_check_latest_version.php
    //2. respect thread-safety for WinInetAccess => don't use wxWidgets in the Windows build here!!!

    std::wstring agentName = std::wstring(L"FreeFileSync (") + zen::ffsVersion;

#ifdef ZEN_WIN
    agentName += L" Windows";
    const auto osvMajor = getOsVersion().major;
    const auto osvMinor = getOsVersion().minor;

#elif defined ZEN_LINUX
    assert(std::this_thread::get_id() == mainThreadId);

    const wxLinuxDistributionInfo distribInfo = wxGetLinuxDistributionInfo();
    assert(contains(distribInfo.Release, L'.'));
    std::vector<wxString> digits = split<wxString>(distribInfo.Release, L'.'); //e.g. "15.04"
    digits.resize(2);
    //distribInfo.Id //e.g. "Ubuntu"

    const int osvMajor = stringTo<int>(digits[0]);
    const int osvMinor = stringTo<int>(digits[1]);
    agentName += L" Linux";

#elif defined ZEN_MAC
    agentName += L" Mac";
    SInt32 osvMajor = 0;
    SInt32 osvMinor = 0;
    ::Gestalt(gestaltSystemVersionMajor, &osvMajor);
    ::Gestalt(gestaltSystemVersionMinor, &osvMinor);
#endif
    agentName += L" " + numberTo<std::wstring>(osvMajor) + L"." + numberTo<std::wstring>(osvMinor);

    agentName +=
#ifdef ZEN_WIN
        running64BitWindows() ? L" 64" : L" 32";

#elif defined ZEN_LINUX || defined ZEN_MAC
#ifdef ZEN_BUILD_32BIT
        L" 32";
#elif defined ZEN_BUILD_64BIT
        L" 64";
#endif
#endif

    const std::wstring isoLang    = getIso639Language();
    const std::wstring isoCountry = getIso3166Country();
    agentName += L" " + (!isoLang   .empty() ? isoLang    : L"zz");
    agentName += L" " + (!isoCountry.empty() ? isoCountry : L"ZZ");

    agentName += L")";
    return agentName;
}


class InternetConnectionError {};

#ifdef ZEN_WIN
//WinInet: 1. uses IE proxy settings! :) 2. follows HTTP redirects by default 3. swallows HTTPS
std::string readBytesFromUrl(const wchar_t* url) //throw InternetConnectionError
{
    //::InternetAttemptConnect(0) -> not working as expected: succeeds even when there is no internet connection!

    HINTERNET hInternet = ::InternetOpen(getUserAgentName().c_str(),   //_In_  LPCTSTR lpszAgent,
                                         INTERNET_OPEN_TYPE_PRECONFIG, //_In_  DWORD dwAccessType,
                                         nullptr,   //_In_  LPCTSTR lpszProxyName,
                                         nullptr,   //_In_  LPCTSTR lpszProxyBypass,
                                         0);        //_In_  DWORD dwFlags
    if (!hInternet)
        throw InternetConnectionError();
    ZEN_ON_SCOPE_EXIT(::InternetCloseHandle(hInternet));

    HINTERNET hRequest = ::InternetOpenUrl(hInternet, //_In_  HINTERNET hInternet,
                                           url,       //_In_  LPCTSTR lpszUrl,
                                           nullptr,   //_In_  LPCTSTR lpszHeaders,
                                           0,         //_In_  DWORD dwHeadersLength,
                                           INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI, //_In_  DWORD dwFlags,
                                           0);        //_In_  DWORD_PTR dwContext
    if (!hRequest) //fails with ERROR_INTERNET_NAME_NOT_RESOLVED if server not found => the server-relative part is checked by HTTP_QUERY_STATUS_CODE below!!!
        throw InternetConnectionError();
    ZEN_ON_SCOPE_EXIT(::InternetCloseHandle(hRequest));

    DWORD statusCode = 0;
    DWORD bufferLength = sizeof(statusCode);
    if (!::HttpQueryInfo(hRequest,      //_In_     HINTERNET hRequest,
                         HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, //_In_     DWORD dwInfoLevel,
                         &statusCode,   //_Inout_  LPVOID lpvBuffer,
                         &bufferLength, //_Inout_  LPDWORD lpdwBufferLength,
                         nullptr))      //_Inout_  LPDWORD lpdwIndex
        throw InternetConnectionError();

    if (statusCode != HTTP_STATUS_OK)
        throw InternetConnectionError(); //e.g. 404 - HTTP_STATUS_NOT_FOUND

    std::string buffer;
    const DWORD blockSize = 64 * 1024;
    //internet says "HttpQueryInfo() + HTTP_QUERY_CONTENT_LENGTH" not supported by all http servers...
    for (;;)
    {
        buffer.resize(buffer.size() + blockSize);

        DWORD bytesRead = 0;
        if (!::InternetReadFile(hRequest,    //_In_   HINTERNET hFile,
                                &*(buffer.begin() + buffer.size() - blockSize),  //_Out_  LPVOID lpBuffer,
                                blockSize,   //_In_   DWORD dwNumberOfBytesToRead,
                                &bytesRead)) //_Out_  LPDWORD lpdwNumberOfBytesRead
            throw InternetConnectionError();

        if (bytesRead < blockSize)
            buffer.resize(buffer.size() - (blockSize - bytesRead)); //caveat: unsigned arithmetics

        if (bytesRead == 0)
            return buffer;
    }
}


inline
bool internetIsAlive() //noexcept
{
    try
    {
        readBytesFromUrl(L"http://www.google.com/"); //throw InternetConnectionError
        return true;
    }
    catch (const InternetConnectionError&) { return false; }
}

#else
std::string readBytesFromUrl(const wxString& url, int level = 0) //throw InternetConnectionError
{
    assert(std::this_thread::get_id() == mainThreadId);
    assert(wxApp::IsMainLoopRunning());

    wxString urlFmt = url;
    assert(!startsWith(urlFmt, L"https:")); //not supported by wxHTTP!
    if (startsWith(urlFmt, L"http://"))
        urlFmt = afterFirst(urlFmt, L"://", IF_MISSING_RETURN_NONE);
    const wxString server =       beforeFirst(urlFmt, L'/', IF_MISSING_RETURN_ALL);
    const wxString page   = L'/' + afterFirst(urlFmt, L'/', IF_MISSING_RETURN_NONE);

    wxHTTP webAccess;
    webAccess.SetHeader(L"content-type", L"text/html; charset=utf-8");
    webAccess.SetHeader(L"USER-AGENT", getUserAgentName());

    webAccess.SetTimeout(5 /*[s]*/); //default: 10 minutes: WTF are these wxWidgets people thinking???

    if (!webAccess.Connect(server)) //will *not* fail for non-reachable url here!
        throw InternetConnectionError();

    std::unique_ptr<wxInputStream> httpStream(webAccess.GetInputStream(page)); //must be deleted BEFORE webAccess is closed
    const int rs = webAccess.GetResponse();

    if (rs == 301 || //http://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
        rs == 302 ||
        rs == 303 ||
        rs == 307 ||
        rs == 308)
        if (level < 5) //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
        {
            const wxString newUrl = webAccess.GetHeader(L"Location");
            if (!newUrl.empty())
                return readBytesFromUrl(newUrl, level + 1);
        }

    if (rs != 200 || //HTTP_STATUS_OK
        !httpStream || webAccess.GetError() != wxPROTO_NOERR)
        throw InternetConnectionError();

    std::string buffer;
    int newValue = 0;
    while ((newValue = httpStream->GetC()) != wxEOF)
        buffer.push_back(static_cast<char>(newValue));
    return buffer;
}


inline
bool internetIsAlive() //noexcept
{
    const wxString server = L"www.google.com";
    const wxString page   = L"/";

    wxHTTP webAccess;
    webAccess.SetHeader(L"content-type", L"text/html; charset=utf-8");
    webAccess.SetTimeout(5 /*[s]*/); //default: 10 minutes: WTF are these wxWidgets people thinking???

    if (!webAccess.Connect(server)) //will *not* fail for non-reachable url here!
        return false;

    std::unique_ptr<wxInputStream> httpStream(webAccess.GetInputStream(page)); //call before checking wxHTTP::GetResponse()
    const int rs = webAccess.GetResponse();

    return rs == 301 || //http://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
           rs == 302 ||
           rs == 303 ||
           rs == 307 ||
           rs == 308 ||
           rs == 200; //HTTP_STATUS_OK
    //attention: http://www.google.com/ might redirect to "https" => don't follow, just return "true"!!!
}
#endif


enum GetVerResult
{
    GET_VER_SUCCESS,
    GET_VER_NO_CONNECTION, //no internet connection or homepage down?
    GET_VER_PAGE_NOT_FOUND //version file seems to have moved! => trigger an update!
};

//access is thread-safe on Windows (WinInet), but not on Linux/OS X (wxWidgets)
GetVerResult getOnlineVersion(std::wstring& version)
{
    try
    {
        //harmonize with wxHTTP: latest_version.txt must not use https!!!
        const std::string buffer = readBytesFromUrl(L"http://www.freefilesync.org/latest_version.txt"); //throw InternetConnectionError
        version = utfCvrtTo<std::wstring>(buffer);
        trim(version); //Windows: remove trailing blank and newline
        return version.empty() ? GET_VER_PAGE_NOT_FOUND : GET_VER_SUCCESS; //empty version possible??
    }
    catch (const InternetConnectionError&)
    {
        return internetIsAlive() ? GET_VER_PAGE_NOT_FOUND : GET_VER_NO_CONNECTION;
    }
}


std::vector<size_t> parseVersion(const std::wstring& version)
{
    std::vector<std::wstring> digits = split(version, FFS_VERSION_SEPARATOR);

    std::vector<size_t> output;
    std::transform(digits.begin(), digits.end(), std::back_inserter(output), [&](const std::wstring& d) { return stringTo<size_t>(d); });
    return output;
}
}


bool zen::haveNewerVersionOnline(const std::wstring& onlineVersion)
{
    std::vector<size_t> current = parseVersion(zen::ffsVersion);
    std::vector<size_t> online  = parseVersion(onlineVersion);

    if (online.empty() || online[0] == 0) //online version string may be "This website has been moved..." In this case better check for an update
        return true;

    return std::lexicographical_compare(current.begin(), current.end(),
                                        online .begin(), online .end());
}


bool zen::updateCheckActive(time_t lastUpdateCheck)
{
    return lastUpdateCheck != getInactiveCheckId();
}


void zen::disableUpdateCheck(time_t& lastUpdateCheck)
{
    lastUpdateCheck = getInactiveCheckId();
}


void zen::checkForUpdateNow(wxWindow* parent, std::wstring& lastOnlineVersion)
{
    std::wstring onlineVersion;
    switch (getOnlineVersion(onlineVersion))
    {
        case GET_VER_SUCCESS:
            lastOnlineVersion = onlineVersion;
            if (haveNewerVersionOnline(onlineVersion))
            {
                switch (showConfirmationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                               setTitle(_("Check for Program Updates")).
                                               setMainInstructions(_("A new version of FreeFileSync is available:")  + L" " + onlineVersion + L"\n\n" + _("Download now?")),
                                               _("&Download")))
                {
                    case ConfirmationButton::DO_IT:
                        wxLaunchDefaultBrowser(L"http://www.freefilesync.org/get_latest.php");
                        break;
                    case ConfirmationButton::CANCEL:
                        break;
                }
            }
            else
                showNotificationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                       setTitle(_("Check for Program Updates")).
                                       setMainInstructions(_("FreeFileSync is up to date.")));
            break;

        case GET_VER_NO_CONNECTION:
            showNotificationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                   setTitle(("Check for Program Updates")).
                                   setMainInstructions(_("Unable to connect to www.freefilesync.org.")));
            break;

        case GET_VER_PAGE_NOT_FOUND:
            lastOnlineVersion = L"unknown";
            switch (showConfirmationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. Do you want to check manually?")),
                                           _("&Check")))
            {
                case ConfirmationButton::DO_IT:
                    wxLaunchDefaultBrowser(L"http://www.freefilesync.org/get_latest.php");
                    break;
                case ConfirmationButton::CANCEL:
                    break;
            }
            break;
    }
}


bool zen::shouldRunPeriodicUpdateCheck(time_t lastUpdateCheck)
{
    if (updateCheckActive(lastUpdateCheck))
    {
        static_assert(sizeof(time_t) >= 8, "Still using 32-bit time_t? WTF!!");
        const time_t now = std::time(nullptr);
        return numeric::dist(now, lastUpdateCheck) >= 7 * 24 * 3600; //check weekly
    }
    return false;
}


struct zen::UpdateCheckResult
{
#ifdef ZEN_WIN
    GetVerResult versionStatus = GET_VER_PAGE_NOT_FOUND;
    std::wstring onlineVersion;
#endif
};


std::shared_ptr<UpdateCheckResult> zen::retrieveOnlineVersion()
{
#ifdef ZEN_WIN
    try
    {
        ComInitializer ci; //throw SysError

        auto result = std::make_shared<UpdateCheckResult>();
        result->versionStatus = getOnlineVersion(result->onlineVersion); //access is thread-safe on Windows only!
        return result;
    }
    catch (SysError&) { assert(false); return nullptr; }
#else
    return nullptr;
#endif
}


void zen::evalPeriodicUpdateCheck(wxWindow* parent, time_t& lastUpdateCheck, std::wstring& lastOnlineVersion, const UpdateCheckResult* result)
{
#ifdef ZEN_WIN
    const GetVerResult versionStatus = result->versionStatus;
    const std::wstring onlineVersion = result->onlineVersion;
#else
    std::wstring onlineVersion;
    const GetVerResult versionStatus = getOnlineVersion(onlineVersion);
#endif

    switch (versionStatus)
    {
        case GET_VER_SUCCESS:
            lastUpdateCheck = std::time(nullptr);
            lastOnlineVersion = onlineVersion;

            if (haveNewerVersionOnline(onlineVersion))
            {
                switch (showConfirmationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                               setTitle(_("Check for Program Updates")).
                                               setMainInstructions(_("A new version of FreeFileSync is available:")  + L" " + onlineVersion + L"\n\n" + _("Download now?")),
                                               _("&Download")))
                {
                    case ConfirmationButton::DO_IT:
                        wxLaunchDefaultBrowser(L"http://www.freefilesync.org/get_latest.php");
                        break;
                    case ConfirmationButton::CANCEL:
                        break;
                }
            }
            break;

        case GET_VER_NO_CONNECTION:
            break; //ignore this error

        case GET_VER_PAGE_NOT_FOUND:
            lastOnlineVersion = L"unknown";
            switch (showConfirmationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. Do you want to check manually?")),
                                           _("&Check")))
            {
                case ConfirmationButton::DO_IT:
                    wxLaunchDefaultBrowser(L"http://www.freefilesync.org/get_latest.php");
                    break;
                case ConfirmationButton::CANCEL:
                    break;
            }
            break;
    }
}
