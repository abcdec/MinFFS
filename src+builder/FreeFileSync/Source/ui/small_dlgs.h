// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SMALL_DLGS_H_8321790875018750245
#define SMALL_DLGS_H_8321790875018750245

#include <wx/window.h>
#include "../lib/process_xml.h"
#include "../synchronization.h"


namespace zen
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems

struct ReturnSmallDlg
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_OKAY = 1
    };
};

void showAboutDialog(wxWindow* parent);

ReturnSmallDlg::ButtonPressed showFilterDialog(wxWindow* parent, FilterConfig& filter, const wxString& caption);

ReturnSmallDlg::ButtonPressed showCopyToDialog(wxWindow* parent,
                                               const std::vector<FileSystemObject*>& rowsOnLeft,
                                               const std::vector<FileSystemObject*>& rowsOnRight,
                                               Zstring& lastUsedPath,
                                               std::vector<Zstring>& folderPathHistory,
                                               size_t historySizeMax,
                                               bool& keepRelPaths,
                                               bool& overwriteIfExists);

ReturnSmallDlg::ButtonPressed showDeleteDialog(wxWindow* parent,
                                               const std::vector<FileSystemObject*>& rowsOnLeft,
                                               const std::vector<FileSystemObject*>& rowsOnRight,
                                               bool& useRecycleBin);

ReturnSmallDlg::ButtonPressed showSyncConfirmationDlg(wxWindow* parent,
                                                      const wxString& variantName,
                                                      const SyncStatistics& statistics,
                                                      bool& dontShowAgain);

ReturnSmallDlg::ButtonPressed showCompareCfgDialog(wxWindow* parent, CompConfig& cmpConfig, const wxString& title);

ReturnSmallDlg::ButtonPressed showOptionsDlg(wxWindow* parent, xmlAccess::XmlGlobalSettings& globalSettings);

ReturnSmallDlg::ButtonPressed showSelectTimespanDlg(wxWindow* parent, std::int64_t& timeFrom, std::int64_t& timeTo);

#ifdef ZEN_WIN_VISTA_AND_LATER
    ReturnSmallDlg::ButtonPressed showSftpSetupDialog(wxWindow* parent, Zstring& folderPathPhrase);
#endif
}

#endif //SMALL_DLGS_H_8321790875018750245
