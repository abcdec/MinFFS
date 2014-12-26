// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "icon_buffer.h"
#include <queue>
#include <set>
#include <zen/thread.h> //includes <boost/thread.hpp>
#include <zen/scope_guard.h>
#include <wx+/image_resources.h>

#ifdef ZEN_WIN
    #include <zen/dll.h>
    #include <zen/win_ver.h>
    #include <wx/image.h>
    #include "../dll/Thumbnail/thumbnail.h"

#elif defined ZEN_LINUX
    #include <gtk/gtk.h>

#elif defined ZEN_MAC
    #include "osx_file_icon.h"
#endif

using namespace zen;


namespace
{
const size_t BUFFER_SIZE_MAX = 800; //maximum number of icons to hold in buffer: must be big enough to hold visible icons + preload buffer! Consider OS limit on GDI resources (wxBitmap)!!!

#ifndef NDEBUG
    const boost::thread::id mainThreadId = boost::this_thread::get_id();
#endif

#ifdef ZEN_WIN
    const bool isXpOrLater = winXpOrLater(); //VS2010 compiled DLLs are not supported on Win 2000: Popup dialog "DecodePointer not found"

    #define DEF_DLL_FUN(name) const auto name = isXpOrLater ? DllFun<thumb::FunType_##name>(thumb::getDllName(), thumb::funName_##name) : DllFun<thumb::FunType_##name>();
    DEF_DLL_FUN(getIconByIndex);   //
    DEF_DLL_FUN(getThumbnail);     //let's spare the boost::call_once hustle and allocate statically
    DEF_DLL_FUN(releaseImageData); //
#endif

class IconHolder //handle HICON/GdkPixbuf ownership supporting thread-safe usage (in contrast to wxIcon/wxBitmap)
{
public:
#ifdef ZEN_WIN
    typedef const thumb::ImageData* HandleType;
#elif defined ZEN_LINUX
    typedef GdkPixbuf* HandleType;
#elif defined ZEN_MAC
    typedef osx::ImageData* HandleType;
#endif

    explicit IconHolder(HandleType handle = nullptr) : handle_(handle) {} //take ownership!

    IconHolder(IconHolder&& other) : handle_(other.release()) {}

    IconHolder& operator=(IconHolder other) //unifying assignment
    {
        other.swap(*this);
        return *this;
    }

    ~IconHolder()
    {
        if (handle_ != nullptr)
#ifdef ZEN_WIN
#ifdef TODO_MInFFS
	    releaseImageData(handle_); //should be checked already before creating IconHolder!
#else//TODO_MinFFS
	{}
#endif//TODO_MinFFS
#elif defined ZEN_LINUX
            ::g_object_unref(handle_); //superseedes "::gdk_pixbuf_unref"!
#elif defined ZEN_MAC
            delete handle_;
#endif
    }

    HandleType release()
    {
        ZEN_ON_SCOPE_EXIT(handle_ = nullptr);
        return handle_;
    }

    void swap(IconHolder& other) { std::swap(handle_, other.handle_); } //throw()

    //destroys raw icon! Call from GUI thread only!
    wxBitmap extractWxBitmap()
    {
        ZEN_ON_SCOPE_EXIT(assert(!*this));
        assert(boost::this_thread::get_id() == mainThreadId);

        if (!handle_)
            return wxNullBitmap;

#ifdef ZEN_WIN
        ZEN_ON_SCOPE_EXIT(IconHolder().swap(*this)); //destroy after extraction

        //let wxImage reference data without taking ownership:
        wxImage fileIcon(handle_->width, handle_->height, handle_->rgb, true);
        fileIcon.SetAlpha(handle_->alpha, true);
        return wxBitmap(fileIcon);

#elif defined ZEN_LINUX
        return wxBitmap(release()); //ownership passed!

#elif defined ZEN_MAC
        ZEN_ON_SCOPE_EXIT(IconHolder().swap(*this)); //destroy after extraction

        //let wxImage reference data without taking ownership:
        assert(!handle_->rgb.empty() && !handle_->alpha.empty());
        if (!handle_->rgb.empty())
        {
            wxImage fileIcon(handle_->width, handle_->height, &handle_->rgb[0], true);
            if (!handle_->alpha.empty())
                fileIcon.SetAlpha(&handle_->alpha[0], true);
            return wxBitmap(fileIcon);
        }
        return wxBitmap();
#endif
    }

private:
    HandleType handle_;

