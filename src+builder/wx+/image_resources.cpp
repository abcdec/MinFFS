// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "image_resources.h"
#include <memory>
#include <map>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <zen/utf.h>
#include "image_tools.h"

using namespace zen;


namespace
{
void loadAnimFromZip(wxZipInputStream& zipInput, wxAnimation& anim)
{
    //work around wxWidgets bug:
    //construct seekable input stream (zip-input stream is not seekable) for wxAnimation::Load()
    //luckily this method call is very fast: below measurement precision!
    std::vector<char> data;
    data.reserve(10000);

    int newValue = 0;
    while ((newValue = zipInput.GetC()) != wxEOF)
        data.push_back(newValue);

    wxMemoryInputStream seekAbleStream(&data.front(), data.size()); //stream does not take ownership of data

    anim.Load(seekAbleStream, wxANIMATION_TYPE_GIF);
}


class GlobalResources
{
public:
    static GlobalResources& instance()
    {
#if defined _MSC_VER && _MSC_VER < 1900
#error function scope static initialization is not yet thread-safe!
#endif
        static GlobalResources inst;
        return inst;
    }

    void init(const Zstring& filepath);

    const wxBitmap&    getImage    (const wxString& name) const;
    const wxAnimation& getAnimation(const wxString& name) const;

private:
    GlobalResources() {}
    GlobalResources           (const GlobalResources&) = delete;
    GlobalResources& operator=(const GlobalResources&) = delete;

    std::map<wxString, wxBitmap> bitmaps;
    std::map<wxString, wxAnimation> anims;
};


void GlobalResources::init(const Zstring& filepath)
{
    assert(bitmaps.empty() && anims.empty());

    wxFFileInputStream input(utfCvrtTo<wxString>(filepath));
    if (input.IsOk()) //if not... we don't want to react too harsh here
    {
        //activate support for .png files
        wxImage::AddHandler(new wxPNGHandler); //ownership passed

        wxZipInputStream streamIn(input, wxConvUTF8);
        //do NOT rely on wxConvLocal! On failure shows unhelpful popup "Cannot convert from the charset 'Unknown encoding (-1)'!"

        while (true)
        {
            std::unique_ptr<wxZipEntry> entry(streamIn.GetNextEntry()); //take ownership!
            if (!entry)
                break;

            const wxString name = entry->GetName();

            //generic image loading
            if (endsWith(name, L".png"))
            {
                wxImage img(streamIn, wxBITMAP_TYPE_PNG);

                //end this alpha/no-alpha/mask/wxDC::DrawBitmap/RTL/high-contrast-scheme interoperability nightmare here and now!!!!
                //=> there's only one type of png image: with alpha channel, no mask!!!
                convertToVanillaImage(img);

                bitmaps.emplace(name, img);
            }
            else if (endsWith(name, L".gif"))
                loadAnimFromZip(streamIn, anims[name]);
        }
    }
}


const wxBitmap& GlobalResources::getImage(const wxString& name) const
{
    auto it = bitmaps.find(contains(name, L'.') ? name : name + L".png"); //assume .png ending if nothing else specified
    if (it != bitmaps.end())
        return it->second;

    assert(false);
    return wxNullBitmap;
}


const wxAnimation& GlobalResources::getAnimation(const wxString& name) const
{
    auto it = anims.find(contains(name, L'.') ? name : name + L".gif");
    if (it != anims.end())
        return it->second;

    assert(false);
    return wxNullAnimation;
}
}


void zen::initResourceImages(const Zstring& filepath) { GlobalResources::instance().init(filepath); }

const wxBitmap& zen::getResourceImage(const wxString& name) { return GlobalResources::instance().getImage(name); }

const wxAnimation& zen::getResourceAnimation(const wxString& name) { return GlobalResources::instance().getAnimation(name); }
