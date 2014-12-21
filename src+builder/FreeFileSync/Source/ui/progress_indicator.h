// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef PROGRESS_INDICATOR_H_INCLUDED_8037493452348
#define PROGRESS_INDICATOR_H_INCLUDED_8037493452348

#include <functional>
#include <zen/error_log.h>
#include <zen/zstring.h>
#include <wx/frame.h>
#include "../lib/status_handler.h"


class CompareProgressDialog
{
public:
    CompareProgressDialog(wxFrame& parentWindow); //CompareProgressDialog will be owned by parentWindow!

    wxWindow* getAsWindow(); //convenience! don't abuse!

    void init(const zen::Statistics& syncStat); //begin of sync: make visible, set pointer to "syncStat", initialize all status values
    void teardown(); //end of sync: hide again, clear pointer to "syncStat"

    void switchToCompareBytewise();
    void updateStatusPanelNow();

private:
    class Pimpl;
    Pimpl* const pimpl;
};


//SyncStatusHandler will internally process Window messages => disable GUI controls to avoid unexpected callbacks!

struct SyncProgressDialog
{
    enum SyncResult
    {
        RESULT_ABORTED,
        RESULT_FINISHED_WITH_ERROR,
        RESULT_FINISHED_WITH_WARNINGS,
        RESULT_FINISHED_WITH_SUCCESS
    };
    //essential to call one of these two methods in StatusUpdater derived class' destructor at the LATEST(!)
    //to prevent access to callback to updater (e.g. request abort)
    virtual void processHasFinished(SyncResult resultId, const zen::ErrorLog& log) = 0; //sync finished, still dialog may live on
    virtual void closeWindowDirectly() = 0; //don't wait for user

    //---------------------------------------------------------------------------

    virtual wxWindow* getWindowIfVisible() = 0; //may be nullptr; don't abuse, use as parent for modal dialogs only!

    virtual void initNewPhase        () = 0; //call after "StatusHandler::initNewPhase"
    virtual void notifyProgressChange() = 0; //noexcept, required by graph!
    virtual void updateGui           () = 0; //update GUI and process Window messages

    virtual Zstring getExecWhenFinishedCommand() const = 0; //final value (after possible user modification)

    virtual void stopTimer  () = 0; //halt all internal timers!
    virtual void resumeTimer() = 0; //

protected:
    ~SyncProgressDialog() {}
};


SyncProgressDialog* createProgressDialog(zen::AbortCallback& abortCb,
                                         const std::function<void()>& notifyWindowTerminate, //note: user closing window cannot be prevented on OS X! (And neither on Windows during system shutdown!)
                                         const zen::Statistics& syncStat,
                                         wxFrame* parentWindow, //may be nullptr
                                         bool showProgress,
                                         const wxString& jobName,
                                         const Zstring& onCompletion,
                                         std::vector<Zstring>& onCompletionHistory); //changing parameter!
//DON'T delete the pointer! it will be deleted by the user clicking "OK/Cancel"/wxWindow::Destroy() after processHasFinished() or closeWindowDirectly()


class PauseTimers
{
public:
    PauseTimers(SyncProgressDialog& ss) : ss_(ss) { ss_.stopTimer(); }
    ~PauseTimers() { ss_.resumeTimer(); }
private:
    SyncProgressDialog& ss_;
};


#endif //PROGRESS_INDICATOR_H_INCLUDED_8037493452348
