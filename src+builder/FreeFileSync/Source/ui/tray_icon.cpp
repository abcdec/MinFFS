// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "tray_icon.h"
#include <zen/basic_math.h>
#include <zen/i18n.h>
#include <wx/taskbar.h>
#include <wx/menu.h>
#include <wx/icon.h> //req. by Linux
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>

using namespace zen;


namespace
{
void fillRange(wxImage& img, int pixelFirst, int pixelLast, const wxColor& col) //tolerant input range
{
    if (img.IsOk())
    {
        const int width  = img.GetWidth ();
        const int height = img.GetHeight();

        if (width > 0 && height > 0)
        {
            pixelFirst = std::max(pixelFirst, 0);
            pixelLast  = std::min(pixelLast, width * height);

            if (pixelFirst < pixelLast)
            {
                unsigned char* const bytesBegin = img.GetData() + pixelFirst * 3;
                unsigned char* const bytesEnd   = img.GetData() + pixelLast  * 3;

                for (unsigned char* bytePos = bytesBegin; bytePos < bytesEnd; bytePos += 3)
                {
                    bytePos[0] = col.Red  ();
                    bytePos[1] = col.Green();
                    bytePos[2] = col.Blue ();
                }

                if (img.HasAlpha()) //make progress indicator fully opaque:
                    std::fill(img.GetAlpha() + pixelFirst, img.GetAlpha() + pixelLast, wxIMAGE_ALPHA_OPAQUE);
            }
        }
    }
}


wxIcon generateProgressIcon(const wxImage& logo, double fraction) //generate icon with progress indicator
{
    if (!logo.IsOk() || logo.GetWidth() <= 0 || logo.GetHeight() <= 0)
        return wxIcon();

    const int pixelCount = logo.GetWidth() * logo.GetHeight();
    const int startFillPixel = numeric::clampCpy(numeric::round(fraction * pixelCount), 0, pixelCount);

    //minor optimization
    static std::pair<int, wxIcon> buffer = std::make_pair(-1, wxNullIcon);

    if (buffer.first != startFillPixel)
    {
        wxImage genImage(logo.Copy()); //workaround wxWidgets' screwed-up design from hell: their copy-construction implements reference-counting WITHOUT copy-on-write!

        //gradually make FFS icon brighter while nearing completion
        zen::brighten(genImage, -200 * (1 - fraction));

        //fill black border row
        if (startFillPixel <= pixelCount - genImage.GetWidth())
        {
            /*
                    --------
                    ---bbbbb
                    bbbbSyyy  S : start yellow remainder
                    yyyyyyyy
            */
            int bStart = startFillPixel - genImage.GetWidth();
            if (bStart % genImage.GetWidth() != 0) //add one more black pixel, see ascii-art
                --bStart;
            fillRange(genImage, bStart, startFillPixel, *wxBLACK);
        }
        else if (startFillPixel < pixelCount)
        {
            //special handling for last row
            /*
                    --------
                    --------
                    ---bbbbb
                    ---bSyyy  S : start yellow remainder
            */
            int bStart = startFillPixel - genImage.GetWidth() - 1;
            int bEnd = (bStart / genImage.GetWidth() + 1) * genImage.GetWidth();

            fillRange(genImage, bStart, bEnd, *wxBLACK);
            fillRange(genImage, startFillPixel - 1, startFillPixel, *wxBLACK);
        }

        //fill yellow remainder
        fillRange(genImage, startFillPixel, pixelCount, wxColour(240, 200, 0));

        buffer.second.CopyFromBitmap(wxBitmap(genImage));
    }

    return buffer.second;
}

//------------------------------------------------------------------------------------------------

enum Selection
{
    CONTEXT_RESTORE = 1 //wxWidgets: "A MenuItem ID of zero does not work under Mac"
};
}


class FfsTrayIcon::TaskBarImpl : public wxTaskBarIcon
{
public:
    TaskBarImpl(const std::function<void()>& onRequestResume) : onRequestResume_(onRequestResume)
    {
        Connect(wxEVT_TASKBAR_LEFT_DCLICK, wxEventHandler(TaskBarImpl::OnDoubleClick), nullptr, this);

        //Windows User Experience Guidelines: show the context menu rather than doing *nothing* on single left clicks; however:
        //MSDN: "Double-clicking the left mouse button actually generates a sequence of four messages: WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, and WM_LBUTTONUP."
        //Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/ms645606%28v=vs.85%29.aspx
        //=> the only way to distinguish single left click and double-click is to wait wxSystemSettings::GetMetric(wxSYS_DCLICK_MSEC) (480ms) which is way too long!
    }