    IconHolder(const IconHolder& other); //move semantics!
    struct ConversionToBool { int dummy; };
public:
    //use member pointer as implicit conversion to bool (C++ Templates - Vandevoorde/Josuttis; chapter 20)
    operator int ConversionToBool::* () const { return handle_ != nullptr ? &ConversionToBool::dummy : nullptr; }
};


#if defined ZEN_WIN || defined ZEN_LINUX
Zstring getFileExtension(const Zstring& filepath)
{
    const Zstring shortName = afterLast(filepath, Zchar('\\')); //warning: using windows file name separator!

    return contains(shortName, Zchar('.')) ?
           afterLast(filepath, Zchar('.')) :
           Zstring();
}
#endif


#ifdef ZEN_WIN
const bool wereVistaOrLater = vistaOrLater(); //thread-safety: init at startup


thumb::IconSizeType getThumbSizeType(IconBuffer::IconSize sz)
{
    //coordinate with IconBuffer::getSize()!
    using namespace thumb;
    switch (sz)
    {
        case IconBuffer::SIZE_SMALL:
            return ICON_SIZE_16;
        case IconBuffer::SIZE_MEDIUM:
            if (!wereVistaOrLater) return ICON_SIZE_32; //48x48 doesn't look sharp on XP
            return ICON_SIZE_48;
        case IconBuffer::SIZE_LARGE:
            return ICON_SIZE_128;
    }
    return ICON_SIZE_16;
}


IconHolder getIconByAttribute(LPCWSTR pszPath, DWORD dwFileAttributes, IconBuffer::IconSize sz)
{
    //NOTE: CoInitializeEx()/CoUninitialize() needs to be called for THIS thread!
    SHFILEINFO fileInfo = {}; //initialize hIcon
    DWORD_PTR imgList = ::SHGetFileInfo(pszPath, //Windows 7 doesn't like this parameter to be an empty string
                                        dwFileAttributes,
                                        &fileInfo,
                                        sizeof(fileInfo),
                                        SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
    if (!imgList) //no need to IUnknown::Release() imgList!
        return IconHolder();

#ifdef TODO_MinFFS
    if (getIconByIndex && releaseImageData)
        return IconHolder(getIconByIndex(fileInfo.iIcon, getThumbSizeType(sz)));
#endif // TODO_MinFFS

    return IconHolder();
}


IconHolder getAssociatedIconByExt(const Zstring& extension, IconBuffer::IconSize sz)
{
    //no read-access to disk! determine icon by extension
    return getIconByAttribute((L"dummy." + extension).c_str(), FILE_ATTRIBUTE_NORMAL, sz);
}

#elif defined ZEN_LINUX
IconHolder iconHolderFromGicon(GIcon* gicon, IconBuffer::IconSize sz)
{
    if (gicon)
        if (GtkIconTheme* defaultTheme = ::gtk_icon_theme_get_default()) //not owned!
            if (GtkIconInfo* iconInfo = ::gtk_icon_theme_lookup_by_gicon(defaultTheme, gicon, IconBuffer::getSize(sz), GTK_ICON_LOOKUP_USE_BUILTIN)) //this may fail if icon is not installed on system
            {
                ZEN_ON_SCOPE_EXIT(::gtk_icon_info_free(iconInfo);)
                if (GdkPixbuf* pixBuf = ::gtk_icon_info_load_icon(iconInfo, nullptr))
                    return IconHolder(pixBuf); //pass ownership
            }
    return IconHolder();
}
#endif

#ifdef ZEN_WIN
std::set<Zstring, LessFilename> customIconExt //function-scope statics are not (yet) thread-safe in VC12
{
    L"ani",
    L"cur",
    L"exe",
    L"ico",
    L"msc",
    L"scr"
};
std::set<Zstring, LessFilename> linkExt
{
    L"lnk",
    L"pif",
    L"url",
    L"website"
};

//test for extension for non-thumbnail icons that can have a stock icon which does not have to be physically read from disc
bool isStandardIconExtension(const Zstring& extension)
{
    return customIconExt.find(extension) == customIconExt.end() &&
           linkExt.find(extension) == linkExt.end();
}
#endif
}

bool zen::hasLinkExtension(const Zstring& filepath)
{
#ifdef ZEN_WIN
    const Zstring& extension = getFileExtension(filepath);
    return linkExt.find(extension) != linkExt.end();
#elif defined ZEN_LINUX
    const Zstring& extension = getFileExtension(filepath);
    return extension == "desktop";
#elif defined ZEN_MAC
    return false; //alias files already get their arrow icon via "NSWorkspace::iconForFile"
#endif
}

//################################################################################################################################################

IconHolder getThumbnailImage(const Zstring& filepath, int requestedSize) //return 0 on failure
{
#ifdef ZEN_WIN
#ifdef TODO_MinFFS
    if (getThumbnail && releaseImageData)
        return IconHolder(getThumbnail(filepath.c_str(), requestedSize));
#endif // TODO_MinFFS

#elif defined ZEN_LINUX
    gint width  = 0;
    gint height = 0;
    if (GdkPixbufFormat* fmt = ::gdk_pixbuf_get_file_info(filepath.c_str(), &width, &height))
    {
        (void)fmt;
        if (width > 0 && height > 0 && requestedSize > 0)
        {
            int trgWidth  = width;
            int trgHeight = height;

            const int maxExtent = std::max(width, height); //don't stretch small images, but shrink large ones instead!
            if (requestedSize < maxExtent)
            {
                trgWidth  = width  * requestedSize / maxExtent;
                trgHeight = height * requestedSize / maxExtent;
            }
            if (GdkPixbuf* pixBuf = ::gdk_pixbuf_new_from_file_at_size(filepath.c_str(), trgWidth, trgHeight, nullptr))
                return IconHolder(pixBuf); //pass ownership
        }
    }

#elif defined ZEN_MAC
    try
    {
        return IconHolder(new osx::ImageData(osx::getThumbnail(filepath.c_str(), requestedSize))); //throw SysError
    }
    catch (zen::SysError&) {}
#endif
    return IconHolder();
}


IconHolder getGenericFileIcon(IconBuffer::IconSize sz)
{
    //we're called by getAssociatedIcon()! -> avoid endless recursion!
#ifdef ZEN_WIN
    return getIconByAttribute(L"dummy", FILE_ATTRIBUTE_NORMAL, sz);

#elif defined ZEN_LINUX
    const char* mimeFileIcons[] =
    {
        "application-x-zerosize", //Kubuntu: /usr/share/icons/oxygen/48x48/mimetypes
        "text-x-generic",         //http://live.gnome.org/GnomeArt/Tutorials/IconThemes
        "empty",            //Ubuntu: /usr/share/icons/Humanity/mimes/48
        GTK_STOCK_FILE,     //"gtk-file",
        "gnome-fs-regular", //
    };

    if (GtkIconTheme* defaultTheme = gtk_icon_theme_get_default()) //not owned!
        for (auto it = std::begin(mimeFileIcons); it != std::end(mimeFileIcons); ++it)
            if (GdkPixbuf* pixBuf = gtk_icon_theme_load_icon(defaultTheme, *it, IconBuffer::getSize(sz), GTK_ICON_LOOKUP_USE_BUILTIN, nullptr))
                return IconHolder(pixBuf); //pass ownership
    return IconHolder();

#elif defined ZEN_MAC
    try
    {
        return IconHolder(new osx::ImageData(osx::getDefaultFileIcon(IconBuffer::getSize(sz)))); //throw SysError
    }
    catch (zen::SysError&) {}
    return IconHolder();
#endif
}


IconHolder getAssociatedIcon(const Zstring& filepath, IconBuffer::IconSize sz)
{
    //1. try to load thumbnails
    switch (sz)
    {
        case IconBuffer::SIZE_SMALL:
            break;
        case IconBuffer::SIZE_MEDIUM:
        case IconBuffer::SIZE_LARGE:
            if (IconHolder ico = getThumbnailImage(filepath, IconBuffer::getSize(sz)))
                return ico;
            //else: fallback to non-thumbnail icon
            break;
    }

    //2. retrieve file icons
#ifdef ZEN_WIN
    //perf: optimize fallback case for SIZE_MEDIUM and SIZE_LARGE:
    const Zstring& extension = getFileExtension(filepath);
    if (isStandardIconExtension(extension)) //"pricey" extensions are stored with fullnames and are read from disk, while cheap ones require just the extension
        return getAssociatedIconByExt(extension, sz);
    //SIZE_MEDIUM or SIZE_LARGE: result will buffered under full filepath, not extension; this is okay: failure to load thumbnail is independent from extension in general!

    SHFILEINFO fileInfo = {};
    if (DWORD_PTR imgList = ::SHGetFileInfo(filepath.c_str(), //_In_     LPCTSTR pszPath, -> note: ::SHGetFileInfo() can't handle \\?\-prefix!
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

#ifdef TODO_MinFFS
        if (getIconByIndex && releaseImageData)
            if (const thumb::ImageData* imgData = getIconByIndex(fileInfo.iIcon, getThumbSizeType(sz)))
                return IconHolder(imgData);
#endif//TODO_MinFFS
    }

#elif defined ZEN_LINUX
    GFile* file = ::g_file_new_for_path(filepath.c_str()); //documented to "never fail"
    ZEN_ON_SCOPE_EXIT(::g_object_unref(file);)

    if (GFileInfo* fileInfo = ::g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, nullptr, nullptr))
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(fileInfo);)
        if (GIcon* gicon = ::g_file_info_get_icon(fileInfo)) //not owned!
            return iconHolderFromGicon(gicon, sz);
    }
    //need fallback: icon lookup may fail because some icons are currently not present on system

