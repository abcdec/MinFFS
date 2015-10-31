// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef RTS_MAIN_DLG_H_2384790842252445
#define RTS_MAIN_DLG_H_2384790842252445

#include "gui_generated.h"
#include <vector>
#include <memory>
#include <zen/zstring.h>
#include <wx+/async_task.h>
#include <wx+/file_drop.h>
#include <wx/timer.h>
#include "folder_selector2.h"

namespace xmlAccess
{
struct XmlRealConfig;
}
class DirectoryPanel;


class MainDialog: public MainDlgGenerated
{
public:
    static void create(const Zstring& cfgFile);

    void onQueryEndSession(); //last chance to do something useful before killing the application!

private:
    MainDialog(wxDialog* dlg, const Zstring& cfgFileName);
    ~MainDialog();

    void loadConfig(const Zstring& filepath);

    void OnClose          (wxCloseEvent&  event ) override  { Destroy(); }
    void OnShowHelp       (wxCommandEvent& event) override;
    void OnMenuAbout      (wxCommandEvent& event) override;
    void OnAddFolder      (wxCommandEvent& event) override;
    void OnRemoveFolder   (wxCommandEvent& event);
    void OnRemoveTopFolder(wxCommandEvent& event) override;
    void OnKeyPressed     (wxKeyEvent&     event);
    void OnStart          (wxCommandEvent& event) override;
    void OnConfigSave     (wxCommandEvent& event) override;
    void OnConfigLoad     (wxCommandEvent& event) override;
    void OnMenuQuit       (wxCommandEvent& event) override { Close(); }
    void onFilesDropped(zen::FileDropEvent& event);

    void setConfiguration(const xmlAccess::XmlRealConfig& cfg);
    xmlAccess::XmlRealConfig getConfiguration();
    void setLastUsedConfig(const Zstring& filepath);

    void addFolder(const std::vector<Zstring>& newFolders, bool addFront = false);
    void removeAddFolder(size_t pos);
    void clearAddFolders();

    static const Zstring& lastConfigFileName();

    std::unique_ptr<zen::FolderSelector2> dirpathFirst;
    std::vector<DirectoryPanel*> dirpathsExtra; //additional pairs to the standard pair

    Zstring currentConfigFileName;

    zen::AsyncGuiQueue guiQueue; //schedule and run long-running tasks asynchronously, but process results on GUI queue
};

#endif //RTS_MAIN_DLG_H_2384790842252445
