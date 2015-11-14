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

#include "application.h"
#include <memory>
#include <zen/file_access.h>
#include <wx/tooltip.h>
#include <wx/log.h>
#include <wx+/app_main.h>
#include <wx+/string_conv.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "comparison.h"
#include "algorithm.h"
#include "synchronization.h"
#include "ui/batch_status_handler.h"
#include "ui/main_dlg.h"
#include "ui/switch_to_gui.h"
#include "lib/help_provider.h"
#include "lib/process_xml.h"
#include "lib/error_log.h"
#include "lib/resolve_path.h"

#ifdef ZEN_WIN
    #include <zen/win_ver.h>
    #include <zen/dll.h>
    #include "lib/app_user_mode_id.h"

#elif defined ZEN_LINUX
    #include <gtk/gtk.h>
#endif

using namespace zen;
using namespace xmlAccess;


IMPLEMENT_APP(Application)

#ifdef _MSC_VER
//catch CRT floating point errors: http://msdn.microsoft.com/en-us/library/k3backsw.aspx
int _matherr(_Inout_ struct _exception* except)
{
    assert(false);
    return 0; //use default action
}
#endif


namespace
{
#ifdef ZEN_WIN
void enableCrashingOnCrashes() //should be needed for 32-bit code only: http://randomascii.wordpress.com/2012/07/05/when-even-crashing-doesnt-work
{
    typedef BOOL (WINAPI* GetProcessUserModeExceptionPolicyFun)(LPDWORD lpFlags);
    typedef BOOL (WINAPI* SetProcessUserModeExceptionPolicyFun)(  DWORD dwFlags);
    const DWORD EXCEPTION_SWALLOWING = 0x1;

    const SysDllFun<GetProcessUserModeExceptionPolicyFun> getProcessUserModeExceptionPolicy(L"kernel32.dll", "GetProcessUserModeExceptionPolicy");
    const SysDllFun<SetProcessUserModeExceptionPolicyFun> setProcessUserModeExceptionPolicy(L"kernel32.dll", "SetProcessUserModeExceptionPolicy");
    if (getProcessUserModeExceptionPolicy && setProcessUserModeExceptionPolicy) //available since Windows 7 SP1
    {
        DWORD dwFlags = 0;
        if (getProcessUserModeExceptionPolicy(&dwFlags) && (dwFlags & EXCEPTION_SWALLOWING))
            setProcessUserModeExceptionPolicy(dwFlags & ~EXCEPTION_SWALLOWING);
    }
}

#ifdef _MSC_VER
void crtInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved) { assert(false); }
#endif
#endif


std::vector<Zstring> getCommandlineArgs(const wxApp& app)
{
    std::vector<Zstring> args;
#ifdef ZEN_WIN
    //we do the job ourselves! both wxWidgets and ::CommandLineToArgvW() parse "C:\" "D:\" as single line C:\" D:\"
    //-> "solution": we just don't support protected quotation mark!
    Zstring cmdLine = ::GetCommandLine(); //only way to get a unicode commandline
    while (endsWith(cmdLine, L' ')) //may end with space
        cmdLine.resize(cmdLine.size() - 1);

    auto iterStart = cmdLine.end(); //end() means: no token
    for (auto it = cmdLine.begin(); it != cmdLine.end(); ++it)
        if (*it == L' ') //space commits token
        {
            if (iterStart != cmdLine.end())
            {
                args.emplace_back(iterStart, it);
                iterStart = cmdLine.end(); //expect consecutive blanks!
            }
        }
        else
        {
            //start new token
            if (iterStart == cmdLine.end())
                iterStart = it;

            if (*it == L'\"')
            {
                it = std::find(it + 1, cmdLine.end(), L'\"');
                if (it == cmdLine.end())
                    break;
            }
        }
    if (iterStart != cmdLine.end())
        args.emplace_back(iterStart, cmdLine.end());

    if (!args.empty())
        args.erase(args.begin()); //remove first argument which is exe path by convention: http://blogs.msdn.com/b/oldnewthing/archive/2006/05/15/597984.aspx

    for (Zstring& str : args)
        if (str.size() >= 2 && startsWith(str, L'\"') && endsWith(str, L'\"'))
            str = Zstring(str.c_str() + 1, str.size() - 2);

#else
    for (int i = 1; i < app.argc; ++i) //wxWidgets screws up once again making "argv implicitly convertible to a wxChar**" in 2.9.3,
        args.push_back(toZ(wxString(app.argv[i]))); //so we are forced to use this pitiful excuse for a range construction!!
#endif
    return args;
}

