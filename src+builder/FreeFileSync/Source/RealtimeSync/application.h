// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef APPLICATION_H_18506781708176342677
#define APPLICATION_H_18506781708176342677

#include <wx/app.h>


class Application : public wxApp
{
public:
    bool OnInit() override;
    int  OnExit() override;
    int  OnRun () override;
    bool OnExceptionInMainLoop() override { throw; } //just re-throw and avoid display of additional messagebox: it will be caught in OnRun()
    void onQueryEndSession(wxEvent& event);

private:
    void onEnterEventLoop(wxEvent& event);
    //wxLayoutDirection GetLayoutDirection() const override { return wxLayout_LeftToRight; }
};

#endif //APPLICATION_H_18506781708176342677
