// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef IMAGE_TOOLS_HEADER_45782456427634254
#define IMAGE_TOOLS_HEADER_45782456427634254

#include <numeric>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <zen/basic_math.h>

namespace zen
{
enum class ImageStackLayout
{
    HORIZONTAL,
    VERTICAL
};

enum class ImageStackAlignment
{
    CENTER,
    LEFT,
    RIGHT,
    TOP = LEFT,
    BOTTOM = RIGHT,
};
wxImage stackImages(const wxImage& img1, const wxImage& img2, ImageStackLayout dir, ImageStackAlignment align, int gap = 0);

wxImage createImageFromText(const wxString& text, const wxFont& font, const wxColor& col);


wxImage greyScale(const wxImage& img); //greyscale + brightness adaption
wxBitmap greyScale(const wxBitmap& bmp); //
wxBitmap layOver(const wxBitmap& foreground, const wxBitmap& background); //merge

//void moveImage(wxImage& img, int right, int up);
void adjustBrightness(wxImage& img, int targetLevel);
double getAvgBrightness(const wxImage& img); //in [0, 255]
void brighten(wxImage& img, int level); //level: delta per channel in points

bool isEqual(const wxBitmap& lhs, const wxBitmap& rhs); //pixel-wise equality (respecting alpha channel)

//wxColor gradient(const wxColor& from, const wxColor& to, double fraction); //maps fraction within [0, 1] to an intermediate color

//wxColour hsvColor(double h, double s, double v); //h within [0, 360), s, v within [0, 1]














//################################### implementation ###################################
/*
inline
void moveImage(wxImage& img, int right, int up)
{
    img = img.GetSubImage(wxRect(std::max(0, -right), std::max(0, up), img.GetWidth() - abs(right), img.GetHeight() - abs(up)));
    img.Resize(wxSize(img.GetWidth() + abs(right), img.GetHeight() + abs(up)), wxPoint(std::max(0, right), std::max(0, -up)));
}
*/


inline
wxImage greyScale(const wxImage& img)
{
    wxImage output = img.ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3); //treat all channels equally!
    //wxImage output = bmp.ConvertToImage().ConvertToGreyscale();
    adjustBrightness(output, 160);
    return output;
}


inline
wxBitmap greyScale(const wxBitmap& bmp)
{
    assert(!bmp.GetMask()); //wxWidgets screws up for the gazillionth time applying a mask instead of alpha channel if the .png image has only 0 and 0xff opacity values!!!
    return greyScale(bmp.ConvertToImage());
}


inline
double getAvgBrightness(const wxImage& img)
{
    const int pixelCount = img.GetWidth() * img.GetHeight();
    auto pixBegin = img.GetData();

    if (pixelCount > 0 && pixBegin)
    {
        auto pixEnd = pixBegin + 3 * pixelCount; //RGB

        if (img.HasAlpha())
        {
            const unsigned char* alphaFirst = img.GetAlpha();

            //calculate average weighted by alpha channel
            double dividend = 0;
            for (auto iter = pixBegin; iter != pixEnd; ++iter)
                dividend += *iter * static_cast<double>(alphaFirst[(iter - pixBegin) / 3]);

            const double divisor = 3.0 * std::accumulate(alphaFirst, alphaFirst + pixelCount, 0.0);

            return numeric::isNull(divisor) ? 0 : dividend / divisor;
        }
        else
            return std::accumulate(pixBegin, pixEnd, 0.0) / (3.0 * pixelCount);
    }
    return 0;
}


inline
void brighten(wxImage& img, int level)
{
    const int pixelCount = img.GetWidth() * img.GetHeight();
    auto pixBegin = img.GetData();
    if (pixBegin)
    {
        auto pixEnd = pixBegin + 3 * pixelCount; //RGB
        if (level > 0)
            std::for_each(pixBegin, pixEnd, [&](unsigned char& c) { c = std::min(255, c + level); });
        else
            std::for_each(pixBegin, pixEnd, [&](unsigned char& c) { c = std::max(0, c + level); });
    }
}


inline
void adjustBrightness(wxImage& img, int targetLevel)
{
    brighten(img, targetLevel - getAvgBrightness(img));
}


inline
wxBitmap layOver(const wxBitmap& foreground, const wxBitmap& background)
{
    wxBitmap output = background;
    {
        wxMemoryDC dc;
        dc.SelectObject(output);

        const int offsetX = (background.GetWidth () - foreground.GetWidth ()) / 2;
        const int offsetY = (background.GetHeight() - foreground.GetHeight()) / 2;
        dc.DrawBitmap(foreground, offsetX, offsetY, true);
        dc.SelectObject(wxNullBitmap);
    }
    return output;
}


inline
bool isEqual(const wxBitmap& lhs, const wxBitmap& rhs)
{
    if (lhs.IsOk() != rhs.IsOk())
        return false;
    if (!lhs.IsOk())
        return true;

    const int pixelCount = lhs.GetWidth() * lhs.GetHeight();
    if (pixelCount != rhs.GetWidth() * rhs.GetHeight())
        return false;

    wxImage imLhs = lhs.ConvertToImage();
    wxImage imRhs = rhs.ConvertToImage();

    if (imLhs.HasAlpha() != imRhs.HasAlpha())
        return false;

    if (imLhs.HasAlpha())
    {
        if (!std::equal(imLhs.GetAlpha(), imLhs.GetAlpha() + pixelCount, imRhs.GetAlpha()))
            return false;
    }

    return std::equal(imLhs.GetData(), imLhs.GetData() + pixelCount * 3, imRhs.GetData());
}

/*
inline
wxColor gradient(const wxColor& from, const wxColor& to, double fraction)
{
    fraction = std::max(0.0, fraction);
    fraction = std::min(1.0, fraction);
    return wxColor(from.Red  () + (to.Red  () - from.Red  ()) * fraction,
                   from.Green() + (to.Green() - from.Green()) * fraction,
                   from.Blue () + (to.Blue () - from.Blue ()) * fraction,
                   from.Alpha() + (to.Alpha() - from.Alpha()) * fraction);
}
*/

/*
inline
wxColour hsvColor(double h, double s, double v) //h within [0, 360), s, v within [0, 1]
{
    //http://de.wikipedia.org/wiki/HSV-Farbraum

    //make input values fit into bounds
    if (h > 360)
        h -= static_cast<int>(h / 360) * 360;
    else if (h < 0)
        h -= static_cast<int>(h / 360) * 360 - 360;
    numeric::confine<double>(s, 0, 1);
    numeric::confine<double>(v, 0, 1);
    //------------------------------------
    const int h_i = h / 60;
    const float f = h / 60 - h_i;

    auto polish = [](double val) -> unsigned char
    {
        int result = numeric::round(val * 255);
        numeric::confine(result, 0, 255);
        return static_cast<unsigned char>(result);
    };

    const unsigned char p  = polish(v * (1 - s));
    const unsigned char q  = polish(v * (1 - s * f));
    const unsigned char t  = polish(v * (1 - s * (1 - f)));
    const unsigned char vi = polish(v);

    switch (h_i)
    {
        case 0:
            return wxColour(vi, t, p);
        case 1:
            return wxColour(q, vi, p);
        case 2:
            return wxColour(p, vi, t);
        case 3:
            return wxColour(p, q, vi);
        case 4:
            return wxColour(t, p, vi);
        case 5:
            return wxColour(vi, p, q);
    }
    assert(false);
    return *wxBLACK;
}
*/
}

#endif //IMAGE_TOOLS_HEADER_45782456427634254
