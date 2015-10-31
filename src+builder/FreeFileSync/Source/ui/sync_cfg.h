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

struct LocalPairConfig
{
    std::wstring folderPairName; //read-only!
    std::shared_ptr<const CompConfig> altCmpConfig;  //optional
    std::shared_ptr<const SyncConfig> altSyncConfig; //
    FilterConfig localFilter;
};


struct MiscSyncConfig
{
    xmlAccess::OnGuiError handleError;
    Zstring onCompletionCommand;
    std::vector<Zstring> onCompletionHistory;
};


struct GlobalSyncConfig
{
    CompConfig   cmpConfig;
    SyncConfig   syncCfg;
    FilterConfig filter;
    MiscSyncConfig miscCfg;
};


ReturnSyncConfig::ButtonPressed showSyncConfigDlg(wxWindow* parent,
                                                  SyncConfigPanel panelToShow,
                                                  int localPairIndexToShow, //< 0 to show global config

                                                  std::vector<LocalPairConfig>& folderPairConfig,

                                                  CompConfig&   globalCmpConfig,
                                                  SyncConfig&   globalSyncCfg,
                                                  FilterConfig& globalFilter,

                                                  xmlAccess::OnGuiError& handleError,
                                                  Zstring& onCompletionCommand,
                                                  std::vector<Zstring>& onCompletionHistory,

                                                  size_t onCompletionHistoryMax);
}

#endif //SYNCCONFIG_H_INCLUDED_31289470134253425