    void dontCallbackAnymore() { onRequestResume_ = nullptr; }

private:
    wxMenu* CreatePopupMenu() override
    {
        if (!onRequestResume_)
            return nullptr;

        wxMenu* contextMenu = new wxMenu;

        wxMenuItem* defaultItem = new wxMenuItem(contextMenu, CONTEXT_RESTORE, _("&Restore"));
        //wxWidgets font mess-up:
        //1. font must be set *before* wxMenu::Append()!
        //2. don't use defaultItem->GetFont(); making it bold creates a huge font size for some reason
#ifdef ZEN_WIN //no wxMenuItem::SetFont() on Linux and OS X: wasn't wxWidgets supposed to be *portable* at some point in time?????
        defaultItem->SetFont(wxNORMAL_FONT->Bold()); //make default selection bold/align with double-click
#endif
        contextMenu->Append(defaultItem);

        //event handling
        contextMenu->Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TaskBarImpl::OnContextMenuSelection), nullptr, this);

        return contextMenu; //ownership transferred to caller
    }

    void OnContextMenuSelection(wxCommandEvent& event)
    {
        switch (static_cast<Selection>(event.GetId()))
        {
            case CONTEXT_RESTORE:
                if (onRequestResume_)
                    onRequestResume_();
                break;
        }
    }

    void OnDoubleClick(wxEvent& event)
    {
        if (onRequestResume_)
            onRequestResume_();
    }

    //void OnLeftDownClick(wxEvent& event)
    //{
    //	//copied from wxTaskBarIconBase::OnRightButtonDown()
    //   if (wxMenu* menu = CreatePopupMenu())
    //   {
    //       PopupMenu(menu);
    //       delete menu;
    //   }
    //}

    std::function<void()> onRequestResume_;
};


FfsTrayIcon::FfsTrayIcon(const std::function<void()>& onRequestResume) :
    trayIcon(new TaskBarImpl(onRequestResume)),
    activeFraction(1), //show FFS logo by default
#if defined ZEN_WIN || defined ZEN_MAC //16x16 seems to be the only size that is shown correctly on OS X
    logo(getResourceImage(L"FFS_tray_16x16").ConvertToImage())
#elif defined ZEN_LINUX
    logo(getResourceImage(L"FFS_tray_24x24").ConvertToImage())
#endif
{
    trayIcon->SetIcon(generateProgressIcon(logo, activeFraction), L"FreeFileSync");
}


FfsTrayIcon::~FfsTrayIcon()
{
    trayIcon->dontCallbackAnymore(); //TaskBarImpl has longer lifetime than FfsTrayIcon: avoid callback!

    /*
    This is not working correctly on OS X! It seems both wxTaskBarIcon::RemoveIcon() and ~wxTaskBarIcon() are broken and do NOT immediately
    remove the icon from the system tray! Only some time later in the event loop which called these functions they will be removed.
    Maybe some system component has still shared ownership? Objective C auto release pools are freed at the end of the current event loop...
    Anyway, wxWidgets fails to disconnect the wxTaskBarIcon event handlers before calling "[m_statusitem release]"!

    => !!!clicking on the icon after ~wxTaskBarIcon ran crashes the application!!!

    - if ~wxTaskBarIcon() ran from the SyncProgressDialog::updateGui() event loop (e.g. user manually clicking the icon) => icon removed on return
    - if ~wxTaskBarIcon() ran from SyncProgressDialog::closeWindowDirectly() => leaves the icon dangling until user closes this dialog and outter event loop runs!
    */

    trayIcon->RemoveIcon(); //required on Windows: unlike on OS X, wxPendingDelete does not kick in before main event loop!
    //use wxWidgets delayed destruction: delete during next idle loop iteration (handle late window messages, e.g. when double-clicking)
    wxPendingDelete.Append(trayIcon); //identical to wxTaskBarIconBase::Destroy() in wxWidgets 2.9.5
}


void FfsTrayIcon::setToolTip(const wxString& toolTip)
{
    activeToolTip = toolTip;
    trayIcon->SetIcon(generateProgressIcon(logo, activeFraction), activeToolTip); //another wxWidgets design bug: non-orthogonal method!
}


void FfsTrayIcon::setProgress(double fraction)
{
    activeFraction = fraction;
    trayIcon->SetIcon(generateProgressIcon(logo, activeFraction), activeToolTip);
}
