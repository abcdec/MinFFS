// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "icon_loader.h"
#include <zen/scope_guard.h>

#ifdef ZEN_WIN
    #include <zen/dll.h>
    #include <zen/win_ver.h>
    #include "file_icon_win.h"

#elif defined ZEN_LINUX
    #include <gtk/gtk.h>
    #include <sys/stat.h>

#elif defined ZEN_MAC
    #include "file_icon_osx.h"
#endif

using namespace zen;


namespace
{
#ifdef ZEN_LINUX
ImageHolder copyToImageHolder(const GdkPixbuf* pixbuf)
{
    //see: https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-The-GdkPixbuf-Structure.html
    if (pixbuf &&
        ::gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB &&
        ::gdk_pixbuf_get_bits_per_sample(pixbuf) == 8)
    {
        const int channels = ::gdk_pixbuf_get_n_channels(pixbuf);
        if (channels == 3 || channels == 4)
        {
            const int stride = ::gdk_pixbuf_get_rowstride(pixbuf);
            const unsigned char* rgbaSrc = ::gdk_pixbuf_get_pixels(pixbuf);

            if (channels == 3)
            {
                assert(!::gdk_pixbuf_get_has_alpha(pixbuf));

                ImageHolder out(::gdk_pixbuf_get_width(pixbuf), ::gdk_pixbuf_get_height(pixbuf), false /*withAlpha*/);
                unsigned char* rgbTrg = out.getRgb();

                for (int y = 0; y < out.getHeight(); ++y)
                {
                    const unsigned char* srcLine = rgbaSrc + y * stride;
                    for (int x = 0; x < out.getWidth(); ++x)
                    {
                        *rgbTrg++ = *srcLine++;
                        *rgbTrg++ = *srcLine++;
                        *rgbTrg++ = *srcLine++;
                    }
                }
                return out;
            }
            else if (channels == 4)
            {
                assert(::gdk_pixbuf_get_has_alpha(pixbuf));

                ImageHolder out(::gdk_pixbuf_get_width(pixbuf), ::gdk_pixbuf_get_height(pixbuf), true /*withAlpha*/);
                unsigned char* rgbTrg   = out.getRgb();
                unsigned char* alphaTrg = out.getAlpha();

                for (int y = 0; y < out.getHeight(); ++y)
                {
                    const unsigned char* srcLine = rgbaSrc + y * stride;
                    for (int x = 0; x < out.getWidth(); ++x)
                    {
                        *rgbTrg++   = *srcLine++;
                        *rgbTrg++   = *srcLine++;
                        *rgbTrg++   = *srcLine++;
                        *alphaTrg++ = *srcLine++;
                    }
                }
                return out;
            }
        }
    }
    return ImageHolder();
}
#endif


#ifdef ZEN_WIN

#ifdef MinFFS_PATCH
#ifdef TODO_MinFFS_GetCorrectIcon
#else
typedef enum _IconSizeType {
    ICON_SIZE_16,
    ICON_SIZE_32,
    ICON_SIZE_48,
    ICON_SIZE_128,
    ICON_SIZE_256
} IconSizeType;

ImageHolder getIconByIndex(int iIconIn, IconSizeType iconSizeTypeIn)
{
    return ImageHolder();
}

ImageHolder getThumbnail(LPCWSTR filePathIn, int pixelSizeIn)
{
    return ImageHolder();
}
#endif//TODO_MinFFS_GetCorrectIcon
#endif//MinFFS_PATCH

IconSizeType getThumbSizeType(int pixelSize)
{
    //coordinate with IconBuffer::getSize()!
    if (pixelSize >= 256) return ICON_SIZE_256;
    if (pixelSize >= 128) return ICON_SIZE_128;
    if (pixelSize >=  48) return ICON_SIZE_48;
    if (pixelSize >=  32) return ICON_SIZE_32;
    return ICON_SIZE_16;
}

ImageHolder getIconByAttribute(LPCWSTR pszPath, DWORD dwFileAttributes, int pixelSize)
{
    //NOTE: CoInitializeEx()/CoUninitialize() needs to be called for THIS thread!
    SHFILEINFO fileInfo = {}; //initialize hIcon
    DWORD_PTR imgList = ::SHGetFileInfo(::wcslen(pszPath) == 0 ? L"dummy" : pszPath, //Windows 7 doesn't like this parameter to be an empty string!
                                        dwFileAttributes,
                                        &fileInfo,
                                        sizeof(fileInfo),
                                        SHGFI_USEFILEATTRIBUTES | //== no disk access: http://blogs.msdn.com/b/oldnewthing/archive/2004/06/01/145428.aspx
                                        SHGFI_SYSICONINDEX);
    if (!imgList) //not owned: no need for IUnknown::Release()!
        return ImageHolder();

    if (ImageHolder img = getIconByIndex(fileInfo.iIcon, getThumbSizeType(pixelSize)))
        return img;

    return ImageHolder();
}

#elif defined ZEN_LINUX
ImageHolder imageHolderFromGicon(GIcon* gicon, int pixelSize)
{
    if (gicon)
        if (GtkIconTheme* defaultTheme = ::gtk_icon_theme_get_default()) //not owned!
            if (GtkIconInfo* iconInfo = ::gtk_icon_theme_lookup_by_gicon(defaultTheme, gicon, pixelSize, GTK_ICON_LOOKUP_USE_BUILTIN)) //this may fail if icon is not installed on system
            {
                ZEN_ON_SCOPE_EXIT(::gtk_icon_info_free(iconInfo));
                if (GdkPixbuf* pixBuf = ::gtk_icon_info_load_icon(iconInfo, nullptr))
                {
                    ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf)); //superseedes "::gdk_pixbuf_unref"!
                    return copyToImageHolder(pixBuf);
                }
            }
    return ImageHolder();
}
#endif
}


