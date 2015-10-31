// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef GUISTATUSHANDLER_H_INCLUDED
#define GUISTATUSHANDLER_H_INCLUDED

#include <zen/error_log.h>
#include <wx/event.h>
#include "progress_indicator.h"
#include "main_dlg.h"
#include "../lib/status_handler.h"
#include "../lib/process_xml.h"


//Exception class used to abort the "compare" and "sync" process
class GuiAbortProcess {};

//classes handling sync and compare errors as well as status feedback

//StatusHandlerTemporaryPanel(CompareProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerTemporaryPanel : private wxEvtHandler, public zen::StatusHandler //throw GuiAbortProcess
{
public:
    StatusHandlerTemporaryPanel(MainDialog& dlg);
    ~StatusHandlerTemporaryPanel();

    void initNewPhase(int objectsTotal, std::int64_t dataTotal, Phase phaseID) override;
    void forceUiRefresh() override;

    Response reportError     (const std::wstring& text, size_t retryNumber)            override;
    void     reportFatalError(const std::wstring& errorMessage)                        override;
    void     reportWarning   (const std::wstring& warningMessage, bool& warningActive) override;

    void abortProcessNow() override final; //throw GuiAbortProcess

private:
    void OnKeyPressed(wxKeyEvent& event);
    void OnAbortCompare(wxCommandEvent& event); //handle abort button click

    MainDialog& mainDlg;
    bool ignoreErrors;
};


//StatusHandlerFloatingDialog(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerFloatingDialog : public zen::StatusHandler
{
public:
    StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                size_t lastSyncsLogFileSizeMax,
                                xmlAccess::OnGuiError handleError,
                                size_t automaticRetryCount,
                                size_t automaticRetryDelay,
                                const std::wstring& jobName,
                                const Zstring& onCompletion,
                                std::vector<Zstring>& onCompletionHistory);
    ~StatusHandlerFloatingDialog();

    void initNewPhase       (int objectsTotal, std::int64_t dataTotal, Phase phaseID) override;
    void updateProcessedData(int objectsDelta, std::int64_t dataDelta               ) override;
    void reportInfo         (const std::wstring& text                               ) override;
    void forceUiRefresh     ()                                                        override;

    Response reportError     (const std::wstring& text, size_t retryNumber           ) override;
    void     reportFatalError(const std::wstring& errorMessage                       ) override;
    void     reportWarning   (const std::wstring& warningMessage, bool& warningActive) override;

    void abortProcessNow() override final; //throw GuiAbortProcess

private:
    void onProgressDialogTerminate();

    SyncProgressDialog* progressDlg; //managed to have shorter lifetime than this handler!
    const size_t lastSyncsLogFileSizeMax_;
    xmlAccess::OnGuiError handleError_;
    zen::ErrorLog errorLog;
    const size_t automaticRetryCount_;
    const size_t automaticRetryDelay_;
    const std::wstring jobName_;
    const time_t startTime_; //don't use wxStopWatch: may overflow after a few days due to ::QueryPerformanceCounter()
};


#endif // GUISTATUSHANDLER_H_INCLUDED
