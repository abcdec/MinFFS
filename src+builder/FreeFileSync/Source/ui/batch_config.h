// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef BATCH_CONFIG_H_3921674832168945
#define BATCH_CONFIG_H_3921674832168945

#include <wx/window.h>
#include "../lib/process_xml.h"


namespace zen
{
struct ReturnBatchConfig
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_SAVE_AS
    };
};


//show and let user customize batch settings (without saving)
ReturnBatchConfig::ButtonPressed customizeBatchConfig(wxWindow* parent,
                                                      xmlAccess::XmlBatchConfig& batchCfg, //in/out
                                                      std::vector<Zstring>& onCompletionHistory,
                                                      size_t onCompletionHistoryMax);
}

#endif //BATCH_CONFIG_H_3921674832168945
