// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "icon_buffer.h"
#include <map>
#include <set>
#include <zen/thread.h> //includes <std/thread.hpp>
#include <zen/scope_guard.h>
#include <wx+/image_resources.h>
#include "icon_loader.h"

#ifdef ZEN_WIN
    #include <zen/win_ver.h>
    #include <zen/com_tools.h>
#endif

using namespace zen;
using AFS = AbstractFileSystem;


namespace
{
const size_t BUFFER_SIZE_MAX = 800; //maximum number of icons to hold in buffer: must be big enough to hold visible icons + preload buffer! Consider OS limit on GDI resources (wxBitmap)!!!

#ifndef NDEBUG
    const std::thread::id mainThreadId = std::this_thread::get_id();
#endif

#ifdef ZEN_WIN
    const bool wereVistaOrLater = vistaOrLater();
#endif


//destroys raw icon! Call from GUI thread only!
wxBitmap extractWxBitmap(ImageHolder&& ih)
{
    assert(std::this_thread::get_id() == mainThreadId);
#ifndef NDEBUG
    ZEN_ON_SCOPE_EXIT(assert(!ih));
#endif

    if (!ih.getRgb())
        return wxNullBitmap;

    wxImage img(ih.getWidth(), ih.getHeight(), ih.releaseRgb(), false /*static_data*/); //pass ownership
    if (ih.getAlpha())
        img.SetAlpha(ih.releaseAlpha(), false);
    return wxBitmap(img);
}


#ifdef ZEN_WIN
const std::set<Zstring, LessFilePath> linkExt { L"lnk", L"pif", L"url", L"website" };


//test for extension for non-thumbnail icons that can have a stock icon which does not have to be physically read from disc
inline
bool hasStandardIconExtension(const Zstring& filePath)
{
    static const std::set<Zstring, LessFilePath> customIconExt { L"ani", L"cur", L"exe", L"ico", L"msc", L"scr" }; //function-scope statics are not (yet) thread-safe in VC12
#if defined _MSC_VER && _MSC_VER < 1900
#error function scope static initialization is not yet thread-safe!
#endif

    const Zstring extension(getFileExtension(filePath));

    return customIconExt.find(extension) == customIconExt.end() &&
           linkExt.find(extension) == linkExt.end();
}
#endif
}

//################################################################################################################################################

ImageHolder getDisplayIcon(const AbstractPath& itemPath, IconBuffer::IconSize sz)
{
    //1. try to load thumbnails
    switch (sz)
    {
        case IconBuffer::SIZE_SMALL:
            break;
        case IconBuffer::SIZE_MEDIUM:
        case IconBuffer::SIZE_LARGE:
            if (ImageHolder img = AFS::getThumbnailImage(itemPath, IconBuffer::getSize(sz)))
                return img;
            //else: fallback to non-thumbnail icon
            break;
    }

    const Zstring& templateName = AFS::getFileShortName(itemPath);

    //2. retrieve file icons
#ifdef ZEN_WIN
    //result will be buffered with full path, not extension; this is okay: failure to load thumbnail is independent from extension in general!
    if (!hasStandardIconExtension(templateName)) //perf: no need for physical disk access for standard icons
#endif
        if (ImageHolder ih = AFS::getFileIcon(itemPath, IconBuffer::getSize(sz)))
            return ih;

    //3. fallbacks
    if (ImageHolder ih = getIconByTemplatePath(templateName, IconBuffer::getSize(sz)))
        return ih;

    return genericFileIcon(IconBuffer::getSize(sz));
}

//################################################################################################################################################

//---------------------- Shared Data -------------------------
class WorkLoad
{
public:
    //context of worker thread, blocking:
    AbstractPath extractNextFile() //throw ThreadInterruption
    {
        assert(std::this_thread::get_id() != mainThreadId);
        std::unique_lock<std::mutex> dummy(lockFiles);

        interruptibleWait(conditionNewWork, dummy, [this] { return !workLoad.empty(); }); //throw ThreadInterruption

        AbstractPath filePath = workLoad.back(); //
        workLoad.pop_back();                     //yes, no strong exception guarantee (std::bad_alloc)
        return filePath;                         //
    }

