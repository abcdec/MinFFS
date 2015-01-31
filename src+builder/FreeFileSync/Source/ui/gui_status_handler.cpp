// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "gui_status_handler.h"
#include <zen/shell_execute.h>
#include <wx/app.h>
#include <wx/wupdlock.h>
#include <wx+/bitmap_button.h>
#include <wx+/popup_dlg.h>
#include "main_dlg.h"
#include "on_completion_box.h"
#include "../lib/generate_logfile.h"
#include "../lib/resolve_path.h"
#include "../lib/status_handler_impl.h"

using namespace zen;
using namespace xmlAccess;


CompareStatusHandler::CompareStatusHandler(MainDialog& dlg) :
    mainDlg(dlg),
    ignoreErrors(false)
{
    {
#ifdef ZEN_WIN
        wxWindowUpdateLocker dummy(&mainDlg); //leads to GUI corruption problems on Linux/OS X!
#endif

        mainDlg.compareStatus->init(*this); //clear old values before showing panel

        //------------------------------------------------------------------
        const wxAuiPaneInfo& topPanel = mainDlg.auiMgr.GetPane(mainDlg.m_panelTopButtons);
        wxAuiPaneInfo& statusPanel    = mainDlg.auiMgr.GetPane(mainDlg.compareStatus->getAsWindow());

        //determine best status panel row near top panel
        switch (topPanel.dock_direction)
        {
            case wxAUI_DOCK_TOP:
            case wxAUI_DOCK_BOTTOM:
                statusPanel.Layer    (topPanel.dock_layer);
                statusPanel.Direction(topPanel.dock_direction);
                statusPanel.Row      (topPanel.dock_row + 1);
                break;

            case wxAUI_DOCK_LEFT:
            case wxAUI_DOCK_RIGHT:
                statusPanel.Layer    (std::max(0, topPanel.dock_layer - 1));
                statusPanel.Direction(wxAUI_DOCK_TOP);
                statusPanel.Row      (0);
                break;
                //case wxAUI_DOCK_CENTRE:
        }

        wxAuiPaneInfoArray& paneArray = mainDlg.auiMgr.GetAllPanes();

        const bool statusRowTaken = [&]() -> bool
        {
            for (size_t i = 0; i < paneArray.size(); ++i)
            {
                wxAuiPaneInfo& paneInfo = paneArray[i];

                if (&paneInfo != &statusPanel &&
                paneInfo.dock_layer     == statusPanel.dock_layer &&
                paneInfo.dock_direction == statusPanel.dock_direction &&
                paneInfo.dock_row       == statusPanel.dock_row)
                    return true;
            }
            return false;
        }();

        //move all rows that are in the way one step further
        if (statusRowTaken)
            for (size_t i = 0; i < paneArray.size(); ++i)
            {
                wxAuiPaneInfo& paneInfo = paneArray[i];

                if (&paneInfo != &statusPanel &&
                    paneInfo.dock_layer     == statusPanel.dock_layer &&
                    paneInfo.dock_direction == statusPanel.dock_direction &&
                    paneInfo.dock_row       >= statusPanel.dock_row)
                    ++paneInfo.dock_row;
            }
        //------------------------------------------------------------------

        statusPanel.Show();
        mainDlg.auiMgr.Update();
    }

    mainDlg.Update(); //don't wait until idle event!

    //register keys
    mainDlg.Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(CompareStatusHandler::OnKeyPressed), nullptr, this);
    mainDlg.m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CompareStatusHandler::OnAbortCompare), nullptr, this);
}


CompareStatusHandler::~CompareStatusHandler()
{
    //unregister keys
    mainDlg.Disconnect(wxEVT_CHAR_HOOK, wxKeyEventHandler(CompareStatusHandler::OnKeyPressed), nullptr, this);
    mainDlg.m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CompareStatusHandler::OnAbortCompare), nullptr, this);

    mainDlg.auiMgr.GetPane(mainDlg.compareStatus->getAsWindow()).Hide();
    mainDlg.auiMgr.Update();
    mainDlg.compareStatus->teardown();
}


void CompareStatusHandler::OnKeyPressed(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_ESCAPE)
    {
        wxCommandEvent dummy;
        OnAbortCompare(dummy);
    }

    event.Skip();
}