#elif defined ZEN_MAC
    try
    {
        return IconHolder(new osx::ImageData(osx::getFileIcon(filepath.c_str(), IconBuffer::getSize(sz)))); //throw SysError
    }
    catch (zen::SysError&) { assert(false); }
#endif
    return ::getGenericFileIcon(sz); //make sure this does not internally call getAssociatedIcon("someDefaultFile.txt")!!! => endless recursion!
}

//################################################################################################################################################

//---------------------- Shared Data -------------------------
class WorkLoad
{
public:
    Zstring extractNextFile() //context of worker thread, blocking
    {
        assert(boost::this_thread::get_id() != mainThreadId);
        boost::unique_lock<boost::mutex> dummy(lockFiles);

        while (filesToLoad.empty())
            conditionNewFiles.timed_wait(dummy, boost::posix_time::milliseconds(100)); //interruption point!

        Zstring filepath = filesToLoad.back(); //yes, not std::bad_alloc exception-safe, but bad_alloc is not relevant for us
        filesToLoad.pop_back();                //
        return filepath;
    }

    void setWorkload(const std::vector<Zstring>& newLoad) //context of main thread
    {
        assert(boost::this_thread::get_id() == mainThreadId);
        {
            boost::lock_guard<boost::mutex> dummy(lockFiles);
            filesToLoad = newLoad;
        }
        conditionNewFiles.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        //condition handling, see: http://www.boost.org/doc/libs/1_43_0/doc/html/thread/synchronization.html#thread.synchronization.condvar_ref
    }

