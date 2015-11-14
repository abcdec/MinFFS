// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef WIN_H_8701570183560183247891346363457
#define WIN_H_8701570183560183247891346363457

#ifndef _WINSOCKAPI_ //prevent inclusion of winsock.h in windows.h: obsoleted by and conflicting with winsock2.h
    #define _WINSOCKAPI_
#endif

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

#endif //WIN_H_8701570183560183247891346363457
