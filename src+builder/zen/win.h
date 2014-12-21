// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef YAWFWH_YET_ANOTHER_WRAPPER_FOR_WINDOWS_H
#define YAWFWH_YET_ANOTHER_WRAPPER_FOR_WINDOWS_H

//------------------------------------------------------
#ifdef __WXMSW__ //we have wxWidgets
    #include <wx/msw/wrapwin.h> //includes "windows.h"
    //------------------------------------------------------
#else
    //#define WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #ifndef STRICT
        #define STRICT //improve type checking
    #endif

    #include <windows.h>
#endif
//------------------------------------------------------

#endif //YAWFWH_YET_ANOTHER_WRAPPER_FOR_WINDOWS_H
