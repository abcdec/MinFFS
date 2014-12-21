// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SYNCCONFIG_H_INCLUDED_31289470134253425
#define SYNCCONFIG_H_INCLUDED_31289470134253425

#include <wx/window.h>
#include "../lib/process_xml.h"


namespace zen
{
struct ReturnSyncConfig
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_OKAY
    };
};

enum class SyncConfigPanel
{
    COMPARISON = 0, //
    FILTER     = 1, //used as zero-based notebook page index!
    SYNC       = 2, //
};

struct MiscGlobalCfg
{
    xmlAccess::OnGuiError& handleError; //in/out param

    Zstring& onCompletionCommand;       //
    std::vector<Zstring>& onCompletionHistory;
    size_t onCompletionHistoryMax;
};


ReturnSyncConfig::ButtonPressed showSyncConfigDlg(wxWindow* parent,
                                                  SyncConfigPanel panelToShow,
                                                  bool* useAlternateCmpCfg,  //optional parameter
                                                  CompConfig&   cmpCfg,
                                                  FilterConfig& filterCfg,
                                                  bool* useAlternateSyncCfg,  //
                                                  SyncConfig&   syncCfg,
                                                  CompareVariant globalCmpVar,
                                                  MiscGlobalCfg* miscCfg, //
                                                  const wxString& title);
}

#endif //SYNCCONFIG_H_INCLUDED_31289470134253425
