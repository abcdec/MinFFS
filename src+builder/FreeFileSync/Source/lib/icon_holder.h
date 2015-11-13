// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef IMAGE_HOLDER_H_284578426342567457
#define IMAGE_HOLDER_H_284578426342567457

#include <memory>

//used by fs/abstract.h => check carefully before adding dependencies!

namespace zen
{
struct ImageHolder //prepare conversion to wxImage as much as possible while staying thread-safe (in contrast to wxIcon/wxBitmap)
{
    ImageHolder() {}

    ImageHolder(int w, int h, bool withAlpha) : //init with allocated memory
        width(w), height(h),
        rgb(static_cast<unsigned char*>(::malloc(width * height * 3))),
        alpha(withAlpha ? static_cast<unsigned char*>(::malloc(width * height)) : nullptr) {}

    ImageHolder           (ImageHolder&& tmp) = default; //
    ImageHolder& operator=(ImageHolder&& tmp) = default; //move semantics only!
    ImageHolder           (const ImageHolder&) = delete; //
    ImageHolder& operator=(const ImageHolder&) = delete; //

    explicit operator bool() const { return rgb.get() != nullptr; }

    int getWidth () const { return width;  }
    int getHeight() const { return height; }

    unsigned char* getRgb  () { return rgb  .get(); }
    unsigned char* getAlpha() { return alpha.get(); }

    unsigned char* releaseRgb  () { return rgb  .release(); }
    unsigned char* releaseAlpha() { return alpha.release(); }

    struct CLibFree { void operator()(unsigned char* p) const { ::free(p); } }; //use malloc/free to allow direct move into wxImage!

private:
    int width  = 0;
    int height = 0;
    std::unique_ptr<unsigned char, CLibFree> rgb;   //optional
    std::unique_ptr<unsigned char, CLibFree> alpha; //
};
}

#endif //IMAGE_HOLDER_H_284578426342567457