ImageHolder zen::getIconByTemplatePath(const Zstring& templatePath, int pixelSize)
{
#ifdef ZEN_WIN
    //no read-access to disk! determine icon by extension
    return getIconByAttribute(templatePath.c_str(), FILE_ATTRIBUTE_NORMAL, pixelSize);

#elif defined ZEN_LINUX
    //uses full file name, e.g. "AUTHORS" has own mime type on Linux:
    if (gchar* contentType = ::g_content_type_guess(templatePath.c_str(), //const gchar* filename,
                                                    nullptr,              //const guchar* data,
                                                    0,                    //gsize data_size,
                                                    nullptr))             //gboolean* result_uncertain
    {
        ZEN_ON_SCOPE_EXIT(::g_free(contentType));
        if (GIcon* dirIcon = ::g_content_type_get_icon(contentType))
        {
            ZEN_ON_SCOPE_EXIT(::g_object_unref(dirIcon));
            return imageHolderFromGicon(dirIcon, pixelSize);
        }
    }
    return ImageHolder();

#elif defined ZEN_MAC
    try
    {
        return osx::getIconByExtension(getFileExtension(templatePath).c_str(), pixelSize); //throw SysError
    }
    catch (SysError&) { return ImageHolder(); }
#endif
}


ImageHolder zen::genericFileIcon(int pixelSize)
{
    //we're called by getDisplayIcon()! -> avoid endless recursion!
#ifdef ZEN_WIN
    return getIconByAttribute(L"", FILE_ATTRIBUTE_NORMAL, pixelSize);

#elif defined ZEN_LINUX
    if (GIcon* fileIcon = ::g_content_type_get_icon("text/plain"))
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(fileIcon));
        return imageHolderFromGicon(fileIcon, pixelSize);
    }
    return ImageHolder();

#elif defined ZEN_MAC
    try
    {
        return osx::getDefaultFileIcon(pixelSize); //throw SysError
    }
    catch (SysError&) { return ImageHolder(); }
#endif
}


ImageHolder zen::genericDirIcon(int pixelSize)
{
#ifdef ZEN_WIN
    return getIconByAttribute(L"", FILE_ATTRIBUTE_DIRECTORY, pixelSize);

#elif defined ZEN_LINUX
    if (GIcon* dirIcon = ::g_content_type_get_icon("inode/directory")) //should contain fallback to GTK_STOCK_DIRECTORY ("gtk-directory")
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(dirIcon));
        return imageHolderFromGicon(dirIcon, pixelSize);
    }
    return ImageHolder();