const wxEventType EVENT_ENTER_EVENT_LOOP = wxNewEventType();
}

//##################################################################################################################

bool Application::OnInit()
{
#ifdef ZEN_WIN
    enableCrashingOnCrashes();
#ifdef _MSC_VER
    _set_invalid_parameter_handler(crtInvalidParameterHandler); //see comment in <zen/time.h>
#endif
    //Quote: "Best practice is that all applications call the process-wide ::SetErrorMode() function with a parameter of
    //SEM_FAILCRITICALERRORS at startup. This is to prevent error mode dialogs from hanging the application."
    ::SetErrorMode(SEM_FAILCRITICALERRORS);

#ifdef TODO_MinFFS_SET_APP_USER_MODEL_ID
    setAppUserModeId(L"FreeFileSync", L"Zenju.FreeFileSync"); //noexcept
#endif//TODO_MinFFS_SET_APP_USER_MODEL_ID
    //consider: FreeFileSync.exe, FreeFileSync_Win32.exe, FreeFileSync_x64.exe

    wxToolTip::SetMaxWidth(-1); //disable tooltip wrapping -> Windows only

#elif defined ZEN_LINUX
    ::gtk_init(nullptr, nullptr);
    ::gtk_rc_parse((getResourceDir() + "styles.gtk_rc").c_str()); //remove inner border from bitmap buttons
#endif

    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::SetAutoPop(7000); //http://msdn.microsoft.com/en-us/library/windows/desktop/aa511495.aspx

    SetAppName(L"FreeFileSync"); //if not set, the default is the executable's name!

    initResourceImages(getResourceDir() + Zstr("Resources.zip"));

    Connect(wxEVT_QUERY_END_SESSION, wxEventHandler(Application::onQueryEndSession), nullptr, this);
    Connect(wxEVT_END_SESSION,       wxEventHandler(Application::onQueryEndSession), nullptr, this);

    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    //Note: app start is deferred: batch mode requires the wxApp eventhandler to be established for UI update events. This is not the case at the time of OnInit()!
    Connect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);
    wxCommandEvent scrollEvent(EVENT_ENTER_EVENT_LOOP);
    AddPendingEvent(scrollEvent);

    return true; //true: continue processing; false: exit immediately.
}


int Application::OnExit()
{
    uninitializeHelp();
    releaseWxLocale();
    return wxApp::OnExit();
}


void Application::onEnterEventLoop(wxEvent& event)
{
    Disconnect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);

    //determine FFS mode of operation
    std::vector<Zstring> commandArgs = getCommandlineArgs(*this);
    launch(commandArgs);
}


#ifdef ZEN_MAC
    /*
    wxWidgets initialization sequence on OS X is a mess:
    ----------------------------------------------------
    1. double click FFS app bundle or execute from command line without arguments
    OnInit()
    OnRun()
    onEnterEventLoop()
    MacNewFile()

    2. double-click .ffs_gui file
    OnInit()
    OnRun()
    onEnterEventLoop()
    MacOpenFiles()

    3. start from command line with .ffs_gui file as first argument
    OnInit()
    OnRun()
    MacOpenFiles() -> WTF!?
    onEnterEventLoop()
    MacNewFile()   -> yes, wxWidgets screws up once again: http://trac.wxwidgets.org/ticket/14558

    => solution: map Apple events to regular command line via launcher
    */
#endif