    void setWorkload(const std::vector<AbstractPath>& newLoad) //context of main thread
    {
        assert(std::this_thread::get_id() == mainThreadId);
        {
            std::lock_guard<std::mutex> dummy(lockFiles);

            workLoad.clear();
            for (const AbstractPath& filePath : newLoad)
                workLoad.emplace_back(filePath);
        }
        conditionNewWork.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        //condition handling, see: http://www.boost.org/doc/libs/1_43_0/doc/html/thread/synchronization.html#thread.synchronization.condvar_ref
    }

    void addToWorkload(const AbstractPath& filePath) //context of main thread
    {
        assert(std::this_thread::get_id() == mainThreadId);
        {
            std::lock_guard<std::mutex> dummy(lockFiles);
            workLoad.emplace_back(filePath); //set as next item to retrieve
        }
        conditionNewWork.notify_all();
    }

private:
    //AbstractPath is thread-safe like an int!
    std::vector<AbstractPath> workLoad; //processes last elements of vector first!
    std::mutex                lockFiles;
    std::condition_variable   conditionNewWork; //signal event: data for processing available
};


class Buffer
{
public:
    Buffer() : firstInsertPos(iconList.end()), lastInsertPos(iconList.end()) {}

    //called by main and worker thread:
    bool hasIcon(const AbstractPath& filePath) const
    {
        std::lock_guard<std::mutex> dummy(lockIconList);
        return iconList.find(filePath) != iconList.end();
    }

    //must be called by main thread only! => wxBitmap is NOT thread-safe like an int (non-atomic ref-count!!!)
    Opt<wxBitmap> retrieve(const AbstractPath& filePath)
    {
        assert(std::this_thread::get_id() == mainThreadId);
        std::lock_guard<std::mutex> dummy(lockIconList);

        auto it = iconList.find(filePath);
        if (it == iconList.end())
            return NoValue();

        markAsHot(it);

        IconData& idata = refData(it);
        if (idata.iconRaw) //if not yet converted...
        {
            idata.iconFmt = std::make_unique<wxBitmap>(extractWxBitmap(std::move(idata.iconRaw))); //convert in main thread!
            assert(!idata.iconRaw);
        }
        return idata.iconFmt ? *idata.iconFmt : wxNullBitmap; //idata.iconRaw may be inserted as empty from worker thread!
    }

    //called by main and worker thread:
    void insert(const AbstractPath& filePath, ImageHolder&& icon)
    {
        std::lock_guard<std::mutex> dummy(lockIconList);

        //thread safety: moving ImageHolder is free from side effects, but ~wxBitmap() is NOT! => do NOT delete items from iconList here!
        auto rc = iconList.emplace(filePath, makeValueObject());
        assert(rc.second); //insertion took place
        if (rc.second)
        {
            refData(rc.first).iconRaw = std::move(icon);
            priorityListPushBack(rc.first);
        }
    }

    //must be called by main thread only! => ~wxBitmap() is NOT thread-safe!
    //call at an appropriate time, e.g. after Workload::setWorkload()
    void limitSize()
    {
        assert(std::this_thread::get_id() == mainThreadId);
        std::lock_guard<std::mutex> dummy(lockIconList);

        while (iconList.size() > BUFFER_SIZE_MAX)
        {
            auto itDelPos = firstInsertPos;
            priorityListPopFront();
            iconList.erase(itDelPos); //remove oldest element
        }
    }

private:
    struct IconData;

#ifdef __clang__ //workaround libc++ limitation for incomplete types: http://llvm.org/bugs/show_bug.cgi?id=17701
    typedef std::map<AbstractPath, std::unique_ptr<IconData>, AFS::LessAbstractPath> FileIconMap;
    static IconData& refData(FileIconMap::iterator it) { return *(it->second); }
    static std::unique_ptr<IconData> makeValueObject() { return std::make_unique<IconData>(); }
#else
    typedef std::map<AbstractPath, IconData, AFS::LessAbstractPath> FileIconMap;
    IconData& refData(FileIconMap::iterator it) { return it->second; }
    static IconData makeValueObject() { return IconData(); }
#endif

