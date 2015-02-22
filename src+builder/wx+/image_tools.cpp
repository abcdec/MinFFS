// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "image_tools.h"
#include <zen/string_tools.h>
#include <wx/app.h>


using namespace zen;

namespace
{
void writeToImage(const wxImage& source, wxImage& target, const wxPoint& pos)
{
    const int srcWidth  = source.GetWidth ();
    const int srcHeight = source.GetHeight();
    const int trgWidth  = target.GetWidth ();

    if (srcWidth > 0 && srcHeight > 0)
    {
        assert(0 <= pos.x && pos.x + srcWidth  <= trgWidth          ); //draw area must be a
        assert(0 <= pos.y && pos.y + srcHeight <= target.GetHeight()); //subset of target image!
        assert(target.HasAlpha());

        {
            const unsigned char* sourcePtr = source.GetData();
            unsigned char*       targetPtr = target.GetData() + 3 * (pos.x + pos.y * trgWidth);

            for (int row = 0; row < srcHeight; ++row)
                ::memcpy(targetPtr + 3 * row * trgWidth, sourcePtr + 3 * row * srcWidth, 3 * srcWidth);
        }

        //handle alpha channel
        {
            unsigned char* targetPtr = target.GetAlpha() + pos.x + pos.y * trgWidth;
            if (source.HasAlpha())
            {
                const unsigned char* sourcePtr = source.GetAlpha();
                for (int row = 0; row < srcHeight; ++row)
                    ::memcpy(targetPtr + row * trgWidth, sourcePtr + row * srcWidth, srcWidth);
            }
            else
                for (int row = 0; row < srcHeight; ++row)
                    ::memset(targetPtr + row * trgWidth, wxIMAGE_ALPHA_OPAQUE, srcWidth);
        }
    }
}
}


wxImage zen::stackImages(const wxImage& img1, const wxImage& img2, ImageStackLayout dir, ImageStackAlignment align, int gap)
{
    assert(gap >= 0);
    gap = std::max(0, gap);

    const int img1Width  = img1.GetWidth ();
    const int img1Height = img1.GetHeight();
    const int img2Width  = img2.GetWidth ();
    const int img2Height = img2.GetHeight();

    int width  = std::max(img1Width,  img2Width);
    int height = std::max(img1Height, img2Height);
    switch (dir)
    {
        case ImageStackLayout::HORIZONTAL:
            width  = img1Width + gap + img2Width;
            break;

        case ImageStackLayout::VERTICAL:
            height = img1Height + gap + img2Height;
            break;
    }
    wxImage output(width, height);
    output.SetAlpha();
    ::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, width * height);
    ::memset(output.GetData (), 0, 3 * width * height); //redundant due to transparent alpha

    auto calcPos = [&](int imageExtent, int totalExtent)
    {
        switch (align)
        {
            case ImageStackAlignment::CENTER:
                return (totalExtent - imageExtent) / 2;
            case ImageStackAlignment::LEFT:
                return 0;
            case ImageStackAlignment::RIGHT:
                return totalExtent - imageExtent;
        }
        assert(false);
        return 0;
    };

    switch (dir)
    {
        case ImageStackLayout::HORIZONTAL:
            writeToImage(img1, output, wxPoint(0,               calcPos(img1Height, height)));
            writeToImage(img2, output, wxPoint(img1Width + gap, calcPos(img2Height, height)));
            break;

        case ImageStackLayout::VERTICAL:
            writeToImage(img1, output, wxPoint(calcPos(img1Width, width), 0));
            writeToImage(img2, output, wxPoint(calcPos(img2Width, width), img1Height + gap));
            break;
    }
    return output;
}


namespace
{
void calcAlphaForBlackWhiteImage(wxImage& image) //assume black text on white background
{
    assert(image.HasAlpha());
    if (unsigned char* alphaPtr = image.GetAlpha())
    {
        const int pixelCount = image.GetWidth() * image.GetHeight();
        const unsigned char* dataPtr = image.GetData();
        for (int i = 0; i < pixelCount; ++ i)
        {
            const unsigned char r = *dataPtr++;
            const unsigned char g = *dataPtr++;
            const unsigned char b = *dataPtr++;

            //black(0,0,0) becomes fully opaque(255), while white(255,255,255) becomes transparent(0)
            alphaPtr[i] = static_cast<unsigned char>((255 - r + 255 - g + 255 - b) / 3); //mixed mode arithmetics!
        }
    }
}


wxSize getTextExtent(const wxString& text, const wxFont& font)
{
    wxMemoryDC dc; //the context used for bitmaps
    dc.SetFont(font); //the font parameter of GetMultiLineTextExtent() is not evalated on OS X, wxWidgets 2.9.5, so apply it to the DC directly!
    return dc.GetMultiLineTextExtent(replaceCpy(text, L"&", L"", false)); //remove accelerator
}
}

wxImage zen::createImageFromText(const wxString& text, const wxFont& font, const wxColor& col)
{
    //wxDC::DrawLabel() doesn't respect alpha channel => calculate alpha values manually:

    if (text.empty())
        return wxImage();

    wxBitmap newBitmap(getTextExtent(text, font)); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    {
        wxMemoryDC dc(newBitmap);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        dc.SetTextForeground(*wxBLACK); //for use in calcAlphaForBlackWhiteImage
        dc.SetTextBackground(*wxWHITE); //
        dc.SetFont(font);

        //assert(!contains(text, L"&")); //accelerator keys not supported here; see also getTextExtent()
        wxString textFmt = replaceCpy(text, L"&", L"", false);

        //for some reason wxDC::DrawText messes up "weak" bidi characters even when wxLayout_RightToLeft is set! (--> arrows in hebrew/arabic)
        //=> use mark characters instead:
        const wchar_t rtlMark = L'\u200F';
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
            textFmt = rtlMark + textFmt + rtlMark;

        dc.DrawText(textFmt, wxPoint());
    }

    wxImage output(newBitmap.ConvertToImage());
    output.SetAlpha();

    //calculate alpha channel
    calcAlphaForBlackWhiteImage(output);

    //apply actual text color
    unsigned char* dataPtr = output.GetData();
    const int pixelCount = output.GetWidth() * output.GetHeight();
    for (int i = 0; i < pixelCount; ++ i)
    {
        *dataPtr++ = col.Red();
        *dataPtr++ = col.Green();
        *dataPtr++ = col.Blue();
    }
    return output;
}