int Application::OnRun()
{
    auto processException = [](const std::wstring& msg)
    {
        //it's not always possible to display a message box, e.g. corrupted stack, however low-level file output works!
        logError(utfCvrtTo<std::string>(msg));
        wxSafeShowMessage(L"FreeFileSync - " + _("An exception occurred"), msg);
    };

    try
    {
        wxApp::OnRun();
    }
    catch (const std::exception& e) //catch all STL exceptions
    {
        processException(utfCvrtTo<std::wstring>(e.what()));
        return FFS_RC_EXCEPTION;
    }
    /* -> let it crash and create mini dump!!!
    catch (...)
    {
        processException(L"Unknown error.");
        return FFS_RC_EXCEPTION;
    }
    */
    return returnCode;
}


void Application::onQueryEndSession(wxEvent& event)
{
    if (auto mainWin = dynamic_cast<MainDialog*>(GetTopWindow()))
        mainWin->onQueryEndSession();
    OnExit(); //wxWidgets screws up again: http://trac.wxwidgets.org/ticket/3069
    //wxEntryCleanup(); -> gives popup "dll init failed" on XP
    std::exit(returnCode); //Windows will terminate anyway: destruct global objects
}


void runGuiMode  (const Zstring& globalConfigFile);
void runGuiMode  (const Zstring& globalConfigFile, const XmlGuiConfig& guiCfg, const std::vector<Zstring>& referenceFiles, bool startComparison);
void runBatchMode(const Zstring& globalConfigFile, const XmlBatchConfig& batchCfg, const Zstring& referenceFile, FfsReturnCode& returnCode);
void showSyntaxHelp();