    //call while holding lock:
    void priorityListPopFront()
    {
        assert(firstInsertPos!= iconList.end());
        firstInsertPos = refData(firstInsertPos).next_;

        if (firstInsertPos != iconList.end())
            refData(firstInsertPos).prev_ = iconList.end();
        else //priority list size > BUFFER_SIZE_MAX in this context, but still for completeness:
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

        ImageHolder iconRaw; //native icon representation: may be used by any thread

        std::unique_ptr<wxBitmap> iconFmt; //use ONLY from main thread!
        //wxBitmap is NOT thread-safe: non-atomic ref-count just to begin with...
        //- prohibit implicit calls to wxBitmap(const wxBitmap&)
        //- prohibit calls to ~wxBitmap() and transitively ~IconData()
        //- prohibit even wxBitmap() default constructor - better be safe than sorry!

        FileIconMap::iterator prev_; //store list sorted by time of insertion into buffer
        FileIconMap::iterator next_; //
    };

    mutable std::mutex lockIconList;
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

    void operator()() const; //thread entry

private:
    std::shared_ptr<WorkLoad> workload_; //main/worker thread may access different shared_ptr instances safely (even though they have the same target!)
    std::shared_ptr<Buffer> buffer_;     //http://www.boost.org/doc/libs/1_43_0/libs/smart_ptr/shared_ptr.htm?sess=8153b05b34d890e02d48730db1ff7ddc#ThreadSafety
    const IconBuffer::IconSize iconSizeType;
};


class RunOnStartup
{
public:
    RunOnStartup()
    {
#ifdef ZEN_WIN
        //icon_loader.h/file_icon_win.h prerequisites: 1. initialize COM, 2. initialize system image list
        typedef BOOL (WINAPI* FileIconInitFun)(BOOL fRestoreCache);
        const SysDllFun<FileIconInitFun> fileIconInit(L"Shell32.dll", reinterpret_cast<LPCSTR>(660)); //MS requires and documents this magic number
        assert(fileIconInit);
        if (fileIconInit)
            fileIconInit(true); //MSDN: "TRUE to restore the system image cache from disk; FALSE otherwise."
        /*
            "FileIconInit's "fRestoreCache" parameter determines whether or not it loads the 48-or-so "standard" shell icons. If FALSE is specified,
            it only loads a very minimal set of icons. [...] SHGetFileInfo internally call FileIconInit(FALSE), so if you want
            your copy of the system image list to contain the standard icons,  you should call FileIconInit(TRUE) at startup."
                - Jim Barry, MVP (Windows SDK)
        */
#endif
    }
} dummy;


void WorkerThread::operator()() const //thread entry
{
#ifdef ZEN_WIN
    setCurrentThreadName("Icon Buffer Worker");

    try
    {
#ifdef TODO_MinFFS_ComInit
        //1. Initialize COM here due to the icon_loader.h dependency only, but NOT due to native.h, mtp.h's internal COM usage => this is not our responsibility!
        ComInitializer ci; //throw SysError
#endif//TODO_MinFFS_ComInit
#endif

        for (;;)
        {
            interruptionPoint(); //throw ThreadInterruption

            //start work: blocks until next icon to load is retrieved:
            const AbstractPath itemPath = workload_->extractNextFile(); //throw ThreadInterruption

            if (!buffer_->hasIcon(itemPath)) //perf: workload may contain duplicate entries?
                buffer_->insert(itemPath, getDisplayIcon(itemPath, iconSizeType));
        }

#ifdef ZEN_WIN
    }
    catch (SysError&) { assert(false); }
#endif
}

//#########################  redirect to impl  #####################################################