void CompareStatusHandler::initNewPhase(int objectsTotal, std::int64_t dataTotal, Phase phaseID)
{
    StatusHandler::initNewPhase(objectsTotal, dataTotal, phaseID);

    switch (currentPhase())
    {
        case PHASE_NONE:
        case PHASE_SYNCHRONIZING:
            assert(false);
        case PHASE_SCANNING:
            break;
        case PHASE_COMPARING_CONTENT:
        {
#ifdef ZEN_WIN
            wxWindowUpdateLocker dummy(&mainDlg); //leads to GUI corruption problems on Linux/OS X!
#endif
            mainDlg.compareStatus->switchToCompareBytewise();
            mainDlg.Layout();  //show progress bar...
            mainDlg.Refresh(); //remove distortion...
        }
        break;
    }

    forceUiRefresh(); //throw ?; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


ProcessCallback::Response CompareStatusHandler::reportError(const std::wstring& errorMessage, size_t retryNumber)
{
    //no need to implement auto-retry here: 1. user is watching 2. comparison is fast
    //=> similar behavior like "ignoreErrors" which does not honor sync settings

    if (ignoreErrors)
        return ProcessCallback::IGNORE_ERROR;

    forceUiRefresh();

    bool ignoreNextErrors = false;
    switch (showConfirmationDialog3(&mainDlg, DialogInfoType::ERROR2, PopupDialogCfg3().
                                    setDetailInstructions(errorMessage).
                                    setCheckBox(ignoreNextErrors, _("&Ignore subsequent errors"), ConfirmationButton3::DONT_DO_IT),
                                    _("&Ignore"), _("&Retry")))
    {
        case ConfirmationButton3::DO_IT: //ignore
            ignoreErrors = ignoreNextErrors;
            return ProcessCallback::IGNORE_ERROR;

        case ConfirmationButton3::DONT_DO_IT: //retry
            return ProcessCallback::RETRY;

        case ConfirmationButton3::CANCEL:
            abortProcessNow();
            break;
    }

    assert(false);
    return ProcessCallback::IGNORE_ERROR; //dummy return value
}


void CompareStatusHandler::reportFatalError(const std::wstring& errorMessage)
{
    forceUiRefresh();
    showNotificationDialog(&mainDlg, DialogInfoType::ERROR2, PopupDialogCfg().setTitle(_("Serious Error")).setDetailInstructions(errorMessage));
}


void CompareStatusHandler::reportWarning(const std::wstring& warningMessage, bool& warningActive)
{
    if (!warningActive || ignoreErrors) //if errors are ignored, then warnings should also
        return;

    forceUiRefresh();

    //show pop-up and ask user how to handle warning
    bool dontWarnAgain = false;
    switch (showConfirmationDialog(&mainDlg, DialogInfoType::WARNING,
                                   PopupDialogCfg().setDetailInstructions(warningMessage).
                                   setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                   _("&Ignore")))
    {
        case ConfirmationButton::DO_IT:
            warningActive = !dontWarnAgain;
            break;
        case ConfirmationButton::CANCEL:
            abortProcessNow();
            break;
    }
}


void CompareStatusHandler::forceUiRefresh()
{
    mainDlg.compareStatus->updateStatusPanelNow();
}


void CompareStatusHandler::OnAbortCompare(wxCommandEvent& event)
{
    requestAbortion();
}


void CompareStatusHandler::abortProcessNow()
{
    requestAbortion(); //just make sure...
    throw GuiAbortProcess();
}

//########################################################################################################

SyncStatusHandler::SyncStatusHandler(wxFrame* parentDlg,
                                     size_t lastSyncsLogFileSizeMax,
                                     OnGuiError handleError,
                                     size_t automaticRetryCount,
                                     size_t automaticRetryDelay,
                                     const std::wstring& jobName,
                                     const Zstring& onCompletion,
                                     std::vector<Zstring>& onCompletionHistory) :
    progressDlg(createProgressDialog(*this, [this] { this->onProgressDialogTerminate(); }, *this, parentDlg, true, jobName, onCompletion, onCompletionHistory)),
            lastSyncsLogFileSizeMax_(lastSyncsLogFileSizeMax),
            handleError_(handleError),
            automaticRetryCount_(automaticRetryCount),
            automaticRetryDelay_(automaticRetryDelay),
            jobName_(jobName),
startTime_(wxGetUTCTimeMillis().GetValue()) {}


SyncStatusHandler::~SyncStatusHandler()
{
    //------------ "on completion" command conceptually is part of the sync, not cleanup --------------------------------------

    //decide whether to stay on status screen or exit immediately...
    bool showFinalResults = true;

    if (progressDlg)
    {
        //execute "on completion" command (even in case of ignored errors)
        if (!abortIsRequested()) //if aborted (manually), we don't execute the command
        {
            const Zstring finalCommand = progressDlg->getExecWhenFinishedCommand(); //final value (after possible user modification)
            if (!finalCommand.empty())
            {
                if (isCloseProgressDlgCommand(finalCommand))
                    showFinalResults = false; //take precedence over current visibility status
                else
                    try
                    {
                        //use EXEC_TYPE_ASYNC until there is reason not to: https://sourceforge.net/p/freefilesync/discussion/help/thread/828dca52
                        tryReportingError([&] { shellExecute(expandMacros(finalCommand), EXEC_TYPE_ASYNC); }, //throw FileError
                                          *this); //throw X?
                    }
                    catch (...) {}
            }
        }
    }
    //------------ end of sync: begin of cleanup --------------------------------------

    const int totalErrors   = errorLog.getItemCount(TYPE_ERROR | TYPE_FATAL_ERROR); //evaluate before finalizing log
    const int totalWarnings = errorLog.getItemCount(TYPE_WARNING);

    //finalize error log
    std::wstring finalStatus;
    if (abortIsRequested())
    {
        finalStatus = _("Synchronization stopped");
        errorLog.logMsg(finalStatus, TYPE_ERROR);
    }
    else if (totalErrors > 0)
    {
        finalStatus = _("Synchronization completed with errors");
        errorLog.logMsg(finalStatus, TYPE_ERROR);
    }
    else if (totalWarnings > 0)
    {
        finalStatus = _("Synchronization completed with warnings");
        errorLog.logMsg(finalStatus, TYPE_WARNING); //give status code same warning priority as display category!
    }
    else
    {
        if (getObjectsTotal(PHASE_SYNCHRONIZING) == 0 && //we're past "initNewPhase(PHASE_SYNCHRONIZING)" at this point!
            getDataTotal   (PHASE_SYNCHRONIZING) == 0)
            finalStatus = _("Nothing to synchronize"); //even if "ignored conflicts" occurred!
        else
            finalStatus = _("Synchronization completed successfully");
        errorLog.logMsg(finalStatus, TYPE_INFO);
    }

    const SummaryInfo summary =
    {
        jobName_, finalStatus,
        getObjectsCurrent(PHASE_SYNCHRONIZING), getDataCurrent(PHASE_SYNCHRONIZING),
        getObjectsTotal  (PHASE_SYNCHRONIZING), getDataTotal  (PHASE_SYNCHRONIZING),
        (wxGetUTCTimeMillis().GetValue() - startTime_) / 1000
    };

    //----------------- write results into LastSyncs.log------------------------
    try
    {
        saveToLastSyncsLog(summary, errorLog, lastSyncsLogFileSizeMax_, OnUpdateLogfileStatusNoThrow(*this, getLastSyncsLogfilePath())); //throw FileError
    }
    catch (FileError&) { assert(false); }

    if (progressDlg)
    {
        //notify to progressDlg that current process has ended
        if (showFinalResults)
        {
            if (abortIsRequested())
                progressDlg->processHasFinished(SyncProgressDialog::RESULT_ABORTED, errorLog);  //enable okay and close events
            else if (totalErrors > 0)
                progressDlg->processHasFinished(SyncProgressDialog::RESULT_FINISHED_WITH_ERROR, errorLog);
            else if (totalWarnings > 0)
                progressDlg->processHasFinished(SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS, errorLog);
            else
                progressDlg->processHasFinished(SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS, errorLog);
        }
        else
            progressDlg->closeWindowDirectly();

        //wait until progress dialog notified shutdown via onProgressDialogTerminate()
        //-> required since it has our "this" pointer captured in lambda "notifyWindowTerminate"!
        //-> nicely manages dialog lifetime
        while (progressDlg)
        {
            wxTheApp->Yield(); //*first* refresh GUI (removing flicker) before sleeping!
            boost::this_thread::sleep(boost::posix_time::milliseconds(UI_UPDATE_INTERVAL));
        }
    }
}


void SyncStatusHandler::initNewPhase(int objectsTotal, std::int64_t dataTotal, Phase phaseID)
{
    assert(phaseID == PHASE_SYNCHRONIZING);
    StatusHandler::initNewPhase(objectsTotal, dataTotal, phaseID);
    if (progressDlg)
        progressDlg->initNewPhase(); //call after "StatusHandler::initNewPhase"

    forceUiRefresh(); //throw ?; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


void SyncStatusHandler::updateProcessedData(int objectsDelta, std::int64_t dataDelta)
{
    StatusHandler::updateProcessedData(objectsDelta, dataDelta);
    if (progressDlg)
        progressDlg->notifyProgressChange(); //noexcept
    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
}


void SyncStatusHandler::reportInfo(const std::wstring& text)
{
    StatusHandler::reportInfo(text);
    errorLog.logMsg(text, TYPE_INFO);
}


ProcessCallback::Response SyncStatusHandler::reportError(const std::wstring& errorMessage, size_t retryNumber)
{
    //auto-retry
    if (retryNumber < automaticRetryCount_)
    {
        errorLog.logMsg(errorMessage + L"\n-> " +
                        _P("Automatic retry in 1 second...", "Automatic retry in %x seconds...", automaticRetryDelay_), TYPE_INFO);
        //delay
        const int iterations = static_cast<int>(1000 * automaticRetryDelay_ / UI_UPDATE_INTERVAL); //always round down: don't allow for negative remaining time below
        for (int i = 0; i < iterations; ++i)
        {
            reportStatus(_("Error") + L": " + _P("Automatic retry in 1 second...", "Automatic retry in %x seconds...",
                                                 (1000 * automaticRetryDelay_ - i * UI_UPDATE_INTERVAL + 999) / 1000)); //integer round up
            boost::this_thread::sleep(boost::posix_time::milliseconds(UI_UPDATE_INTERVAL));
        }
        return ProcessCallback::RETRY;
    }


    //always, except for "retry":
    zen::ScopeGuard guardWriteLog = zen::makeGuard([&] { errorLog.logMsg(errorMessage, TYPE_ERROR); });

    switch (handleError_)
    {
        case ON_GUIERROR_POPUP:
        {
            if (!progressDlg) abortProcessNow();
            PauseTimers dummy(*progressDlg);
            forceUiRefresh();

            bool ignoreNextErrors = false;
            switch (showConfirmationDialog3(progressDlg->getWindowIfVisible(), DialogInfoType::ERROR2, PopupDialogCfg3().
                                            setDetailInstructions(errorMessage).
                                            setCheckBox(ignoreNextErrors, _("&Ignore subsequent errors"), ConfirmationButton3::DONT_DO_IT),
                                            _("&Ignore"), _("&Retry")))
            {
                case ConfirmationButton3::DO_IT: //ignore
                    if (ignoreNextErrors) //falsify only
                        handleError_ = ON_GUIERROR_IGNORE;
                    return ProcessCallback::IGNORE_ERROR;

                case ConfirmationButton3::DONT_DO_IT: //retry
                    guardWriteLog.dismiss();
                    errorLog.logMsg(errorMessage + L"\n-> " + _("Retrying operation..."), TYPE_INFO); //explain why there are duplicate "doing operation X" info messages in the log!
                    return ProcessCallback::RETRY;

                case ConfirmationButton3::CANCEL:
                    abortProcessNow();
                    break;
            }
        }
        break;

        case ON_GUIERROR_IGNORE:
            return ProcessCallback::IGNORE_ERROR;
    }

    assert(false);
    return ProcessCallback::IGNORE_ERROR; //dummy value
}


void SyncStatusHandler::reportFatalError(const std::wstring& errorMessage)
{
    errorLog.logMsg(errorMessage, TYPE_FATAL_ERROR);

    switch (handleError_)
    {
        case ON_GUIERROR_POPUP:
        {
            if (!progressDlg) abortProcessNow();
            PauseTimers dummy(*progressDlg);
            forceUiRefresh();

            bool ignoreNextErrors = false;
            switch (showConfirmationDialog(progressDlg->getWindowIfVisible(), DialogInfoType::ERROR2,
                                           PopupDialogCfg().setTitle(_("Serious Error")).
                                           setDetailInstructions(errorMessage).
                                           setCheckBox(ignoreNextErrors, _("&Ignore subsequent errors")),
                                           _("&Ignore")))
            {
                case ConfirmationButton::DO_IT:
                    if (ignoreNextErrors) //falsify only
                        handleError_ = ON_GUIERROR_IGNORE;
                    break;
                case ConfirmationButton::CANCEL:
                    abortProcessNow();
                    break;
            }
        }
        break;

        case ON_GUIERROR_IGNORE:
            break;
    }
}


void SyncStatusHandler::reportWarning(const std::wstring& warningMessage, bool& warningActive)
{
    errorLog.logMsg(warningMessage, TYPE_WARNING);

    if (!warningActive)
        return;

    switch (handleError_)
    {
        case ON_GUIERROR_POPUP:
        {
            if (!progressDlg) abortProcessNow();
            PauseTimers dummy(*progressDlg);
            forceUiRefresh();

            bool dontWarnAgain = false;
            switch (showConfirmationDialog(progressDlg->getWindowIfVisible(), DialogInfoType::WARNING,
                                           PopupDialogCfg().setDetailInstructions(warningMessage).
                                           setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                           _("&Ignore")))
            {
                case ConfirmationButton::DO_IT:
                    warningActive = !dontWarnAgain;
                    break;
                case ConfirmationButton::CANCEL:
                    abortProcessNow();
                    break;
            }
        }
        break;

        case ON_GUIERROR_IGNORE:
            break; //if errors are ignored, then warnings should be, too
    }
}


void SyncStatusHandler::forceUiRefresh()
{
    if (progressDlg)
        progressDlg->updateGui();
}


void SyncStatusHandler::abortProcessNow()
{
    requestAbortion(); //just make sure...
    throw GuiAbortProcess();  //abort can be triggered by progressDlg
}


void SyncStatusHandler::onProgressDialogTerminate()
{
    //it's responsibility of "progressDlg" to call requestAbortion() when closing dialog
    progressDlg = nullptr;
}
