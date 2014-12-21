// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FONT_SIZE_HEADER_23849632846734343234532
#define FONT_SIZE_HEADER_23849632846734343234532

#include <zen/basic_math.h>
#include <wx/window.h>
#ifdef ZEN_WIN
    #include <zen/dll.h>
    #include <Uxtheme.h>
    #include <vsstyle.h> //TEXT_MAININSTRUCTION
    #include <vssym32.h> //TMT_COLOR
#endif

namespace zen
{
//set portable font size in multiples of the operating system's default font size
void setRelativeFontSize(wxWindow& control, double factor);
void setMainInstructionFont(wxWindow& control); //following Windows/Gnome/OS X guidelines












//###################### implementation #####################
inline
void setRelativeFontSize(wxWindow& control, double factor)
{
    wxFont font = control.GetFont();
    font.SetPointSize(numeric::round(wxNORMAL_FONT->GetPointSize() * factor));
    control.SetFont(font);
};


inline
void setMainInstructionFont(wxWindow& control)
{
    wxFont font = control.GetFont();
#ifdef ZEN_WIN //http://msdn.microsoft.com/de-DE/library/windows/desktop/aa974176#fonts
    font.SetPointSize(wxNORMAL_FONT->GetPointSize() * 4 / 3); //integer round down

    //get main instruction color: don't hard-code, respect accessibility!
    typedef HTHEME  (WINAPI* OpenThemeDataFun )(HWND hwnd, LPCWSTR pszClassList);
    typedef HRESULT (WINAPI* CloseThemeDataFun)(HTHEME hTheme);
    typedef HRESULT (WINAPI* GetThemeColorFun )(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

    const SysDllFun<OpenThemeDataFun>  openThemeData (L"UxTheme.dll", "OpenThemeData"); //available with Windows XP and later
    const SysDllFun<CloseThemeDataFun> closeThemeData(L"UxTheme.dll", "CloseThemeData");
    const SysDllFun<GetThemeColorFun>  getThemeColor (L"UxTheme.dll", "GetThemeColor");
    if (openThemeData && closeThemeData && getThemeColor)
        if (HTHEME hTheme = openThemeData(NULL,          //__in  HWND hwnd,
                                          L"TEXTSTYLE")) //__in  LPCWSTR pszClassList
        {
            ZEN_ON_SCOPE_EXIT(closeThemeData(hTheme));

            COLORREF cr = {};
            if (getThemeColor(hTheme,               //_In_   HTHEME hTheme,
                              TEXT_MAININSTRUCTION, //  _In_   int iPartId,
                              0,                    //  _In_   int iStateId,
                              TMT_TEXTCOLOR,        //  _In_   int iPropId,
                              &cr) == S_OK)         //  _Out_  COLORREF *pColor
                control.SetForegroundColour(wxColour(cr));
        }

#elif defined ZEN_LINUX //https://developer.gnome.org/hig-book/3.2/hig-book.html#alert-text
    font.SetPointSize(numeric::round(wxNORMAL_FONT->GetPointSize() * 12.0 / 11));
    font.SetWeight(wxFONTWEIGHT_BOLD);

#elif defined ZEN_MAC //https://developer.apple.com/library/mac/documentation/UserExperience/Conceptual/AppleHIGuidelines/Windows/Windows.html#//apple_ref/doc/uid/20000961-TP10
    font.SetWeight(wxFONTWEIGHT_BOLD);
#endif
    control.SetFont(font);
};
}

#endif //FONT_SIZE_HEADER_23849632846734343234532