#elif defined ZEN_MAC
    try
    {
        return osx::getDefaultFolderIcon(pixelSize); //throw SysError
    }
    catch (SysError&) { return ImageHolder(); }
#endif
}


ImageHolder zen::getFileIcon(const Zstring& filePath, int pixelSize)
{
    //2. retrieve file icons
#ifdef ZEN_WIN
    SHFILEINFO fileInfo = {};
    if (DWORD_PTR imgList = ::SHGetFileInfo(filePath.c_str(), //_In_     LPCTSTR pszPath, -> note: ::SHGetFileInfo() can't handle \\?\-prefix!
                                            0,                //DWORD dwFileAttributes,
                                            &fileInfo,        //_Inout_  SHFILEINFO *psfi,
                                            sizeof(fileInfo), //UINT cbFileInfo,
                                            SHGFI_SYSICONINDEX /*| SHGFI_ATTRIBUTES*/)) //UINT uFlags
    {
        (void)imgList;
        //imgList->Release(); //empiric study: crash on XP if we release this! Seems we do not own it... -> also no GDI leak on Win7 -> okay
        //another comment on http://msdn.microsoft.com/en-us/library/bb762179(v=VS.85).aspx describes exact same behavior on Win7/XP

        //Quote: "The IImageList pointer type, such as that returned in the ppv parameter, can be cast as an HIMAGELIST as needed;
        //        for example, for use in a list view. Conversely, an HIMAGELIST can be cast as a pointer to an IImageList."
        //http://msdn.microsoft.com/en-us/library/windows/desktop/bb762185(v=vs.85).aspx

        //Check for link icon type (= shell links and symlinks): SHGetFileInfo + SHGFI_ATTRIBUTES:
        //const bool isLink = (fileInfo.dwAttributes & SFGAO_LINK) != 0;

        if (ImageHolder img = getIconByIndex(fileInfo.iIcon, getThumbSizeType(pixelSize)))
            return img;
    }

#elif defined ZEN_LINUX
    GFile* file = ::g_file_new_for_path(filePath.c_str()); //documented to "never fail"
    ZEN_ON_SCOPE_EXIT(::g_object_unref(file));

    if (GFileInfo* fileInfo = ::g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, nullptr, nullptr))
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(fileInfo));
        if (GIcon* gicon = ::g_file_info_get_icon(fileInfo)) //not owned!
            return imageHolderFromGicon(gicon, pixelSize);
    }
    //need fallback: icon lookup may fail because some icons are currently not present on system

#elif defined ZEN_MAC
    try
    {
        return osx::getFileIcon(filePath.c_str(), pixelSize); //throw SysError
    }
    catch (SysError&) { assert(false); }
#endif
    return ImageHolder();
}


ImageHolder zen::getThumbnailImage(const Zstring& filePath, int pixelSize) //return null icon on failure
{
#ifdef ZEN_WIN
    if (ImageHolder img = getThumbnail(filePath.c_str(), pixelSize))
        return img;

#elif defined ZEN_LINUX
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0)
        if (!S_ISFIFO(fileInfo.st_mode)) //skip named pipes: else gdk_pixbuf_get_file_info() would hang forever!
        {
            gint width  = 0;
            gint height = 0;
            if (GdkPixbufFormat* fmt = ::gdk_pixbuf_get_file_info(filePath.c_str(), &width, &height))
            {
                (void)fmt;
                if (width > 0 && height > 0 && pixelSize > 0)
                {
                    int trgWidth  = width;
                    int trgHeight = height;

                    const int maxExtent = std::max(width, height); //don't stretch small images, shrink large ones only!
                    if (pixelSize < maxExtent)
                    {
                        trgWidth  = width  * pixelSize / maxExtent;
                        trgHeight = height * pixelSize / maxExtent;
                    }
                    if (GdkPixbuf* pixBuf = ::gdk_pixbuf_new_from_file_at_size(filePath.c_str(), trgWidth, trgHeight, nullptr))
                    {
                        ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf));
                        return copyToImageHolder(pixBuf);
                    }
                }
            }
        }

#elif defined ZEN_MAC
    try
    {
        return osx::getThumbnail(filePath.c_str(), pixelSize); //throw SysError
    }
    catch (SysError&) {}
#endif
    return ImageHolder();
}
