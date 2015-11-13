// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef APPLICATION_H_081568741942010985702395
#define APPLICATION_H_081568741942010985702395

#include <vector>
#include <zen/zstring.h>
#include <wx/app.h>
#include "lib/return_codes.h"


class Application : public wxApp
{
public:
    Application() : returnCode(zen::FFS_RC_SUCCESS) {}

private:
    bool OnInit() override;
    int  OnExit() override;
    int  OnRun() override;
    bool OnExceptionInMainLoop() override { throw; } //just re-throw and avoid display of additional messagebox: it will be caught in OnRun()

    void onEnterEventLoop(wxEvent& event);
    void onQueryEndSession(wxEvent& event);
    void launch(const std::vector<Zstring>& commandArgs);

    zen::FfsReturnCode returnCode;
};

#endif //APPLICATION_H_081568741942010985702395