    void addToWorkload(const Zstring& newEntry) //context of main thread
    {
        assert(boost::this_thread::get_id() == mainThreadId);
        {
            boost::lock_guard<boost::mutex> dummy(lockFiles);
            filesToLoad.push_back(newEntry); //set as next item to retrieve
        }
        conditionNewFiles.notify_all();
    }

private:
    std::vector<Zstring>      filesToLoad; //processes last elements of vector first!
    boost::mutex              lockFiles;
    boost::condition_variable conditionNewFiles; //signal event: data for processing available
};


class Buffer
{
public:
    Buffer() : firstInsertPos(iconList.end()), lastInsertPos(iconList.end()) {}

    //called by main and worker thread:
    bool hasIcon(const Zstring& filepath) const
    {
        boost::lock_guard<boost::mutex> dummy(lockIconList);
        return iconList.find(filepath) != iconList.end();
    }

    //must be called by main thread only! => wxBitmap is NOT thread-safe like an int (non-atomic ref-count!!!)
    Opt<wxBitmap> retrieve(const Zstring& filepath)
    {
        assert(boost::this_thread::get_id() == mainThreadId);
        boost::lock_guard<boost::mutex> dummy(lockIconList);

        auto it = iconList.find(filepath);
        if (it == iconList.end())
            return NoValue();

        markAsHot(it);

        IconData& idata = refData(it);
        if (idata.iconRaw) //if not yet converted...
        {
            idata.iconFmt = make_unique<wxBitmap>(idata.iconRaw.extractWxBitmap()); //convert in main thread!
            assert(!idata.iconRaw);
        }
        return idata.iconFmt ? *idata.iconFmt : wxNullBitmap; //idata.iconRaw may be inserted as empty from worker thread!
    }

