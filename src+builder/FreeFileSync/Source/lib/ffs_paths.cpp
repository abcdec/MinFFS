// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "ffs_paths.h"
#include <zen/file_access.h>
#include <wx/stdpaths.h>
#include <wx/app.h>
#include <wx+/string_conv.h>

#ifdef ZEN_MAC
    #include <vector>
    #include <zen/scope_guard.h>
    #include <zen/osx_string.h>
    //keep in .cpp file to not pollute global namespace!
    #include <ApplicationServices/ApplicationServices.h> //LSFindApplicationForInfo
#endif

using namespace zen;


namespace
{
#if defined ZEN_WIN || defined ZEN_LINUX
inline
Zstring getExecutableDir() //directory containing executable WITH path separator at end
{
    return appendSeparator(beforeLast(utfCvrtTo<Zstring>(wxStandardPaths::Get().GetExecutablePath()), FILE_NAME_SEPARATOR));
}
#endif

#ifdef ZEN_WIN
inline
Zstring getInstallDir() //root install directory WITH path separator at end
{
    return appendSeparator(beforeLast(beforeLast(getExecutableDir(), FILE_NAME_SEPARATOR), FILE_NAME_SEPARATOR));
}
#endif


#ifdef ZEN_WIN
inline
bool isPortableVersion()
{
    return !(fileExists(getInstallDir() + L"uninstall.exe") || //created by NSIS
             dirExists (getInstallDir() + L"Uninstall"));      //created by Inno Setup
}
#elif defined ZEN_LINUX
inline
bool isPortableVersion() { return !endsWith(getExecutableDir(), "/bin/"); } //this check is a bit lame...
#endif
}


bool zen::manualProgramUpdateRequired()
{
#if defined ZEN_WIN || defined ZEN_MAC
    return true;
#elif defined ZEN_LINUX
    return true;
    //return isPortableVersion(); //locally installed version is updated by Launchpad
#endif
}


Zstring zen::getResourceDir()
{
    //make independent from wxWidgets global variable "appname"; support being called by RealtimeSync
    auto appName = wxTheApp->GetAppName();
    wxTheApp->SetAppName(L"FreeFileSync");
    ZEN_ON_SCOPE_EXIT(wxTheApp->SetAppName(appName));

#ifdef ZEN_WIN
    return getInstallDir();
#elif defined ZEN_LINUX
    if (isPortableVersion())
        return getExecutableDir();
    else //use OS' standard paths
        return appendSeparator(toZ(wxStandardPathsBase::Get().GetResourcesDir()));
#elif defined ZEN_MAC
    return appendSeparator(toZ(wxStandardPathsBase::Get().GetResourcesDir())); //if packaged, used "Contents/Resources", else the executable directory
#endif
}


Zstring zen::getConfigDir()
{
    //make independent from wxWidgets global variable "appname"; support being called by RealtimeSync
    auto appName = wxTheApp->GetAppName();
    wxTheApp->SetAppName(L"FreeFileSync");
    ZEN_ON_SCOPE_EXIT(wxTheApp->SetAppName(appName));

#ifdef ZEN_WIN
    if (isPortableVersion())
        return getInstallDir();
#elif defined ZEN_LINUX
    if (isPortableVersion())
        return getExecutableDir();
#elif defined ZEN_MAC
    //portable apps do not seem common on OS - fine with me: http://theocacao.com/document.page/319
#endif
    //use OS' standard paths
    Zstring userDirectory = toZ(wxStandardPathsBase::Get().GetUserDataDir());

    if (!dirExists(userDirectory))
        try
        {
            makeDirectory(userDirectory); //throw FileError
        }
        catch (const FileError&) {}

    return appendSeparator(userDirectory);
}


//this function is called by RealtimeSync!!!
Zstring zen::getFreeFileSyncLauncher()
{
#ifdef ZEN_WIN
    return getInstallDir() + Zstr("FreeFileSync.exe");

#elif defined ZEN_LINUX
    return getExecutableDir() + Zstr("FreeFileSync");

#elif defined ZEN_MAC
    CFURLRef appURL = nullptr;
    ZEN_ON_SCOPE_EXIT(if (appURL) ::CFRelease(appURL));

    if (::LSFindApplicationForInfo(kLSUnknownCreator, // OSType inCreator,
                                   CFSTR("Zenju.FreeFileSync"),//CFStringRef inBundleID,
                                   nullptr,           //CFStringRef inName,
                                   nullptr,           //FSRef *outAppRef,
                                   &appURL) == noErr) //CFURLRef *outAppURL
        if (appURL)
            if (CFURLRef absUrl = ::CFURLCopyAbsoluteURL(appURL))
            {
                ZEN_ON_SCOPE_EXIT(::CFRelease(absUrl));

                if (CFStringRef path = ::CFURLCopyFileSystemPath(absUrl, kCFURLPOSIXPathStyle))
                {
                    ZEN_ON_SCOPE_EXIT(::CFRelease(path));
                    return appendSeparator(osx::cfStringToZstring(path)) + "Contents/MacOS/FreeFileSync";
                }
            }
    return Zstr("./FreeFileSync"); //fallback: at least give some hint...
#endif
}
