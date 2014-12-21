// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "check_version.h"
#include <zen/string_tools.h>
#include <zen/i18n.h>
#include <zen/utf.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx+/popup_dlg.h>
#include "../version/version.h"
#include <zen/scope_guard.h>

#ifdef ZEN_WIN
    #include <zen/win.h> //tame wininet include
    #include <wininet.h>

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <wx/protocol/http.h>
    #include <wx/sstream.h>
#endif

using namespace zen;


namespace
{
std::wstring getUserAgentName()
{
    std::wstring agentName = std::wstring(L"FreeFileSync ") + zen::currentVersion;
#ifdef ZEN_WIN
    agentName += L" Windows";
#elif defined ZEN_LINUX
    agentName += L" Linux";
#elif defined ZEN_MAC
    agentName += L" Mac";
#else
#error wtf
#endif
    const std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    if (!localeName.empty())
        agentName += L" " + localeName;

    return agentName;
}


#ifdef ZEN_WIN
class InternetConnectionError {};

class WinInetAccess //1. uses IE proxy settings! :) 2. follows HTTP redirects by default
{
public:
    WinInetAccess(const wchar_t* url) //throw InternetConnectionError (if url cannot be reached; no need to also call readBytes())
    {
        //::InternetAttemptConnect(0) -> not working as expected: succeeds even when there is no internet connection!

        hInternet = ::InternetOpen(getUserAgentName().c_str(), //_In_  LPCTSTR lpszAgent,
                                   INTERNET_OPEN_TYPE_PRECONFIG, //_In_  DWORD dwAccessType,
                                   nullptr,	//_In_  LPCTSTR lpszProxyName,
                                   nullptr,	//_In_  LPCTSTR lpszProxyBypass,
                                   0);		//_In_  DWORD dwFlags
        if (!hInternet)
            throw InternetConnectionError();
        zen::ScopeGuard guardInternet = zen::makeGuard([&] { ::InternetCloseHandle(hInternet); });

        hRequest = ::InternetOpenUrl(hInternet, //_In_  HINTERNET hInternet,
                                     url,		//_In_  LPCTSTR lpszUrl,
                                     nullptr,   //_In_  LPCTSTR lpszHeaders,
                                     0,			//_In_  DWORD dwHeadersLength,
                                     INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI, //_In_  DWORD dwFlags,
                                     0);		  //_In_  DWORD_PTR dwContext
        if (!hRequest) //won't fail due to unreachable url here! There is no substitute for HTTP_QUERY_STATUS_CODE!!!
            throw InternetConnectionError();
        zen::ScopeGuard guardRequest = zen::makeGuard([&] { ::InternetCloseHandle(hRequest); });

        DWORD statusCode = 0;
        DWORD bufferLength = sizeof(statusCode);
        if (!::HttpQueryInfo(hRequest,		//_In_     HINTERNET hRequest,
                             HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, //_In_     DWORD dwInfoLevel,
                             &statusCode,	//_Inout_  LPVOID lpvBuffer,
                             &bufferLength,	//_Inout_  LPDWORD lpdwBufferLength,
                             nullptr))		//_Inout_  LPDWORD lpdwIndex
            throw InternetConnectionError();

        if (statusCode != HTTP_STATUS_OK)
            throw InternetConnectionError(); //e.g. 404 - HTTP_STATUS_NOT_FOUND

        guardRequest .dismiss();
        guardInternet.dismiss();
    }

    ~WinInetAccess()
    {
        ::InternetCloseHandle(hRequest);
        ::InternetCloseHandle(hInternet);
    }