    //called by main and worker thread:
    void insert(const Zstring& entryName, IconHolder&& icon)
    {
        boost::lock_guard<boost::mutex> dummy(lockIconList);

        //thread safety: moving IconHolder is free from side effects, but ~wxBitmap() is NOT! => do NOT delete items from iconList here!
        auto rc = iconList.emplace(entryName, makeValueObject());
        assert(rc.second);
        if (rc.second) //if insertion took place
        {
            refData(rc.first).iconRaw = std::move(icon);
            priorityListPushBack(rc.first);
        }
    }

    //must be called by main thread only! => ~wxBitmap() is NOT thread-safe!
    //call at an appropriate time, e.g.	after Workload::setWorkload()
    void limitSize()
    {
        assert(boost::this_thread::get_id() == mainThreadId);
        boost::lock_guard<boost::mutex> dummy(lockIconList);

        while (iconList.size() > BUFFER_SIZE_MAX)
        {
            auto itDelPos = firstInsertPos;
            priorityListPopFront();
            iconList.erase(itDelPos); //remove oldest element
        }
    }

private:
    struct IconData;

#if defined ZEN_WIN || defined ZEN_LINUX
    typedef std::map<Zstring, IconData, LessFilename> FileIconMap;
    IconData& refData(FileIconMap::iterator it) { return it->second; }
    static IconData makeValueObject() { return IconData(); }
#elif defined ZEN_MAC //workaround libc++ limitation for incomplete types: http://llvm.org/bugs/show_bug.cgi?id=17701
    typedef std::map<Zstring, std::unique_ptr<IconData>, LessFilename> FileIconMap;
    static IconData& refData(FileIconMap::iterator it) { return *(it->second); }
    static std::unique_ptr<IconData> makeValueObject() { return make_unique<IconData>(); }
#endif

    //call while holding lock:
    void priorityListPopFront()
    {
        assert(firstInsertPos!= iconList.end());
        firstInsertPos = refData(firstInsertPos).next_;

        if (firstInsertPos != iconList.end())
            refData(firstInsertPos).prev_ = iconList.end();
        else //BUFFER_SIZE_MAX > 0, but still for completeness:
            lastInsertPos = iconList.end();
    }

    //call while holding lock:
    void priorityListPushBack(FileIconMap::iterator it)
    {
        if (lastInsertPos == iconList.end())
        {
            assert(firstInsertPos == iconList.end());
            firstInsertPos = lastInsertPos = it;
            refData(it).prev_ = refData(it).next_ = iconList.end();
        }
        else
        {
            refData(it).next_ = iconList.end();
            refData(it).prev_ = lastInsertPos;
            refData(lastInsertPos).next_ = it;
            lastInsertPos = it;
        }
    }

    //call while holding lock:
    void markAsHot(FileIconMap::iterator it) //mark existing buffer entry as if newly inserted
    {
        assert(it != iconList.end());
        if (refData(it).next_ != iconList.end())
        {
            if (refData(it).prev_ != iconList.end())
            {
                refData(refData(it).prev_).next_ = refData(it).next_; //remove somewhere from the middle
                refData(refData(it).next_).prev_ = refData(it).prev_; //
            }
            else
            {
                assert(it == firstInsertPos);
                priorityListPopFront();
            }
            priorityListPushBack(it);
        }
        else
        {
            if (refData(it).prev_ != iconList.end())
                assert(it == lastInsertPos); //nothing to do
            else
                assert(iconList.size() == 1 && it == firstInsertPos && it == lastInsertPos); //nothing to do
        }
    }

