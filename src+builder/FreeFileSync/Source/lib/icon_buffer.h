// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ICONBUFFER_H_INCLUDED
#define ICONBUFFER_H_INCLUDED

#include <vector>
#include <memory>
#include <zen/zstring.h>
#include <zen/optional.h>
#include <wx/bitmap.h>

namespace zen
{
class IconBuffer
{
public:
    enum IconSize
    {
        SIZE_SMALL,
        SIZE_MEDIUM,
        SIZE_LARGE
    };

    IconBuffer(IconSize sz);
    ~IconBuffer();

    static int getSize(IconSize sz); //expected and *maximum* icon size in pixel
    int getSize() const { return getSize(iconSizeType); } //

    bool readyForRetrieval(const Zstring& filepath);
    Opt<wxBitmap> retrieveFileIcon(const Zstring& filepath); //... and mark as hot

    void setWorkload(const std::vector<Zstring>& load); //(re-)set new workload of icons to be retrieved;

    static wxBitmap genericFileIcon(IconSize sz);
    static wxBitmap genericDirIcon (IconSize sz);
    static wxBitmap linkOverlayIcon(IconSize sz);

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    const IconSize iconSizeType;
};

bool hasLinkExtension(const Zstring& filepath);
}

#endif // ICONBUFFER_H_INCLUDED