void Application::launch(const std::vector<Zstring>& commandArgs)
{
    //wxWidgets app exit handling is weird... we want to exit only if the logical main window is closed, not just *any* window!
    wxTheApp->SetExitOnFrameDelete(false); //prevent popup-windows from becoming temporary top windows leading to program exit after closure
    ZEN_ON_SCOPE_EXIT(if (!mainWindowWasSet()) wxTheApp->ExitMainLoop();); //quit application, if no main window was set (batch silent mode)

    try
    {
        //tentatively set program language to OS default until GlobalSettings.xml is read later
        setLanguage(xmlAccess::XmlGlobalSettings().programLanguage); //throw FileError
    }
    catch (const FileError&) { assert(false); }

    auto notifyError = [&](const std::wstring& msg, const std::wstring& title)
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setTitle(title).setDetailInstructions(msg));
        raiseReturnCode(returnCode, FFS_RC_ABORTED);
    };

    //parse command line arguments
    std::vector<Zstring> dirPathPhrasesLeft;
    std::vector<Zstring> dirPathPhrasesRight;
    std::vector<std::pair<Zstring, XmlType>> configFiles; //XmlType: batch or GUI files only
    Opt<Zstring> globalConfigFile;
    bool openForEdit = false;
    {
        const Zchar optionEdit    [] = Zstr("-edit");
        const Zchar optionLeftDir [] = Zstr("-leftdir");
        const Zchar optionRightDir[] = Zstr("-rightdir");

        auto syntaxHelpRequested = [&](const Zstring& arg)
        {
            auto it = std::find_if(arg.begin(), arg.end(), [](Zchar c) { return c != Zchar('/') && c != Zchar('-'); });
            if (it == arg.begin()) return false; //require at least one prefix character

            const Zstring argTmp(it, arg.end());
            return equalNoCase(argTmp, Zstr("help")) ||
                   equalNoCase(argTmp, Zstr("h"))    ||
                   argTmp == Zstr("?");
        };

        for (auto it = commandArgs.begin(); it != commandArgs.end(); ++it)
            if (syntaxHelpRequested(*it))
                return showSyntaxHelp();
            else if (equalNoCase(*it, optionEdit))
                openForEdit = true;
            else if (equalNoCase(*it, optionLeftDir))
            {
                if (++it == commandArgs.end())
                {
                    notifyError(replaceCpy(_("A directory path is expected after %x."), L"%x", utfCvrtTo<std::wstring>(optionLeftDir)), _("Syntax error"));
                    return;
                }
                dirPathPhrasesLeft.push_back(*it);
            }
            else if (equalNoCase(*it, optionRightDir))
            {
                if (++it == commandArgs.end())
                {
                    notifyError(replaceCpy(_("A directory path is expected after %x."), L"%x", utfCvrtTo<std::wstring>(optionRightDir)), _("Syntax error"));
                    return;
                }
                dirPathPhrasesRight.push_back(*it);
            }
            else
            {
                Zstring filePath = getResolvedFilePath(*it);

                if (!fileExists(filePath)) //...be a little tolerant
                {
                    if (fileExists(filePath + Zstr(".ffs_batch")))
                        filePath += Zstr(".ffs_batch");
                    else if (fileExists(filePath + Zstr(".ffs_gui")))
                        filePath += Zstr(".ffs_gui");
                    else if (fileExists(filePath + Zstr(".xml")))
                        filePath += Zstr(".xml");
                    else
                    {
                        notifyError(replaceCpy(_("Cannot find file %x."), L"%x", fmtPath(filePath)), std::wstring());
                        return;
                    }
                }

                try
                {
                    switch (getXmlType(filePath)) //throw FileError
                    {
                        case XML_TYPE_GUI:
                            configFiles.emplace_back(filePath, XML_TYPE_GUI);
                            break;
                        case XML_TYPE_BATCH:
                            configFiles.emplace_back(filePath, XML_TYPE_BATCH);
                            break;
                        case XML_TYPE_GLOBAL:
                            globalConfigFile = filePath;
                            break;
                        case XML_TYPE_OTHER:
                            notifyError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)), std::wstring());
                            return;
                    }
                }
                catch (const FileError& e)
                {
                    notifyError(e.toString(), std::wstring());
                    return;
                }
            }
    }

    if (dirPathPhrasesLeft.size() != dirPathPhrasesRight.size())
    {
        notifyError(_("Unequal number of left and right directories specified."), _("Syntax error"));
        return;
    }

    auto hasNonDefaultConfig = [](const FolderPairEnh& fp)
    {
        return !(fp == FolderPairEnh(fp.folderPathPhraseLeft_,
                                     fp.folderPathPhraseRight_,
                                     nullptr, nullptr, FilterConfig()));
    };

    auto replaceDirectories = [&](MainConfiguration& mainCfg)
    {
        if (!dirPathPhrasesLeft.empty())
        {
            //check if config at folder-pair level is present: this probably doesn't make sense when replacing/adding the user-specified directories
            if (hasNonDefaultConfig(mainCfg.firstPair) || std::any_of(mainCfg.additionalPairs.begin(), mainCfg.additionalPairs.end(), hasNonDefaultConfig))
            {
                notifyError(_("The config file must not contain settings at directory pair level when directories are set via command line."), _("Syntax error"));
                return false;
            }

            mainCfg.additionalPairs.clear();
            for (size_t i = 0; i < dirPathPhrasesLeft.size(); ++i)
                if (i == 0)
                {
                    mainCfg.firstPair.folderPathPhraseLeft_  = dirPathPhrasesLeft [0];
                    mainCfg.firstPair.folderPathPhraseRight_ = dirPathPhrasesRight[0];
                }
                else
                    mainCfg.additionalPairs.emplace_back(dirPathPhrasesLeft[i], dirPathPhrasesRight[i],
                                                         nullptr, nullptr, FilterConfig());
        }
        return true;
    };

    //distinguish sync scenarios:
    //---------------------------
    const Zstring globalConfigFilePath = globalConfigFile ? *globalConfigFile : xmlAccess::getGlobalConfigFile();

    if (configFiles.empty())
    {
        //gui mode: default startup
        if (dirPathPhrasesLeft.empty())
            runGuiMode(globalConfigFilePath);
        //gui mode: default config with given directories
        else
        {
            XmlGuiConfig guiCfg;
            guiCfg.mainCfg.syncCfg.directionCfg.var = DirectionConfig::MIRROR;

            if (!replaceDirectories(guiCfg.mainCfg))
                return;
            runGuiMode(globalConfigFilePath, guiCfg, std::vector<Zstring>(), !openForEdit);
        }
    }
    else if (configFiles.size() == 1)
    {
        const Zstring filepath = configFiles[0].first;

        //batch mode
        if (configFiles[0].second == XML_TYPE_BATCH && !openForEdit)
        {
            XmlBatchConfig batchCfg;
            try
            {
                std::wstring warningMsg;
                readConfig(filepath, batchCfg, warningMsg); //throw FileError

                if (!warningMsg.empty())
                    throw FileError(warningMsg); //batch mode: break on errors AND even warnings!
            }
            catch (const FileError& e)
            {
                notifyError(e.toString(), std::wstring());
                return;
            }
            if (!replaceDirectories(batchCfg.mainCfg))
                return;
            runBatchMode(globalConfigFilePath, batchCfg, filepath, returnCode);
        }
        //GUI mode: single config (ffs_gui *or* ffs_batch)
        else
        {
            XmlGuiConfig guiCfg;
            try
            {
                std::wstring warningMsg;
                readAnyConfig({ filepath }, guiCfg, warningMsg); //throw FileError

                if (!warningMsg.empty())
                    showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
                //what about simulating changed config on parsing errors?
            }
            catch (const FileError& e)
            {
                notifyError(e.toString(), std::wstring());
                return;
            }
            if (!replaceDirectories(guiCfg.mainCfg))
                return;
            //what about simulating changed config due to directory replacement?
            //-> propably fine to not show as changed on GUI and not ask user to save on exit!

            runGuiMode(globalConfigFilePath, guiCfg, { filepath }, !openForEdit); //caveat: guiCfg and filepath do not match if directories were set/replaced via command line!
        }
    }
    //gui mode: merged configs
    else
    {
        if (!dirPathPhrasesLeft.empty())
        {
            notifyError(_("Directories cannot be set for more than one configuration file."), _("Syntax error"));
            return;
        }

        std::vector<Zstring> filepaths;
        for (const auto& item : configFiles)
            filepaths.push_back(item.first);

        XmlGuiConfig guiCfg; //structure to receive gui settings with default values
        try
        {
            std::wstring warningMsg;
            readAnyConfig(filepaths, guiCfg, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about simulating changed config on parsing errors?
        }
        catch (const FileError& e)
        {
            notifyError(e.toString(), std::wstring());
            return;
        }
        runGuiMode(globalConfigFilePath, guiCfg, filepaths, !openForEdit);
    }
}