    struct IconData
    {
        IconData() {}
        IconData(IconData&& tmp) : iconRaw(std::move(tmp.iconRaw)), iconFmt(std::move(tmp.iconFmt)), prev_(tmp.prev_), next_(tmp.next_) {}

        IconHolder iconRaw; //native icon representation: may be used by any thread

        std::unique_ptr<wxBitmap> iconFmt; //use ONLY from main thread!
        //wxBitmap is NOT thread-safe: non-atomic ref-count just to begin with...
        //- prohibit implicit calls to wxBitmap(const wxBitmap&)
        //- prohibit calls to ~wxBitmap() and transitively ~IconData()
        //- prohibit even wxBitmap() default constructor - better be safe than sorry!

        FileIconMap::iterator prev_; //store list sorted by time of insertion into buffer
        FileIconMap::iterator next_; //
    };

    mutable boost::mutex lockIconList;
    FileIconMap iconList; //shared resource; Zstring is thread-safe like an int
    FileIconMap::iterator firstInsertPos;
    FileIconMap::iterator lastInsertPos;
};

//################################################################################################################################################

class WorkerThread //lifetime is part of icon buffer
{
public:
    WorkerThread(const std::shared_ptr<WorkLoad>& workload,
                 const std::shared_ptr<Buffer>& buffer,
                 IconBuffer::IconSize st) :
        workload_(workload),
        buffer_(buffer),
        iconSizeType(st) {}

    void operator()(); //thread entry

private:
    std::shared_ptr<WorkLoad> workload_; //main/worker thread may access different shared_ptr instances safely (even though they have the same target!)
    std::shared_ptr<Buffer> buffer_;     //http://www.boost.org/doc/libs/1_43_0/libs/smart_ptr/shared_ptr.htm?sess=8153b05b34d890e02d48730db1ff7ddc#ThreadSafety
    const IconBuffer::IconSize iconSizeType;
};


void WorkerThread::operator()() //thread entry
{
    //failure to initialize COM for each thread is a source of hard to reproduce bugs: https://sourceforge.net/tracker/?func=detail&aid=3160472&group_id=234430&atid=1093080
#ifdef ZEN_WIN
    //Prerequisites, see thumbnail.h

    //1. Initialize COM
#ifdef TODO_MinFFS
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {
        assert(false);
        return;
    }
    ZEN_ON_SCOPE_EXIT(::CoUninitialize());
#endif//TODO_MinFFS
    
    //2. Initialize system image list
    typedef BOOL (WINAPI* FileIconInitFun)(BOOL fRestoreCache);
    const SysDllFun<FileIconInitFun> fileIconInit(L"Shell32.dll", reinterpret_cast<LPCSTR>(660)); //MS requires and documents this magic number
#ifdef TODO_MinFFS
    assert(fileIconInit);
    if (fileIconInit)
        fileIconInit(false); //TRUE to restore the system image cache from disk; FALSE otherwise.
#endif//MinFFS
#endif

    while (true)
    {
        boost::this_thread::interruption_point();

        const Zstring filepath = workload_->extractNextFile(); //start work: blocks until next icon to load is retrieved

        if (!buffer_->hasIcon(filepath)) //perf: workload may contain duplicate entries?
            buffer_->insert(filepath, getAssociatedIcon(filepath, iconSizeType));
    }
}

//#########################  redirect to impl  #####################################################

struct IconBuffer::Pimpl
{
    Pimpl() :
        workload(std::make_shared<WorkLoad>()),
        buffer  (std::make_shared<Buffer>()) {}

    std::shared_ptr<WorkLoad> workload;
    std::shared_ptr<Buffer> buffer;

    boost::thread worker;
};


IconBuffer::IconBuffer(IconSize sz) : pimpl(make_unique<Pimpl>()), iconSizeType(sz)
{
    pimpl->worker = boost::thread(WorkerThread(pimpl->workload, pimpl->buffer, sz));
}


IconBuffer::~IconBuffer()
{
    setWorkload(std::vector<Zstring>()); //make sure interruption point is always reached!
    pimpl->worker.interrupt();
    pimpl->worker.join(); //we assume precondition "worker.joinable()"!!!
}


