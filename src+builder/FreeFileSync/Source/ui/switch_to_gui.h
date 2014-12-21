// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SWITCHTOGUI_H_132047815734845
#define SWITCHTOGUI_H_132047815734845

#include "../lib/process_xml.h"
#include "main_dlg.h" //in "application.cpp" we have this dependency anyway!

namespace zen
{
//switch from FreeFileSync Batch to GUI modus: opens a new FreeFileSync GUI session asynchronously
class SwitchToGui
{
public:
    SwitchToGui(const Zstring& globalConfigFile,
                xmlAccess::XmlGlobalSettings& globalSettings,
                const Zstring& referenceFile,
                const xmlAccess::XmlBatchConfig& batchCfg) :
        globalConfigFile_(globalConfigFile),
        globalSettings_(globalSettings),
        guiCfg(xmlAccess::convertBatchToGui(batchCfg))
    {
        referenceFiles.push_back(referenceFile);
    }

    void execute() const
    {
        MainDialog::create(globalConfigFile_, &globalSettings_, guiCfg, referenceFiles, /*bool startComparison = */ true); //new toplevel window
    }

private:
    const Zstring globalConfigFile_;
    xmlAccess::XmlGlobalSettings& globalSettings_;

    std::vector<Zstring> referenceFiles;
    const xmlAccess::XmlGuiConfig guiCfg;
};
}

#endif //SWITCHTOGUI_H_132047815734845