void runGuiMode(const Zstring& globalConfigFile) { MainDialog::create(globalConfigFile); }


void runGuiMode(const Zstring& globalConfigFile,
                const xmlAccess::XmlGuiConfig& guiCfg,
                const std::vector<Zstring>& referenceFiles,
                bool startComparison)
{
    MainDialog::create(globalConfigFile, nullptr, guiCfg, referenceFiles, startComparison);
}


void showSyntaxHelp()
{
    showNotificationDialog(nullptr, DialogInfoType::INFO, PopupDialogCfg().
                           setTitle(_("Command line")).
                           setDetailInstructions(_("Syntax:") + L"\n\n" +
#ifdef ZEN_WIN
                                                 L"FreeFileSync.exe " + L"\n" +
#elif defined ZEN_LINUX || defined ZEN_MAC
                                                 L"./FreeFileSync " + L"\n" +
#endif
                                                 L"    [" + _("global config file:") + L" GlobalSettings.xml]" + L"\n" +
                                                 L"    [" + _("config files:") + L" *.ffs_gui/*.ffs_batch]" + L"\n" +
                                                 L"    [-LeftDir " + _("directory") + L"] [-RightDir " + _("directory") + L"]" + L"\n" +
                                                 L"    [-Edit]" + L"\n\n" +

                                                 _("global config file:") + L"\n" +
                                                 _("Path to an alternate GlobalSettings.xml file.") + L"\n\n" +

                                                 _("config files:") + L"\n" +
                                                 _("Any number of FreeFileSync .ffs_gui and/or .ffs_batch configuration files.") + L"\n\n" +

                                                 L"-LeftDir " + _("directory") + L" -RightDir " + _("directory") + L"\n" +
                                                 _("Any number of alternative directory pairs for at most one config file.") + L"\n\n" +

                                                 L"-Edit" + L"\n" +
                                                 _("Open configuration for editing without executing it.")));
}