int IconBuffer::getSize(IconSize icoSize)
{
    switch (icoSize)
    {
        case IconBuffer::SIZE_SMALL:
#if defined ZEN_WIN || defined ZEN_MAC
            return 16;
#elif defined ZEN_LINUX
            return 24;
#endif
        case IconBuffer::SIZE_MEDIUM:
#ifdef ZEN_WIN
            if (!wereVistaOrLater) return 32; //48x48 doesn't look sharp on XP
#endif
            return 48;

        case IconBuffer::SIZE_LARGE:
            return 128;
    }
    assert(false);
    return 0;
}


bool IconBuffer::readyForRetrieval(const Zstring& filepath)
{
#ifdef ZEN_WIN
    if (iconSizeType == IconBuffer::SIZE_SMALL)
        if (isStandardIconExtension(getFileExtension(filepath)))
            return true;
#endif
    return pimpl->buffer->hasIcon(filepath);
}


Opt<wxBitmap> IconBuffer::retrieveFileIcon(const Zstring& filepath)
{
#ifdef ZEN_WIN
    //perf: let's read icons which don't need file access right away! No async delay justified!
    if (iconSizeType == IconBuffer::SIZE_SMALL) //non-thumbnail view, we need file type icons only!
    {
        const Zstring& extension = getFileExtension(filepath);
        if (isStandardIconExtension(extension)) //"pricey" extensions are stored with fullnames and are read from disk, while cheap ones require just the extension
        {
            if (Opt<wxBitmap> ico = pimpl->buffer->retrieve(extension))
                return ico;

            //make sure icon is in buffer, even if icon needs not be retrieved!
            pimpl->buffer->insert(extension, getAssociatedIconByExt(extension, iconSizeType));

            Opt<wxBitmap> ico = pimpl->buffer->retrieve(extension);
            assert(ico);
            return ico;
        }
    }
#endif

    if (Opt<wxBitmap> ico = pimpl->buffer->retrieve(filepath))
        return ico;

    //since this icon seems important right now, we don't want to wait until next setWorkload() to start retrieving
    pimpl->workload->addToWorkload(filepath);
    pimpl->buffer->limitSize();
    return NoValue();
}


void IconBuffer::setWorkload(const std::vector<Zstring>& load)
{
    assert(load.size() < BUFFER_SIZE_MAX / 2);

    pimpl->workload->setWorkload(load); //since buffer can only increase due to new workload,
    pimpl->buffer->limitSize();   //this is the place to impose the limit from main thread!
}


wxBitmap IconBuffer::genericFileIcon(IconSize sz)
{
    return ::getGenericFileIcon(sz).extractWxBitmap();
}


wxBitmap IconBuffer::genericDirIcon(IconSize sz)
{
    return [sz]
    {
#ifdef ZEN_WIN
        return getIconByAttribute(L"dummy", //Windows 7 doesn't like this parameter to be an empty string!
        FILE_ATTRIBUTE_DIRECTORY, sz);
#elif defined ZEN_LINUX
        if (GIcon* dirIcon = ::g_content_type_get_icon("inode/directory")) //should contain fallback to GTK_STOCK_DIRECTORY ("gtk-directory")
            return iconHolderFromGicon(dirIcon, sz);
        return IconHolder();

#elif defined ZEN_MAC
        try
        {
            return IconHolder(new osx::ImageData(osx::getDefaultFolderIcon(IconBuffer::getSize(sz)))); //throw SysError
        }
        catch (zen::SysError&) { return IconHolder(); }
#endif
    }().extractWxBitmap();
}


wxBitmap IconBuffer::linkOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getSize()!
    return getResourceImage([sz]
    {
        switch (sz)
        {
            case IconBuffer::SIZE_SMALL:
#if defined ZEN_WIN || defined ZEN_MAC
                return L"link_16";
#elif defined ZEN_LINUX
                return L"link_24";
#endif
            case IconBuffer::SIZE_MEDIUM:
#ifdef ZEN_WIN
                if (!wereVistaOrLater) return L"link_32";
#endif
                return L"link_48";

            case IconBuffer::SIZE_LARGE:
                return L"link_128";
        }
        assert(false);
        return L"link_16";
    }());
}
