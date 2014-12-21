// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef DC_3813704987123956832143243214
#define DC_3813704987123956832143243214

#include <unordered_map>
#include <wx/dcbuffer.h> //for macro: wxALWAYS_NATIVE_DOUBLE_BUFFER

namespace zen
{
/*
1. wxDCClipper does *not* stack: another fix for yet another poor wxWidgets implementation

class RecursiveDcClipper
{
    RecursiveDcClipper(wxDC& dc, const wxRect& r) : dc_(dc)
};

------------------------------------------------------------------------------------------------

2. wxAutoBufferedPaintDC skips one pixel on left side when RTL layout is active: a fix for a poor wxWidgets implementation
class BufferedPaintDC
{
    BufferedPaintDC(wxWindow& wnd, std::unique_ptr<wxBitmap>& buffer);
};
*/











//---------------------- implementation ------------------------
class RecursiveDcClipper
{
public:
    RecursiveDcClipper(wxDC& dc, const wxRect& r) : dc_(dc)
    {
        auto it = refDcToAreaMap().find(&dc);
        if (it != refDcToAreaMap().end())
        {
            oldRect = zen::make_unique<wxRect>(it->second);

            wxRect tmp = r;
            tmp.Intersect(*oldRect);    //better safe than sorry
            dc_.SetClippingRegion(tmp); //
            it->second = tmp;
        }
        else
        {
            dc_.SetClippingRegion(r);
            refDcToAreaMap().emplace(&dc_, r);
        }
    }

    ~RecursiveDcClipper()
    {
        dc_.DestroyClippingRegion();
        if (oldRect.get() != nullptr)
        {
            dc_.SetClippingRegion(*oldRect);
            refDcToAreaMap()[&dc_] = *oldRect;
        }
        else
            refDcToAreaMap().erase(&dc_);
    }

private:
    //associate "active" clipping area with each DC
    static std::unordered_map<wxDC*, wxRect>& refDcToAreaMap() { static std::unordered_map<wxDC*, wxRect> clippingAreas; return clippingAreas; }

    std::unique_ptr<wxRect> oldRect;
    wxDC& dc_;
};


#ifndef wxALWAYS_NATIVE_DOUBLE_BUFFER
    #error we need this one!
#endif

#if wxALWAYS_NATIVE_DOUBLE_BUFFER
struct BufferedPaintDC : public wxPaintDC { BufferedPaintDC(wxWindow& wnd, std::unique_ptr<wxBitmap>& buffer) : wxPaintDC(&wnd) {} };

#else
class BufferedPaintDC : public wxMemoryDC
{
public:
    BufferedPaintDC(wxWindow& wnd, std::unique_ptr<wxBitmap>& buffer) : buffer_(buffer), paintDc(&wnd)
    {
        const wxSize clientSize = wnd.GetClientSize();
        if (!buffer_ || clientSize != wxSize(buffer->GetWidth(), buffer->GetHeight()))
            buffer = zen::make_unique<wxBitmap>(clientSize.GetWidth(), clientSize.GetHeight());

        SelectObject(*buffer);

        if (paintDc.IsOk() && paintDc.GetLayoutDirection() == wxLayout_RightToLeft)
            SetLayoutDirection(wxLayout_RightToLeft);
    }

    ~BufferedPaintDC()
    {
        if (GetLayoutDirection() == wxLayout_RightToLeft)
        {
            paintDc.SetLayoutDirection(wxLayout_LeftToRight); //workaround bug in wxDC::Blit()
            SetLayoutDirection(wxLayout_LeftToRight);         //
        }

        const wxPoint origin = GetDeviceOrigin();
        paintDc.Blit(0, 0, buffer_->GetWidth(), buffer_->GetHeight(), this, -origin.x, -origin.y);
    }

private:
    std::unique_ptr<wxBitmap>& buffer_;
    wxPaintDC paintDc;
};
#endif
}

#endif //DC_3813704987123956832143243214
