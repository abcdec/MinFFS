// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef CUSTOMTOOLTIP_H_INCLUDED
#define CUSTOMTOOLTIP_H_INCLUDED

#include <wx/window.h>

namespace zen
{
class Tooltip
{
public:
    Tooltip(wxWindow& parent) : //parent needs to live at least as long as this instance!
        tipWindow(nullptr), parent_(parent) {}

    void show(const wxString& text,
              wxPoint mousePos, //absolute screen coordinates
              const wxBitmap* bmp = nullptr);
    void hide();

private:
    class TooltipDialogGenerated;
    TooltipDialogGenerated* tipWindow;
    wxWindow& parent_;
};
}

#endif // CUSTOMTOOLTIP_H_INCLUDED
