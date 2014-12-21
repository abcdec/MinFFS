// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef TRAY_583967857420987534253245
#define TRAY_583967857420987534253245

#include <wx/string.h>
#include "xml_proc.h"

namespace rts
{
enum AbortReason
{
    SHOW_GUI,
    EXIT_APP
};
AbortReason startDirectoryMonitor(const xmlAccess::XmlRealConfig& config, const wxString& jobname); //jobname may be empty
}

#endif //TRAY_583967857420987534253245