    template <class OutputIterator>
    OutputIterator readBytes(OutputIterator result) //throw InternetConnectionError
    {
        //internet says "HttpQueryInfo() + HTTP_QUERY_CONTENT_LENGTH" not supported by all http servers...
        const DWORD bufferSize = 64 * 1024;
        std::vector<char> buffer(bufferSize);
        for (;;)
        {
            DWORD bytesRead = 0;
            if (!::InternetReadFile(hRequest, 	 //_In_   HINTERNET hFile,
                                    &buffer[0],  //_Out_  LPVOID lpBuffer,
                                    bufferSize,  //_In_   DWORD dwNumberOfBytesToRead,
                                    &bytesRead)) //_Out_  LPDWORD lpdwNumberOfBytesRead
                throw InternetConnectionError();
            if (bytesRead == 0)
                return result;

            result = std::copy(buffer.begin(), buffer.begin() + bytesRead, result);
        }
    }

private:
    HINTERNET hInternet;
    HINTERNET hRequest;
};


inline
bool canAccessUrl(const wchar_t* url) //throw ()
{
    try
    {
        (void)WinInetAccess(url); //throw InternetConnectionError
        return true;
    }
    catch (const InternetConnectionError&) { return false; }
}


template <class OutputIterator> inline
OutputIterator readBytesUrl(const wchar_t* url, OutputIterator result) //throw InternetConnectionError
{
    return WinInetAccess(url).readBytes(result); //throw InternetConnectionError
}

#else
bool getStringFromUrl(const wxString& server, const wxString& page, int timeout, wxString* output, int level = 0) //true on successful connection
{
    wxHTTP webAccess;
    webAccess.SetHeader(L"content-type", L"text/html; charset=utf-8");
    webAccess.SetHeader(L"USER-AGENT", getUserAgentName());

    webAccess.SetTimeout(timeout); //default: 10 minutes(WTF are these wxWidgets people thinking???)...

    if (webAccess.Connect(server)) //will *not* fail for non-reachable url here!
    {
        //wxApp::IsMainLoopRunning(); // should return true

        std::unique_ptr<wxInputStream> httpStream(webAccess.GetInputStream(page));
        //must be deleted BEFORE webAccess is closed
        const int rs = webAccess.GetResponse();

        if (rs == 301 || //http://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
            rs == 302 ||
            rs == 303 ||
            rs == 307)
            if (level < 5) //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
            {

                wxString newLocation = webAccess.GetHeader(L"Location");
                if (!newLocation.empty())
                {
                    if (startsWith(newLocation, L"http://"))
                        newLocation = afterFirst(newLocation, L"http://");
                    const wxString serverNew = beforeFirst(newLocation, L"/"); //returns the whole string if term not found
                    const wxString pageNew   = L"/" + afterFirst(newLocation, L"/"); //returns empty string if term not found

                    return getStringFromUrl(serverNew, pageNew, timeout, output, level + 1);
                }
            }

        if (rs == 200) //HTTP_STATUS_OK
            if (httpStream && webAccess.GetError() == wxPROTO_NOERR)
            {
                if (output)
                {
                    output->clear();
                    wxStringOutputStream outStream(output);
                    httpStream->Read(outStream);
                }
                return true;
            }
    }
    return false;
}
#endif


enum GetVerResult
{
    GET_VER_SUCCESS,
    GET_VER_NO_CONNECTION, //no internet connection or homepage down?
    GET_VER_PAGE_NOT_FOUND //version file seems to have moved! => trigger an update!
};

GetVerResult getOnlineVersion(wxString& version) //empty string on error;
{
#ifdef ZEN_WIN
    //internet access supporting proxy connections
    std::vector<char> output;
    try
    {
        readBytesUrl(L"http://www.freefilesync.org/latest_version.txt", std::back_inserter(output)); //throw InternetConnectionError
    }
    catch (const InternetConnectionError&)
    {
        return canAccessUrl(L"http://www.freefilesync.org/") ? GET_VER_PAGE_NOT_FOUND : GET_VER_NO_CONNECTION;
    }

    output.push_back('\0');
    version = utfCvrtTo<wxString>(&output[0]);
    return GET_VER_SUCCESS;

#elif defined ZEN_LINUX || defined ZEN_MAC
    wxWindowDisabler dummy;

    if (getStringFromUrl(L"www.freefilesync.org", L"/latest_version.txt", 5, &version))
        return GET_VER_SUCCESS;

    const bool canConnectToSf = getStringFromUrl(L"www.freefilesync.org", L"/", 1, nullptr);
    return canConnectToSf ? GET_VER_PAGE_NOT_FOUND : GET_VER_NO_CONNECTION;
#endif
}


const wchar_t VERSION_SEP = L'.';

std::vector<size_t> parseVersion(const wxString& version)
{
    std::vector<wxString> digits = split(version, VERSION_SEP);

    std::vector<size_t> output;
    std::transform(digits.begin(), digits.end(), std::back_inserter(output), [&](const wxString& d) { return stringTo<size_t>(d); });
    return output;
}


bool haveNewerVersion(const wxString& onlineVersion)
{
    std::vector<size_t> current = parseVersion(zen::currentVersion);
    std::vector<size_t> online  = parseVersion(onlineVersion);

    if (online.empty() || online[0] == 0) //online version may be "This website has been moved..." In this case better check for an update
        return true;

    return std::lexicographical_compare(current.begin(), current.end(),
                                        online .begin(), online .end());
}
}


void zen::checkForUpdateNow(wxWindow* parent)
{
    wxString onlineVersion;
    switch (getOnlineVersion(onlineVersion))
    {
        case GET_VER_SUCCESS:
            if (haveNewerVersion(onlineVersion))
            {
                switch (showConfirmationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                               setTitle(_("Check for Program Updates")).
                                               setMainInstructions(_("A new version of FreeFileSync is available:")  + L" " + onlineVersion + L"\n\n" + _("Download now?")),
                                               _("&Download")))
                {
                    case ConfirmationButton::DO_IT:
                        wxLaunchDefaultBrowser(L"http://freefilesync.org/get_latest.php");
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
                                   setMainInstructions(_("Unable to connect to FreeFileSync.org.")));
            break;

        case GET_VER_PAGE_NOT_FOUND:
            switch (showConfirmationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. Do you want to check manually?")),
                                           _("&Check")))
            {
                case ConfirmationButton::DO_IT:
                    wxLaunchDefaultBrowser(L"http://www.freefilesync.org/");
                    break;
                case ConfirmationButton::CANCEL:
                    break;
            }
            break;
    }
}


void zen::checkForUpdatePeriodically(wxWindow* parent, long& lastUpdateCheck, const std::function<void()>& onBeforeInternetAccess)
{
    if (lastUpdateCheck != -1)
    {
        if (wxGetLocalTime() >= lastUpdateCheck + 7 * 24 * 3600) //check weekly
        {
            onBeforeInternetAccess(); //notify client before (potentially) blocking some time
            wxString onlineVersion;
            switch (getOnlineVersion(onlineVersion))
            {
                case GET_VER_SUCCESS:
                    lastUpdateCheck = wxGetLocalTime();

                    if (haveNewerVersion(onlineVersion))
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
                    switch (showConfirmationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                                   setTitle(_("Check for Program Updates")).
                                                   setMainInstructions(_("Cannot find current FreeFileSync version number online. Do you want to check manually?")),
                                                   _("&Check")))
                    {
                        case ConfirmationButton::DO_IT:
                            wxLaunchDefaultBrowser(L"http://www.freefilesync.org/");
                            break;
                        case ConfirmationButton::CANCEL:
                            break;
                    }
                    break;
            }
        }
    }
}