struct IconBuffer::Pimpl
{
    std::shared_ptr<WorkLoad> workload = std::make_shared<WorkLoad>();
    std::shared_ptr<Buffer>   buffer   = std::make_shared<Buffer>();

    InterruptibleThread worker;

    //-------------------------
    std::map<Zstring, wxBitmap, LessFilePath> extensionIcons;
};


IconBuffer::IconBuffer(IconSize sz) : pimpl(std::make_unique<Pimpl>()), iconSizeType(sz)
{
    pimpl->worker = InterruptibleThread(WorkerThread(pimpl->workload, pimpl->buffer, sz));
}


IconBuffer::~IconBuffer()
{
    setWorkload({}); //make sure interruption point is always reached!
    pimpl->worker.interrupt();
    pimpl->worker.join();
}


int IconBuffer::getSize(IconSize sz)
{
    //coordinate with getThumbSizeType() and linkOverlayIcon()!
    switch (sz)
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


bool IconBuffer::readyForRetrieval(const AbstractPath& filePath)
{
#ifdef ZEN_WIN
    if (iconSizeType == IconBuffer::SIZE_SMALL)
        if (hasStandardIconExtension(AFS::getFileShortName(filePath)))
            return true;
#endif
    return pimpl->buffer->hasIcon(filePath);
}


Opt<wxBitmap> IconBuffer::retrieveFileIcon(const AbstractPath& filePath)
{
#ifdef ZEN_WIN
    //perf: let's read icons which don't need file access right away! No async delay justified!
    const Zstring fileName = AFS::getFileShortName(filePath);
    if (iconSizeType == IconBuffer::SIZE_SMALL) //non-thumbnail view, we need file type icons only!
        if (hasStandardIconExtension(fileName))
            return this->getIconByExtension(fileName); //buffered!!!
#endif

    if (Opt<wxBitmap> ico = pimpl->buffer->retrieve(filePath))
        return ico;

    //since this icon seems important right now, we don't want to wait until next setWorkload() to start retrieving
    pimpl->workload->addToWorkload(filePath);
    pimpl->buffer->limitSize();
    return NoValue();
}


void IconBuffer::setWorkload(const std::vector<AbstractPath>& load)
{
    assert(load.size() < BUFFER_SIZE_MAX / 2);

    pimpl->workload->setWorkload(load); //since buffer can only increase due to new workload,
    pimpl->buffer->limitSize();         //this is the place to impose the limit from main thread!
}


wxBitmap IconBuffer::getIconByExtension(const Zstring& filePath)
{
    const Zstring& extension = getFileExtension(filePath);

    assert(std::this_thread::get_id() == mainThreadId);

    auto it = pimpl->extensionIcons.find(extension);
    if (it == pimpl->extensionIcons.end())
    {
        const Zstring& templateName(extension.empty() ? Zstr("file") : Zstr("file.") + extension);
        //don't pass actual file name to getIconByTemplatePath(), e.g. "AUTHORS" has own mime type on Linux!!!
        //=> we want to buffer by extension only to minimize buffer-misses!
        it = pimpl->extensionIcons.emplace(extension, extractWxBitmap(getIconByTemplatePath(templateName, IconBuffer::getSize(iconSizeType)))).first;
    }
    //need buffer size limit???
    return it->second;
}


wxBitmap IconBuffer::genericFileIcon(IconSize sz)
{
    return extractWxBitmap(zen::genericFileIcon(IconBuffer::getSize(sz)));
}


wxBitmap IconBuffer::genericDirIcon(IconSize sz)
{
    return extractWxBitmap(zen::genericDirIcon(IconBuffer::getSize(sz)));
}


wxBitmap IconBuffer::linkOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getSize()!
    return getResourceImage([sz]
    {
        const int pixelSize = IconBuffer::getSize(sz);

        if (pixelSize >= 128) return L"link_128";
        if (pixelSize >=  48) return L"link_48";
        if (pixelSize >=  32) return L"link_32";
        if (pixelSize >=  24) return L"link_24";
        return L"link_16";
    }());
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