void runBatchMode(const Zstring& globalConfigFile, const XmlBatchConfig& batchCfg, const Zstring& referenceFile, FfsReturnCode& returnCode)
{
    auto notifyError = [&](const std::wstring& msg, FfsReturnCode rc)
    {
        if (batchCfg.handleError == ON_ERROR_POPUP)
            showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(msg));
        else //"exit" or "ignore"
            logError(utfCvrtTo<std::string>(msg));

        raiseReturnCode(returnCode, rc);
    };

    XmlGlobalSettings globalCfg;
    if (fileExists(globalConfigFile)) //else: globalCfg already has default values
        try
        {
            std::wstring warningMsg;
            readConfig(globalConfigFile, globalCfg, warningMsg); //throw FileError

            assert(warningMsg.empty()); //ignore parsing errors: should be migration problems only *cross-fingers*
        }
        catch (const FileError& e)
        {
            return notifyError(e.toString(), FFS_RC_ABORTED); //abort sync!
        }

    try
    {
        setLanguage(globalCfg.programLanguage); //throw FileError
    }
    catch (const FileError& e)
    {
        notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS);
        //continue!
    }

    //all settings have been read successfully...

    //regular check for program updates -> disabled for batch
    //if (batchCfg.showProgress && manualProgramUpdateRequired())
    //    checkForUpdatePeriodically(globalCfg.lastUpdateCheck);
    //WinInet not working when FFS is running as a service!!! https://support.microsoft.com/en-us/kb/238425

    try //begin of synchronization process (all in one try-catch block)
    {
        const TimeComp timeStamp = localTime();

        const SwitchToGui switchBatchToGui(globalConfigFile, globalCfg, referenceFile, batchCfg); //prepare potential operational switch

        //class handling status updates and error messages
        BatchStatusHandler statusHandler(!batchCfg.runMinimized, //throw BatchAbortProcess
                                         extractJobName(referenceFile),
                                         timeStamp,
                                         batchCfg.logFolderPathPhrase,
                                         batchCfg.logfilesCountLimit,
                                         globalCfg.lastSyncsLogFileSizeMax,
                                         batchCfg.handleError,
                                         globalCfg.automaticRetryCount,
                                         globalCfg.automaticRetryDelay,
                                         switchBatchToGui,
                                         returnCode,
                                         batchCfg.mainCfg.onCompletion,
                                         globalCfg.gui.onCompletionHistory);

        const std::vector<FolderPairCfg> cmpConfig = extractCompareCfg(batchCfg.mainCfg, globalCfg.fileTimeTolerance);

        bool allowPwPrompt = false;
        switch (batchCfg.handleError)
        {
            case ON_ERROR_POPUP:
                allowPwPrompt = true;
                break;
            case ON_ERROR_IGNORE:
            case ON_ERROR_STOP:
                break;
        }

        //batch mode: place directory locks on directories during both comparison AND synchronization
        std::unique_ptr<LockHolder> dirLocks;

        //COMPARE DIRECTORIES
        FolderComparison cmpResult = compare(globalCfg.optDialogs,
                                             allowPwPrompt, //allowUserInteraction
                                             globalCfg.runWithBackgroundPriority,
                                             globalCfg.createLockFile,
                                             dirLocks,
                                             cmpConfig,
                                             statusHandler);

        //START SYNCHRONIZATION
        const std::vector<FolderPairSyncCfg> syncProcessCfg = extractSyncCfg(batchCfg.mainCfg);
        if (syncProcessCfg.size() != cmpResult.size())
            throw std::logic_error("Programming Error: Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

        synchronize(timeStamp,
                    globalCfg.optDialogs,
                    globalCfg.verifyFileCopy,
                    globalCfg.copyLockedFiles,
                    globalCfg.copyFilePermissions,
                    globalCfg.failsafeFileCopy,
                    globalCfg.runWithBackgroundPriority,
                    syncProcessCfg,
                    cmpResult,
                    statusHandler);
    }
    catch (BatchAbortProcess&) {} //exit used by statusHandler

    try //save global settings to XML: e.g. ignored warnings
    {
        xmlAccess::writeConfig(globalCfg, globalConfigFile); //FileError
    }
    catch (const FileError& e)
    {
        notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS);
    }
}
