// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "custom_grid.h"
#include <wx/dc.h>
#include <wx/settings.h>
#include <zen/i18n.h>
#include <zen/file_error.h>
#include <zen/basic_math.h>
#include <zen/format_unit.h>
#include <zen/scope_guard.h>
#include <wx+/tooltip.h>
#include <wx+/string_conv.h>
#include <wx+/rtl.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
#include "../file_hierarchy.h"

using namespace zen;
using namespace gridview;


const wxEventType zen::EVENT_GRID_CHECK_ROWS     = wxNewEventType();
const wxEventType zen::EVENT_GRID_SYNC_DIRECTION = wxNewEventType();

namespace
{
const wxColour COLOR_ORANGE      (238, 201,   0);
const wxColour COLOR_GREY        (212, 208, 200);
const wxColour COLOR_YELLOW      (247, 252,  62);
const wxColour COLOR_YELLOW_LIGHT(253, 252, 169);
const wxColour COLOR_CMP_RED     (255, 185, 187);
const wxColour COLOR_SYNC_BLUE   (185, 188, 255);
const wxColour COLOR_SYNC_GREEN  (196, 255, 185);
const wxColour COLOR_NOT_ACTIVE  (228, 228, 228); //light grey

const size_t ROW_COUNT_IF_NO_DATA = 0;

/*
class hierarchy:
                                GridDataBase
                                    /|\
                     ________________|________________
                    |                                |
               GridDataRim                           |
                   /|\                               |
          __________|__________                      |
         |                    |                      |
   GridDataLeft         GridDataRight          GridDataMiddle
*/



void refreshCell(Grid& grid, size_t row, ColumnType colType)
{
    wxRect cellArea = grid.getCellArea(row, colType); //returns empty rect if column not found; absolute coordinates!
    if (cellArea.height > 0)
    {
        cellArea.SetTopLeft(grid.CalcScrolledPosition(cellArea.GetTopLeft()));
        grid.getMainWin().RefreshRect(cellArea, false);
    }
}


std::pair<ptrdiff_t, ptrdiff_t> getVisibleRows(const Grid& grid) //returns range [from, to)
{
    const wxSize clientSize = grid.getMainWin().GetClientSize();
    if (clientSize.GetHeight() > 0)
    {
        wxPoint topLeft = grid.CalcUnscrolledPosition(wxPoint(0, 0));
        wxPoint bottom  = grid.CalcUnscrolledPosition(wxPoint(0, clientSize.GetHeight() - 1));

        const ptrdiff_t rowCount = grid.getRowCount();
        const ptrdiff_t rowFrom  = grid.getRowAtPos(topLeft.y); //return -1 for invalid position, rowCount if out of range
        if (rowFrom >= 0)
        {
            const ptrdiff_t rowTo = grid.getRowAtPos(bottom.y);
            if (0 <= rowTo && rowTo < rowCount)
                return std::make_pair(rowFrom, rowTo + 1);
            else
                return std::make_pair(rowFrom, rowCount);
        }
    }
    return std::make_pair(0, 0);
}


void fillBackgroundDefaultColorAlternating(wxDC& dc, const wxRect& rect, bool evenRowNumber)
{
    //alternate background color to improve readability (while lacking cell borders)
    if (!evenRowNumber)
    {
        //accessibility, support high-contrast schemes => work with user-defined background color!
        const auto backCol = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

        auto incChannel = [](unsigned char c, int diff) { return static_cast<unsigned char>(std::max(0, std::min(255, c + diff))); };

        auto getAdjustedColor = [&](int diff)
        {
            return wxColor(incChannel(backCol.Red  (), diff),
                           incChannel(backCol.Green(), diff),
                           incChannel(backCol.Blue (), diff));
        };

        auto colorDist = [](const wxColor& lhs, const wxColor& rhs) //just some metric
        {
            return numeric::power<2>(static_cast<int>(lhs.Red  ()) - static_cast<int>(rhs.Red  ())) +
                   numeric::power<2>(static_cast<int>(lhs.Green()) - static_cast<int>(rhs.Green())) +
                   numeric::power<2>(static_cast<int>(lhs.Blue ()) - static_cast<int>(rhs.Blue ()));
        };

        const int signLevel = colorDist(backCol, *wxBLACK) < colorDist(backCol, *wxWHITE) ? 1 : -1; //brighten or darken

        const wxColor colOutter = getAdjustedColor(signLevel * 14); //just some very faint gradient to avoid visual distraction
        const wxColor colInner  = getAdjustedColor(signLevel * 11); //

        //clearArea(dc, rect, backColAlt);

        //add some nice background gradient
        wxRect rectUpper = rect;
        rectUpper.height /= 2;
        wxRect rectLower = rect;
        rectLower.y += rectUpper.height;
        rectLower.height -= rectUpper.height;
        dc.GradientFillLinear(rectUpper, colOutter, colInner, wxSOUTH);
        dc.GradientFillLinear(rectLower, colOutter, colInner, wxNORTH);
    }
    else
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}


Zstring getExtension(const Zstring& shortName)
{
    return contains(shortName, Zchar('.')) ? afterLast(shortName, Zchar('.')) : Zstring();
};


class IconUpdater;
class GridEventManager;
class GridDataLeft;
class GridDataRight;

struct IconManager
{
    IconManager(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer::IconSize sz) :
        iconBuffer(sz),
        fileIcon       (IconBuffer::genericFileIcon(sz)),
        dirIcon        (IconBuffer::genericDirIcon (sz)),
        linkOverlayIcon(IconBuffer::linkOverlayIcon(sz)),
        iconUpdater(make_unique<IconUpdater>(provLeft, provRight, iconBuffer)) {}

    void startIconUpdater();
    IconBuffer& refIconBuffer() { return iconBuffer; }

    wxBitmap getGenericFileIcon() const { return fileIcon;        }
    wxBitmap getGenericDirIcon () const { return dirIcon;         }
    wxBitmap getLinkOverlayIcon() const { return linkOverlayIcon; }

private:
    IconBuffer iconBuffer;
    const wxBitmap fileIcon;
    const wxBitmap dirIcon;
    const wxBitmap linkOverlayIcon;

    std::unique_ptr<IconUpdater> iconUpdater; //bind ownership to GridDataRim<>!
};

//########################################################################################################

class GridDataBase : public GridData
{
public:
    GridDataBase(Grid& grid, const std::shared_ptr<const zen::GridView>& gridDataView) : grid_(grid), gridDataView_(gridDataView) {}

    void holdOwnership(const std::shared_ptr<GridEventManager>& evtMgr) { evtMgr_ = evtMgr; }
    GridEventManager* getEventManager() { return evtMgr_.get(); }

protected:
    Grid& refGrid() { return grid_; }
    const Grid& refGrid() const { return grid_; }

    const GridView* getGridDataView() const { return gridDataView_.get(); }

    const FileSystemObject* getRawData(size_t row) const
    {
        if (auto view = getGridDataView())
            return view->getObject(row);
        return nullptr;
    }

private:
    size_t getRowCount() const override
    {
        if (gridDataView_)
        {
            if (gridDataView_->rowsTotal() == 0)
                return ROW_COUNT_IF_NO_DATA;
            return gridDataView_->rowsOnView();
        }
        else
            return ROW_COUNT_IF_NO_DATA;

        //return std::max(MIN_ROW_COUNT, gridDataView_ ? gridDataView_->rowsOnView() : 0);
    }

    std::shared_ptr<GridEventManager> evtMgr_;
    Grid& grid_;
    std::shared_ptr<const GridView> gridDataView_;
};

//########################################################################################################

template <SelectedSide side>
class GridDataRim : public GridDataBase
{
public:
    GridDataRim(const std::shared_ptr<const zen::GridView>& gridDataView, Grid& grid) : GridDataBase(grid, gridDataView) {}

    void setIconManager(const std::shared_ptr<IconManager>& iconMgr) { iconMgr_ = iconMgr; }

    void updateNewAndGetUnbufferedIcons(std::vector<Zstring>& newLoad) //loads all not yet drawn icons
    {
        if (iconMgr_)
        {
            const auto& rowsOnScreen = getVisibleRows(refGrid());
            const ptrdiff_t visibleRowCount = rowsOnScreen.second - rowsOnScreen.first;

            //loop over all visible rows
            for (ptrdiff_t i = 0; i < visibleRowCount; ++i)
            {
                //alternate when adding rows: first, last, first + 1, last - 1 ...
                const ptrdiff_t currentRow = rowsOnScreen.first + getAlternatingPos(i, visibleRowCount);

                if (isFailedLoad(currentRow)) //find failed attempts to load icon
                {
                    const IconInfo ii = getIconInfo(currentRow);
                    if (!ii.iconPath.empty())
                    {
                        //test if they are already loaded in buffer:
                        if (iconMgr_->refIconBuffer().readyForRetrieval(ii.iconPath))
                        {
                            //do a *full* refresh for *every* failed load to update partial DC updates while scrolling
                            refreshCell(refGrid(), currentRow, static_cast<ColumnType>(COL_TYPE_FILENAME));
                            setFailedLoad(currentRow, false);
                        }
                        else //not yet in buffer: mark for async. loading
                            newLoad.push_back(ii.iconPath);
                    }
                }
            }
        }
    }

    void getUnbufferedIconsForPreload(std::vector<std::pair<ptrdiff_t, Zstring>>& newLoad) //return (priority, filepath) list
    {
        if (iconMgr_)
        {
            const auto& rowsOnScreen = getVisibleRows(refGrid());
            const ptrdiff_t visibleRowCount = rowsOnScreen.second - rowsOnScreen.first;

            //preload icons not yet on screen:
            const int preloadSize = 2 * std::max<ptrdiff_t>(20, visibleRowCount); //:= sum of lines above and below of visible range to preload
            //=> use full visible height to handle "next page" command and a minimum of 20 for excessive mouse wheel scrolls

            for (ptrdiff_t i = 0; i < preloadSize; ++i)
            {
                const ptrdiff_t currentRow = rowsOnScreen.first - (preloadSize + 1) / 2 + getAlternatingPos(i, visibleRowCount + preloadSize); //for odd preloadSize start one row earlier

                const IconInfo ii = getIconInfo(currentRow);
                if (!ii.iconPath.empty())
                    if (!iconMgr_->refIconBuffer().readyForRetrieval(ii.iconPath))
                        newLoad.emplace_back(i, ii.iconPath); //insert least-important items on outer rim first
            }
        }
    }

private:
    bool isFailedLoad(size_t row) const { return row < failedLoads.size() ? failedLoads[row] != 0 : false; }

    void setFailedLoad(size_t row, bool failed = true)
    {
        if (failedLoads.size() != refGrid().getRowCount())
            failedLoads.resize(refGrid().getRowCount());

        if (row < failedLoads.size())
            failedLoads[row] = failed;
    }

    //icon buffer will load reversely, i.e. if we want to go from inside out, we need to start from outside in
    static size_t getAlternatingPos(size_t pos, size_t total)
    {
        assert(pos < total);
        return pos % 2 == 0 ? pos / 2 : total - 1 - pos / 2;
    }

protected:
    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
            //ignore focus
            else
            {
                //alternate background color to improve readability (while lacking cell borders)
                if (getRowDisplayType(row) == DISP_TYPE_NORMAL)
                    fillBackgroundDefaultColorAlternating(dc, rect, row % 2 == 0);
                else
                    clearArea(dc, rect, getBackGroundColor(row));

                //draw horizontal border if required
                DisplayType dispTp = getRowDisplayType(row);
                if (dispTp != DISP_TYPE_NORMAL &&
                    dispTp == getRowDisplayType(row + 1))
                {
                    const wxColor colorGridLine = wxColour(192, 192, 192); //light grey
                    wxDCPenChanger dummy2(dc, wxPen(colorGridLine, 1, wxSOLID));
                    dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight() + wxPoint(1, 0));
                }
            }
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    wxColor getBackGroundColor(size_t row) const
    {
        //accessibility: always set both foreground AND background colors!
        // => harmonize with renderCell()!

        switch (getRowDisplayType(row))
        {
            case DISP_TYPE_NORMAL:
                break;
            case DISP_TYPE_FOLDER:
                return COLOR_GREY;
            case DISP_TYPE_SYMLINK:
                return COLOR_ORANGE;
            case DISP_TYPE_INACTIVE:
                return COLOR_NOT_ACTIVE;
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

private:
    enum DisplayType
    {
        DISP_TYPE_NORMAL,
        DISP_TYPE_FOLDER,
        DISP_TYPE_SYMLINK,
        DISP_TYPE_INACTIVE,
    };

    DisplayType getRowDisplayType(size_t row) const
    {
        const FileSystemObject* fsObj = getRawData(row);
        if (!fsObj )
            return DISP_TYPE_NORMAL;

        //mark filtered rows
        if (!fsObj->isActive())
            return DISP_TYPE_INACTIVE;

        if (fsObj->isEmpty<side>()) //always show not existing files/dirs/symlinks as empty
            return DISP_TYPE_NORMAL;

        DisplayType output = DISP_TYPE_NORMAL;
        //mark directories and symlinks
        struct GetRowType : public FSObjectVisitor
        {
            GetRowType(DisplayType& result) : result_(result) {}

            void visit(const FilePair&    fileObj) override {}
            void visit(const SymlinkPair& linkObj) override
            {
                result_ = DISP_TYPE_SYMLINK;
            }
            void visit(const DirPair& dirObj) override
            {
                result_ = DISP_TYPE_FOLDER;
            }
        private:
            DisplayType& result_;
        } getType(output);
        fsObj->accept(getType);
        return output;
    }

    wxString getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getRawData(row))
        {
            struct GetTextValue : public FSObjectVisitor
            {
                GetTextValue(ColumnTypeRim colType, const FileSystemObject& fso) : colType_(colType), fsObj_(fso) {}

                void visit(const FilePair& fileObj) override
                {
                    switch (colType_)
                    {
                        case COL_TYPE_FULL_PATH:
                            value = toWx(fileObj.getFullPath<side>());
                            break;
                        case COL_TYPE_FILENAME:
                            value = toWx(fileObj.getItemName<side>());
                            break;
                        case COL_TYPE_REL_FOLDER:
                            value = toWx(beforeLast(fileObj.getPairRelativePath(), FILE_NAME_SEPARATOR)); //returns empty string if ch not found
                            break;
                        case COL_TYPE_BASE_DIRECTORY:
                            value = toWx(fileObj.getBaseDirPf<side>());
                            break;
                        case COL_TYPE_SIZE:
                            if (!fsObj_.isEmpty<side>())
                                value = zen::toGuiString(fileObj.getFileSize<side>());

                            // -> test file id
                            //if (!fsObj_.isEmpty<side>())
                            //	value = toGuiString(fileObj.getFileId<side>().second) + L" " + toGuiString(fileObj.getFileId<side>().first);
                            break;
                        case COL_TYPE_DATE:
                            if (!fsObj_.isEmpty<side>())
                                value = zen::utcToLocalTimeString(fileObj.getLastWriteTime<side>());
                            break;
                        case COL_TYPE_EXTENSION:
                            value = toWx(getExtension(fileObj.getItemName<side>()));
                            break;
                    }
                }

                void visit(const SymlinkPair& linkObj) override
                {
                    switch (colType_)
                    {
                        case COL_TYPE_FULL_PATH:
                            value = toWx(linkObj.getFullPath<side>());
                            break;
                        case COL_TYPE_FILENAME:
                            value = toWx(linkObj.getItemName<side>());
                            break;
                        case COL_TYPE_REL_FOLDER:
                            value = toWx(beforeLast(linkObj.getPairRelativePath(), FILE_NAME_SEPARATOR)); //returns empty string if ch not found
                            break;
                        case COL_TYPE_BASE_DIRECTORY:
                            value = toWx(linkObj.getBaseDirPf<side>());
                            break;
                        case COL_TYPE_SIZE:
                            if (!fsObj_.isEmpty<side>())
                                value = L"<" + _("Symlink") + L">";
                            break;
                        case COL_TYPE_DATE:
                            if (!fsObj_.isEmpty<side>())
                                value = zen::utcToLocalTimeString(linkObj.getLastWriteTime<side>());
                            break;
                        case COL_TYPE_EXTENSION:
                            value = toWx(getExtension(linkObj.getItemName<side>()));
                            break;
                    }
                }

                void visit(const DirPair& dirObj) override
                {
                    switch (colType_)
                    {
                        case COL_TYPE_FULL_PATH:
                            value = toWx(dirObj.getFullPath<side>());
                            break;
                        case COL_TYPE_FILENAME:
                            value = toWx(dirObj.getItemName<side>());
                            break;
                        case COL_TYPE_REL_FOLDER:
                            value = toWx(beforeLast(dirObj.getPairRelativePath(), FILE_NAME_SEPARATOR)); //returns empty string if ch not found
                            break;
                        case COL_TYPE_BASE_DIRECTORY:
                            value = toWx(dirObj.getBaseDirPf<side>());
                            break;
                        case COL_TYPE_SIZE:
                            if (!fsObj_.isEmpty<side>())
                                value = L"<" + _("Folder") + L">";
                            break;
                        case COL_TYPE_DATE:
                            if (!fsObj_.isEmpty<side>())
                                value = wxEmptyString;
                            break;
                        case COL_TYPE_EXTENSION:
                            value = wxEmptyString;
                            break;
                    }
                }
                ColumnTypeRim colType_;
                wxString value;

                const FileSystemObject& fsObj_;
            } getVal(static_cast<ColumnTypeRim>(colType), *fsObj);
            fsObj->accept(getVal);
            return getVal.value;
        }
        //if data is not found:
        return wxEmptyString;
    }

    static const int GAP_SIZE = 2;

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected) override
    {
        wxRect rectTmp = rect;

        const bool isActive = [&]() -> bool
        {
            if (const FileSystemObject* fsObj = this->getRawData(row))
                return fsObj->isActive();
            return true;
        }();

        //draw file icon
        if (static_cast<ColumnTypeRim>(colType) == COL_TYPE_FILENAME &&
            iconMgr_)
        {
            rectTmp.x     += GAP_SIZE;
            rectTmp.width -= GAP_SIZE;

            const int iconSize = iconMgr_->refIconBuffer().getSize();
            if (rectTmp.GetWidth() >= iconSize)
            {
                //  Partitioning:
                //   __________________________
                //  | gap | icon | gap | text |
                //   --------------------------

                //whenever there's something new to render on screen, start up watching for failed icon drawing:
                //=> ideally it would suffice to start watching only when scrolling grid or showing new grid content, but this solution is more robust
                //and the icon updater will stop automatically when finished anyway
                //Note: it's not sufficient to start up on failed icon loads only, since we support prefetching of not yet visible rows!!!
                iconMgr_->startIconUpdater();

                const IconInfo ii = getIconInfo(row);

                wxBitmap fileIcon;
                if (ii.drawAsFolder)
                    fileIcon = iconMgr_->getGenericDirIcon();
                else if (!ii.iconPath.empty()) //retrieve file icon
                {
                    if (Opt<wxBitmap> tmpIco = iconMgr_->refIconBuffer().retrieveFileIcon(ii.iconPath))
                        fileIcon = *tmpIco;
                    else
                    {
                        fileIcon = iconMgr_->getGenericFileIcon(); //better than nothing
                        setFailedLoad(row); //save status of failed icon load -> used for async. icon loading
                        //falsify only! we want to avoid writing incorrect success values when only partially updating the DC, e.g. when scrolling,
                        //see repaint behavior of ::ScrollWindow() function!
                    }
                }

                if (fileIcon.IsOk())
                {
                    wxRect rectIcon = rectTmp;
                    rectIcon.width = iconSize; //support small thumbnail centering

                    auto drawIcon = [&](const wxBitmap& icon)
                    {
                        if (isActive)
                            drawBitmapRtlNoMirror(dc, icon, rectIcon, wxALIGN_CENTER, this->buffer); //without "this->" GCC 4.7.2 compiler crash on Debian
                        else
                            drawBitmapRtlNoMirror(dc, wxBitmap(icon.ConvertToImage().ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3)), //treat all channels equally!
                                                  rectIcon, wxALIGN_CENTER, this->buffer);
                    };

                    drawIcon(fileIcon);

                    if (ii.drawAsLink)
                        drawIcon(iconMgr_->getLinkOverlayIcon());
                }
            }
            rectTmp.x     += iconSize;
            rectTmp.width -= iconSize;
        }

        std::unique_ptr<wxDCTextColourChanger> dummy3;
        if (getRowDisplayType(row) != DISP_TYPE_NORMAL)
            dummy3 = make_unique<wxDCTextColourChanger>(dc, *wxBLACK); //accessibility: always set both foreground AND background colors!

        //draw text
        if (static_cast<ColumnTypeRim>(colType) == COL_TYPE_SIZE && refGrid().GetLayoutDirection() != wxLayout_RightToLeft)
        {
            //have file size right-justified (but don't change for RTL languages)
            rectTmp.width -= GAP_SIZE;
            drawCellText(dc, rectTmp, getValue(row, colType), isActive, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
        }
        else
        {
            rectTmp.x     += GAP_SIZE;
            rectTmp.width -= GAP_SIZE;
            drawCellText(dc, rectTmp, getValue(row, colType), isActive, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        //  Partitioning:
        //   ________________________________
        //  | gap | icon | gap | text | gap |
        //   --------------------------------

        int bestSize = 0;
        if (static_cast<ColumnTypeRim>(colType) == COL_TYPE_FILENAME && iconMgr_)
            bestSize += GAP_SIZE + iconMgr_->refIconBuffer().getSize();

        bestSize += GAP_SIZE + dc.GetTextExtent(getValue(row, colType)).GetWidth() + GAP_SIZE;

        return bestSize; // + 1 pix for cell border line -> not used anymore!
    }

    wxString getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeRim>(colType))
        {
            case COL_TYPE_FULL_PATH:
                return _("Full path");
            case COL_TYPE_FILENAME:
                return _("Name"); //= short name
            case COL_TYPE_REL_FOLDER:
                return _("Relative folder");
            case COL_TYPE_BASE_DIRECTORY:
                return _("Base folder");
            case COL_TYPE_SIZE:
                return _("Size");
            case COL_TYPE_DATE:
                return _("Date");
            case COL_TYPE_EXTENSION:
                return _("Extension");
        }
        return wxEmptyString;
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        wxRect rectInside = drawColumnLabelBorder(dc, rect);
        drawColumnLabelBackground(dc, rectInside, highlighted);

        rectInside.x     += COLUMN_GAP_LEFT;
        rectInside.width -= COLUMN_GAP_LEFT;
        drawColumnLabelText(dc, rectInside, getColumnLabel(colType));

        //draw sort marker
        if (getGridDataView())
        {
            auto sortInfo = getGridDataView()->getSortInfo();
            if (sortInfo)
            {
                if (colType == static_cast<ColumnType>(sortInfo->type_) && (side == LEFT_SIDE) == sortInfo->onLeft_)
                {
                    const wxBitmap& marker = getResourceImage(sortInfo->ascending_ ? L"sortAscending" : L"sortDescending");
                    wxPoint markerBegin = rectInside.GetTopLeft() + wxPoint((rectInside.width - marker.GetWidth()) / 2, 0);
                    dc.DrawBitmap(marker, markerBegin, true); //respect 2-pixel gap
                }
            }
        }
    }

    struct IconInfo
    {
        Zstring iconPath;  //mutually exclusive: either non-empty iconPath, or folder, or neither if no entry at this row
        bool drawAsFolder; //
        bool drawAsLink;
    };

    IconInfo getIconInfo(size_t row) const  //return ICON_FILE_FOLDER if row points to a folder
    {
        IconInfo out = {};

        const FileSystemObject* fsObj = getRawData(row);
        if (fsObj && !fsObj->isEmpty<side>())
        {
            struct GetIcon : public FSObjectVisitor
            {
                GetIcon(IconInfo& ii) : ii_(ii) {}

                void visit(const FilePair& fileObj) override
                {
                    ii_.iconPath = fileObj.getFullPath<side>();
                    ii_.drawAsLink = fileObj.isFollowedSymlink<side>() || hasLinkExtension(ii_.iconPath);
                }
                void visit(const SymlinkPair& linkObj) override
                {
                    ii_.iconPath = linkObj.getFullPath<side>();
                    ii_.drawAsLink = true;
                }
                void visit(const DirPair& dirObj) override
                {
                    ii_.drawAsFolder = true;
                    //todo: if ("is followed symlink") ii_.drawAsLink = true;
                }

                IconInfo& ii_;
            } getIcon(out);
            fsObj->accept(getIcon);
        }
        return out;
    }

    wxString getToolTip(size_t row, ColumnType colType) const override
    {
        wxString toolTip;

        if (const FileSystemObject* fsObj = getRawData(row))
            if (!fsObj->isEmpty<side>())
            {
                toolTip = toWx(getGridDataView() && getGridDataView()->getFolderPairCount() > 1 ?
                               fsObj->getFullPath<side>() :
                               fsObj->getRelativePath<side>());

                struct AssembleTooltip : public FSObjectVisitor
                {
                    AssembleTooltip(wxString& tipMsg) : tipMsg_(tipMsg) {}

                    void visit(const FilePair& fileObj) override
                    {
                        tipMsg_ += L"\n" +
                                   _("Size:") + L" " + zen::filesizeToShortString(fileObj.getFileSize<side>()) + L"\n" +
                                   _("Date:") + L" " + zen::utcToLocalTimeString(fileObj.getLastWriteTime<side>());
                    }

                    void visit(const SymlinkPair& linkObj) override
                    {
                        tipMsg_ += L"\n" +
                                   _("Date:") + L" " + zen::utcToLocalTimeString(linkObj.getLastWriteTime<side>());
                    }

                    void visit(const DirPair& dirObj) override {}

                    wxString& tipMsg_;
                } assembler(toolTip);
                fsObj->accept(assembler);
            }
        return toolTip;
    }

    std::shared_ptr<IconManager> iconMgr_; //optional


    std::vector<char> failedLoads; //effectively a vector<bool> of size "number of rows"
    std::unique_ptr<wxBitmap> buffer; //avoid costs of recreating this temporal variable
};


class GridDataLeft : public GridDataRim<LEFT_SIDE>
{
public:
    GridDataLeft(const std::shared_ptr<const zen::GridView>& gridDataView, Grid& grid) : GridDataRim<LEFT_SIDE>(gridDataView, grid) {}

    void setNavigationMarker(std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                             std::unordered_set<const HierarchyObject*>&& markedContainer)
    {
        markedFilesAndLinks_.swap(markedFilesAndLinks);
        markedContainer_    .swap(markedContainer);
    }

private:
    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        GridDataRim<LEFT_SIDE>::renderRowBackgound(dc, rect, row, enabled, selected);

        //mark rows selected on navigation grid:
        if (enabled && !selected)
        {
            const bool markRow = [&]() -> bool
            {
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (markedFilesAndLinks_.find(fsObj) != markedFilesAndLinks_.end()) //mark files/links directly
                        return true;

                    if (auto dirObj = dynamic_cast<const DirPair*>(fsObj))
                    {
                        if (markedContainer_.find(dirObj) != markedContainer_.end()) //mark directories which *are* the given HierarchyObject*
                            return true;
                    }

                    //mark all objects which have the HierarchyObject as *any* matching ancestor
                    const HierarchyObject* parent = &(fsObj->parent());
                    for (;;)
                    {
                        if (markedContainer_.find(parent) != markedContainer_.end())
                            return true;

                        if (auto dirObj = dynamic_cast<const DirPair*>(parent))
                            parent = &(dirObj->parent());
                        else
                            break;
                    }
                }
                return false;
            }();

            if (markRow)
            {
                //const wxColor COLOR_TREE_SELECTION_GRADIENT = wxColor(101, 148, 255); //H:158 S:255 V:178
                const wxColor COLOR_TREE_SELECTION_GRADIENT = getColorSelectionGradientFrom();

                wxRect rectTmp = rect;
                rectTmp.width /= 20;
                dc.GradientFillLinear(rectTmp, COLOR_TREE_SELECTION_GRADIENT, GridDataRim<LEFT_SIDE>::getBackGroundColor(row), wxEAST);
            }
        }
    }

    std::unordered_set<const FileSystemObject*> markedFilesAndLinks_; //mark files/symlinks directly within a container
    std::unordered_set<const HierarchyObject*> markedContainer_;       //mark full container including all child-objects
    //DO NOT DEREFERENCE!!!! NOT GUARANTEED TO BE VALID!!!
};


class GridDataRight : public GridDataRim<RIGHT_SIDE>
{
public:
    GridDataRight(const std::shared_ptr<const zen::GridView>& gridDataView, Grid& grid) : GridDataRim<RIGHT_SIDE>(gridDataView, grid) {}
};

//########################################################################################################

class GridDataMiddle : public GridDataBase
{
public:
    GridDataMiddle(const std::shared_ptr<const zen::GridView>& gridDataView, Grid& grid) :
        GridDataBase(grid, gridDataView),
        highlightSyncAction_(false),
        toolTip(grid), //tool tip must not live longer than grid!
        notch(getResourceImage(L"notch").ConvertToImage()) {}

    void onSelectBegin(const wxPoint& clientPos, size_t row, ColumnType colType)
    {
        if (row < refGrid().getRowCount())
        {
            refGrid().clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!
            dragSelection = make_unique<std::pair<size_t, BlockPosition>>(row, mousePosToBlock(clientPos, row, static_cast<ColumnTypeMiddle>(colType)));
            toolTip.hide(); //handle custom tooltip
        }
    }

    void onSelectEnd(size_t rowFirst, size_t rowLast) //we cannot reuse row from "onSelectBegin": if user is holding shift, this may now be in the middle of the range!
    {
        refGrid().clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!

        //issue custom event
        if (dragSelection)
        {
            if (rowFirst < rowLast && //may be empty? probably not in this context
                rowLast <= refGrid().getRowCount())
            {
                if (wxEvtHandler* evtHandler = refGrid().GetEventHandler())
                    switch (dragSelection->second)
                    {
                        case BLOCKPOS_CHECK_BOX:
                        {
                            const FileSystemObject* fsObj = getRawData(dragSelection->first);
                            const bool setIncluded = fsObj ? !fsObj->isActive() : true;

                            CheckRowsEvent evt(rowFirst, rowLast, setIncluded);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                        case BLOCKPOS_LEFT:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::LEFT);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                        case BLOCKPOS_MIDDLE:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::NONE);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                        case BLOCKPOS_RIGHT:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::RIGHT);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                    }
            }
            dragSelection.reset();
        }

        //update highlight and tooltip: on OS X no mouse movement event is generated after a mouse button click (unlike on Windows)
        wxPoint clientPos = refGrid().getMainWin().ScreenToClient(wxGetMousePosition());
        onMouseMovement(clientPos);
    }

    void onMouseMovement(const wxPoint& clientPos)
    {
        //manage block highlighting and custom tooltip
        if (!dragSelection)
        {
            auto refreshHighlight = [&](size_t row)
            {
                refreshCell(refGrid(), row, static_cast<ColumnType>(COL_TYPE_CHECKBOX));
                refreshCell(refGrid(), row, static_cast<ColumnType>(COL_TYPE_SYNC_ACTION));
            };

            const wxPoint& topLeftAbs = refGrid().CalcUnscrolledPosition(clientPos);
            const size_t row = refGrid().getRowAtPos(topLeftAbs.y); //return -1 for invalid position, rowCount if one past the end
            const Opt<ColumnType> ct = refGrid().getColumnAtPos(topLeftAbs.x);

            if (row < refGrid().getRowCount() && ct)
            {
                if (highlight) refreshHighlight(highlight->row_); //refresh old highlight

                highlight = make_unique<MouseHighlight>(row, mousePosToBlock(clientPos, row, static_cast<ColumnTypeMiddle>(*ct)));

                refreshHighlight(highlight->row_);

                //show custom tooltip
                if (refGrid().getMainWin().GetClientRect().Contains(clientPos)) //cursor might have moved outside visible client area
                    showToolTip(row, static_cast<ColumnTypeMiddle>(*ct), refGrid().getMainWin().ClientToScreen(clientPos));
            }
            else
                onMouseLeave();
        }
    }

    void onMouseLeave() //wxEVT_LEAVE_WINDOW does not respect mouse capture!
    {
        if (!dragSelection)
        {
            if (highlight)
            {
                refreshCell(refGrid(), highlight->row_, static_cast<ColumnType>(COL_TYPE_CHECKBOX));
                refreshCell(refGrid(), highlight->row_, static_cast<ColumnType>(COL_TYPE_SYNC_ACTION));
                highlight.reset();
            }
            toolTip.hide(); //handle custom tooltip
        }
    }

    void highlightSyncAction(bool value) { highlightSyncAction_ = value; }

private:
    wxString getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getRawData(row))
            switch (static_cast<ColumnTypeMiddle>(colType))
            {
                case COL_TYPE_CHECKBOX:
                    break;
                case COL_TYPE_CMP_CATEGORY:
                    return getSymbol(fsObj->getCategory());
                case COL_TYPE_SYNC_ACTION:
                    return getSymbol(fsObj->getSyncOperation());
            }
        return wxString();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
            else
            {
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (fsObj->isActive())
                        fillBackgroundDefaultColorAlternating(dc, rect, row % 2 == 0);
                    else
                        clearArea(dc, rect, COLOR_NOT_ACTIVE);
                }
                else
                    clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            }
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected) override
    {
        auto drawHighlightBackground = [&](const FileSystemObject& fsObj, const wxColor& col)
        {
            if (enabled && !selected && fsObj.isActive()) //coordinate with renderRowBackgound()!
                clearArea(dc, rect, col);
        };

        switch (static_cast<ColumnTypeMiddle>(colType))
        {
            case COL_TYPE_CHECKBOX:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    const bool          rowHighlighted = dragSelection ? row == dragSelection->first : highlight ? row == highlight->row_ : false;
                    const BlockPosition highlightBlock = dragSelection ? dragSelection->second       : highlight ? highlight->blockPos_ : BLOCKPOS_CHECK_BOX;

                    if (rowHighlighted && highlightBlock == BLOCKPOS_CHECK_BOX)
                        drawBitmapRtlMirror(dc, getResourceImage(fsObj->isActive() ? L"checkboxTrueFocus" : L"checkboxFalseFocus"), rect, wxALIGN_CENTER, buffer);
                    else //default
                        drawBitmapRtlMirror(dc, getResourceImage(fsObj->isActive() ? L"checkboxTrue"      : L"checkboxFalse"     ), rect, wxALIGN_CENTER, buffer);
                }
                break;

            case COL_TYPE_CMP_CATEGORY:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (!highlightSyncAction_)
                        drawHighlightBackground(*fsObj, getBackGroundColorCmpCategory(fsObj));

                    wxRect rectTmp = rect;
                    {
                        //draw notch on left side
                        if (notch.GetHeight() != rectTmp.GetHeight())
                            notch.Rescale(notch.GetWidth(), rectTmp.GetHeight());

                        //wxWidgets screws up again and has wxALIGN_RIGHT off by one pixel! -> use wxALIGN_LEFT instead
                        const wxRect rectNotch(rectTmp.x + rectTmp.width - notch.GetWidth(), rectTmp.y, notch.GetWidth(), rectTmp.height);
                        drawBitmapRtlMirror(dc, notch, rectNotch, wxALIGN_LEFT, buffer);
                        rectTmp.width -= notch.GetWidth();
                    }

                    if (!highlightSyncAction_)
                        drawBitmapRtlMirror(dc, getCmpResultImage(fsObj->getCategory()), rectTmp, wxALIGN_CENTER, buffer);
                    else if (fsObj->getCategory() != FILE_EQUAL) //don't show = in both middle columns
                        drawBitmapRtlMirror(dc, greyScale(getCmpResultImage(fsObj->getCategory())), rectTmp, wxALIGN_CENTER, buffer);
                }
                break;

            case COL_TYPE_SYNC_ACTION:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (highlightSyncAction_)
                        drawHighlightBackground(*fsObj, getBackGroundColorSyncAction(fsObj));

                    const bool          rowHighlighted = dragSelection ? row == dragSelection->first : highlight ? row == highlight->row_ : false;
                    const BlockPosition highlightBlock = dragSelection ? dragSelection->second       : highlight ? highlight->blockPos_ : BLOCKPOS_CHECK_BOX;

                    //synchronization preview
                    if (rowHighlighted && highlightBlock != BLOCKPOS_CHECK_BOX)
                        switch (highlightBlock)
                        {
                            case BLOCKPOS_CHECK_BOX:
                                break;
                            case BLOCKPOS_LEFT:
                                drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::LEFT)), rect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, buffer);
                                break;
                            case BLOCKPOS_MIDDLE:
                                drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::NONE)), rect, wxALIGN_CENTER, buffer);
                                break;
                            case BLOCKPOS_RIGHT:
                                drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::RIGHT)), rect, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, buffer);
                                break;
                        }
                    else //default
                    {
                        if (highlightSyncAction_)
                            drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->getSyncOperation()), rect, wxALIGN_CENTER, buffer);
                        else if (fsObj->getSyncOperation() != SO_EQUAL) //don't show = in both middle columns
                            drawBitmapRtlMirror(dc, greyScale(getSyncOpImage(fsObj->getSyncOperation())), rect, wxALIGN_CENTER, buffer);
                    }
                }
                break;
        }
    }

    wxString getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeMiddle>(colType))
        {
            case COL_TYPE_CHECKBOX:
                break;
            case COL_TYPE_CMP_CATEGORY:
                return _("Category") + L" (F10)";
            case COL_TYPE_SYNC_ACTION:
                return _("Action")   + L" (F10)";
        }
        return wxEmptyString;
    }

    wxString getToolTip(ColumnType colType) const override { return getColumnLabel(colType); }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        switch (static_cast<ColumnTypeMiddle>(colType))
        {
            case COL_TYPE_CHECKBOX:
                drawColumnLabelBackground(dc, rect, false);
                break;

            case COL_TYPE_CMP_CATEGORY:
            {
                wxRect rectInside = drawColumnLabelBorder(dc, rect);
                drawColumnLabelBackground(dc, rectInside, highlighted);

                const wxBitmap& cmpIcon = getResourceImage(L"compare_small");
                drawBitmapRtlNoMirror(dc, highlightSyncAction_ ? greyScale(cmpIcon) : cmpIcon, rectInside, wxALIGN_CENTER, buffer);
            }
            break;

            case COL_TYPE_SYNC_ACTION:
            {
                wxRect rectInside = drawColumnLabelBorder(dc, rect);
                drawColumnLabelBackground(dc, rectInside, highlighted);

                const wxBitmap& syncIcon = getResourceImage(L"sync_small");
                drawBitmapRtlNoMirror(dc, highlightSyncAction_ ? syncIcon : greyScale(syncIcon), rectInside, wxALIGN_CENTER, buffer);
            }
            break;
        }
    }

    static wxColor getBackGroundColorSyncAction(const FileSystemObject* fsObj)
    {
        if (fsObj)
        {
            if (!fsObj->isActive())
                return COLOR_NOT_ACTIVE;

            switch (fsObj->getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_DO_NOTHING:
                    return COLOR_NOT_ACTIVE;
                case SO_EQUAL:
                    break; //usually white

                case SO_CREATE_NEW_LEFT:
                case SO_OVERWRITE_LEFT:
                case SO_DELETE_LEFT:
                case SO_MOVE_LEFT_SOURCE:
                case SO_MOVE_LEFT_TARGET:
                case SO_COPY_METADATA_TO_LEFT:
                    return COLOR_SYNC_BLUE;

                case SO_CREATE_NEW_RIGHT:
                case SO_OVERWRITE_RIGHT:
                case SO_DELETE_RIGHT:
                case SO_MOVE_RIGHT_SOURCE:
                case SO_MOVE_RIGHT_TARGET:
                case SO_COPY_METADATA_TO_RIGHT:
                    return COLOR_SYNC_GREEN;

                case SO_UNRESOLVED_CONFLICT:
                    return COLOR_YELLOW;
            }
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

    static wxColor getBackGroundColorCmpCategory(const FileSystemObject* fsObj)
    {
        if (fsObj)
        {
            if (!fsObj->isActive())
                return COLOR_NOT_ACTIVE;

            switch (fsObj->getCategory())
            {
                case FILE_LEFT_SIDE_ONLY:
                case FILE_LEFT_NEWER:
                    return COLOR_SYNC_BLUE; //COLOR_CMP_BLUE;

                case FILE_RIGHT_SIDE_ONLY:
                case FILE_RIGHT_NEWER:
                    return COLOR_SYNC_GREEN; //COLOR_CMP_GREEN;

                case FILE_DIFFERENT_CONTENT:
                    return COLOR_CMP_RED;
                case FILE_EQUAL:
                    break; //usually white
                case FILE_CONFLICT:
                case FILE_DIFFERENT_METADATA: //= sub-category of equal, but hint via background that sync direction follows conflict-setting
                    return COLOR_YELLOW;
                    //return COLOR_YELLOW_LIGHT;
            }
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

    enum BlockPosition //each cell can be divided into four blocks concerning mouse selections
    {
        BLOCKPOS_CHECK_BOX,
        BLOCKPOS_LEFT,
        BLOCKPOS_MIDDLE,
        BLOCKPOS_RIGHT
    };

    //determine block position within cell
    BlockPosition mousePosToBlock(const wxPoint& clientPos, size_t row, ColumnTypeMiddle colType) const
    {
        switch (static_cast<ColumnTypeMiddle>(colType))
        {
            case COL_TYPE_CHECKBOX:
            case COL_TYPE_CMP_CATEGORY:
                break;

            case COL_TYPE_SYNC_ACTION:
            {
                const int absX = refGrid().CalcUnscrolledPosition(clientPos).x;

                const wxRect rect = refGrid().getCellArea(row, static_cast<ColumnType>(COL_TYPE_SYNC_ACTION)); //returns empty rect if column not found; absolute coordinates!
                if (rect.width > 0 && rect.height > 0)
                    if (const FileSystemObject* const fsObj = getRawData(row))
                        if (fsObj->getSyncOperation() != SO_EQUAL) //in sync-preview equal files shall be treated like a checkbox
                            // cell:
                            //  -----------------------
                            // | left | middle | right|
                            //  -----------------------
                            if (rect.GetX() <= absX)
                            {
                                if (absX < rect.GetX() + rect.GetWidth() / 3)
                                    return BLOCKPOS_LEFT;
                                else if (absX < rect.GetX() + 2 * rect.GetWidth() / 3)
                                    return BLOCKPOS_MIDDLE;
                                else if  (absX < rect.GetX() + rect.GetWidth())
                                    return BLOCKPOS_RIGHT;
                            }
            }
            break;
        }
        return BLOCKPOS_CHECK_BOX;
    }

    void showToolTip(size_t row, ColumnTypeMiddle colType, wxPoint posScreen)
    {
        if (const FileSystemObject* fsObj = getRawData(row))
        {
            bool showTooltipSyncAction = true;
            switch (colType)
            {
                case COL_TYPE_CHECKBOX:
                    showTooltipSyncAction = highlightSyncAction_;
                    break;
                case COL_TYPE_CMP_CATEGORY:
                    showTooltipSyncAction = false;
                    break;
                case COL_TYPE_SYNC_ACTION:
                    break;
            }

            if (showTooltipSyncAction) //synchronization preview
            {
                const wchar_t* imageName = [&]() -> const wchar_t*
                {
                    const SyncOperation syncOp = fsObj->getSyncOperation();
                    switch (syncOp)
                    {
                        case SO_CREATE_NEW_LEFT:
                            return L"so_create_left";
                        case SO_CREATE_NEW_RIGHT:
                            return L"so_create_right";
                        case SO_DELETE_LEFT:
                            return L"so_delete_left";
                        case SO_DELETE_RIGHT:
                            return L"so_delete_right";
                        case SO_MOVE_LEFT_SOURCE:
                            return L"so_move_left_source";
                        case SO_MOVE_LEFT_TARGET:
                            return L"so_move_left_target";
                        case SO_MOVE_RIGHT_SOURCE:
                            return L"so_move_right_source";
                        case SO_MOVE_RIGHT_TARGET:
                            return L"so_move_right_target";
                        case SO_OVERWRITE_LEFT:
                            return L"so_update_left";
                        case SO_OVERWRITE_RIGHT:
                            return L"so_update_right";
                        case SO_COPY_METADATA_TO_LEFT:
                            return L"so_move_left";
                        case SO_COPY_METADATA_TO_RIGHT:
                            return L"so_move_right";
                        case SO_DO_NOTHING:
                            return L"so_none";
                        case SO_EQUAL:
                            return L"cat_equal";
                        case SO_UNRESOLVED_CONFLICT:
                            return L"cat_conflict";
                    };
                    assert(false);
                    return L"";
                }();
                const auto& img = mirrorIfRtl(getResourceImage(imageName));
                toolTip.show(getSyncOpDescription(*fsObj), posScreen, &img);
            }
            else
            {
                const wchar_t* imageName = [&]() -> const wchar_t*
                {
                    const CompareFilesResult cmpRes = fsObj->getCategory();
                    switch (cmpRes)
                    {
                        case FILE_LEFT_SIDE_ONLY:
                            return L"cat_left_only";
                        case FILE_RIGHT_SIDE_ONLY:
                            return L"cat_right_only";
                        case FILE_LEFT_NEWER:
                            return L"cat_left_newer";
                        case FILE_RIGHT_NEWER:
                            return L"cat_right_newer";
                        case FILE_DIFFERENT_CONTENT:
                            return L"cat_different";
                        case FILE_EQUAL:
                        case FILE_DIFFERENT_METADATA: //= sub-category of equal
                            return L"cat_equal";
                        case FILE_CONFLICT:
                            return L"cat_conflict";
                    }
                    assert(false);
                    return L"";
                }();
                const auto& img = mirrorIfRtl(getResourceImage(imageName));
                toolTip.show(getCategoryDescription(*fsObj), posScreen, &img);
            }
        }
        else
            toolTip.hide(); //if invalid row...
    }

    bool highlightSyncAction_;

    struct MouseHighlight
    {
        MouseHighlight(size_t row, BlockPosition blockPos) : row_(row), blockPos_(blockPos) {}
        const size_t row_;
        const BlockPosition blockPos_;
    };
    std::unique_ptr<MouseHighlight> highlight; //current mouse highlight
    std::unique_ptr<std::pair<size_t, BlockPosition>> dragSelection; //(row, block); area clicked when beginning selection
    std::unique_ptr<wxBitmap> buffer; //avoid costs of recreating this temporal variable
    Tooltip toolTip;
    wxImage notch;
};

//########################################################################################################

const wxEventType EVENT_ALIGN_SCROLLBARS = wxNewEventType();

class GridEventManager : private wxEvtHandler
{
public:
    GridEventManager(Grid& gridL,
                     Grid& gridC,
                     Grid& gridR,
                     GridDataMiddle& provMiddle) :
        gridL_(gridL), gridC_(gridC), gridR_(gridR), scrollMaster(nullptr),
        provMiddle_(provMiddle),
        scrollbarUpdatePending(false)
    {
        gridL_.Connect(EVENT_GRID_COL_RESIZE, GridColumnResizeEventHandler(GridEventManager::onResizeColumnL), nullptr, this);
        gridR_.Connect(EVENT_GRID_COL_RESIZE, GridColumnResizeEventHandler(GridEventManager::onResizeColumnR), nullptr, this);

        gridL_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler  (GridEventManager::onKeyDownL), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler  (GridEventManager::onKeyDownC), nullptr, this);
        gridR_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler  (GridEventManager::onKeyDownR), nullptr, this);

        gridC_.getMainWin().Connect(wxEVT_MOTION,       wxMouseEventHandler(GridEventManager::onCenterMouseMovement), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(GridEventManager::onCenterMouseLeave   ), nullptr, this);

        gridC_.Connect(EVENT_GRID_MOUSE_LEFT_DOWN, GridClickEventHandler      (GridEventManager::onCenterSelectBegin), nullptr, this);
        gridC_.Connect(EVENT_GRID_SELECT_RANGE,    GridRangeSelectEventHandler(GridEventManager::onCenterSelectEnd  ), nullptr, this);

        //clear selection of other grid when selecting on
        gridL_.Connect(EVENT_GRID_SELECT_RANGE, GridRangeSelectEventHandler(GridEventManager::onGridSelectionL), nullptr, this);
        gridR_.Connect(EVENT_GRID_SELECT_RANGE, GridRangeSelectEventHandler(GridEventManager::onGridSelectionR), nullptr, this);

        //parallel grid scrolling: do NOT use DoPrepareDC() to align grids! GDI resource leak! Use regular paint event instead:
        gridL_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridL), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridC), nullptr, this);
        gridR_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridR), nullptr, this);

        auto connectGridAccess = [&](Grid& grid, wxObjectEventFunction func)
        {
            grid.Connect(wxEVT_SCROLLWIN_TOP,        func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_BOTTOM,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_LINEUP,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_LINEDOWN,   func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_PAGEUP,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_PAGEDOWN,   func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_THUMBTRACK, func, nullptr, this);
            //wxEVT_KILL_FOCUS -> there's no need to reset "scrollMaster"
            //wxEVT_SET_FOCUS -> not good enough:
            //e.g.: left grid has input, right grid is "scrollMaster" due to dragging scroll thumb via mouse.
            //=> Next keyboard input on left does *not* emit focus change event, but still "scrollMaster" needs to change
            //=> hook keyboard input instead of focus event:
            grid.getMainWin().Connect(wxEVT_CHAR,     func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_KEY_UP,   func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_KEY_DOWN, func, nullptr, this);
        };
        connectGridAccess(gridL_, wxEventHandler(GridEventManager::onGridAccessL)); //
        connectGridAccess(gridC_, wxEventHandler(GridEventManager::onGridAccessC)); //connect *after* onKeyDown() in order to receive callback *before*!!!
        connectGridAccess(gridR_, wxEventHandler(GridEventManager::onGridAccessR)); //

        Connect(EVENT_ALIGN_SCROLLBARS, wxEventHandler(GridEventManager::onAlignScrollBars), NULL, this);
    }

    ~GridEventManager() { assert(!scrollbarUpdatePending); }

    void setScrollMaster(const Grid& grid) { scrollMaster = &grid; }

private:
    void onCenterSelectBegin(GridClickEvent& event)
    {

        provMiddle_.onSelectBegin(event.GetPosition(), event.row_, event.colType_);
        event.Skip();
    }

    void onCenterSelectEnd(GridRangeSelectEvent& event)
    {
        if (event.positive_)
            provMiddle_.onSelectEnd(event.rowFirst_, event.rowLast_);
        event.Skip();
    }

    void onCenterMouseMovement(wxMouseEvent& event)
    {
        provMiddle_.onMouseMovement(event.GetPosition());
        event.Skip();
    }

    void onCenterMouseLeave(wxMouseEvent& event)
    {
        provMiddle_.onMouseLeave();
        event.Skip();
    }

    void onGridSelectionL(GridRangeSelectEvent& event) { onGridSelection(gridL_, gridR_); event.Skip(); }
    void onGridSelectionR(GridRangeSelectEvent& event) { onGridSelection(gridR_, gridL_); event.Skip(); }

    void onGridSelection(const Grid& grid, Grid& other)
    {
        if (!wxGetKeyState(WXK_CONTROL)) //clear other grid unless user is holding CTRL
            other.clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!
    }

    void onKeyDownL(wxKeyEvent& event) {  onKeyDown(event, gridL_); }
    void onKeyDownC(wxKeyEvent& event) {  onKeyDown(event, gridC_); }
    void onKeyDownR(wxKeyEvent& event) {  onKeyDown(event, gridR_); }

    void onKeyDown(wxKeyEvent& event, const Grid& grid)
    {
        int keyCode = event.GetKeyCode();
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
        {
            if (keyCode == WXK_LEFT)
                keyCode = WXK_RIGHT;
            else if (keyCode == WXK_RIGHT)
                keyCode = WXK_LEFT;
            else if (keyCode == WXK_NUMPAD_LEFT)
                keyCode = WXK_NUMPAD_RIGHT;
            else if (keyCode == WXK_NUMPAD_RIGHT)
                keyCode = WXK_NUMPAD_LEFT;
        }

        //skip middle component when navigating via keyboard
        const size_t row = grid.getGridCursor();

        if (event.ShiftDown())
            ;
        else if (event.ControlDown())
            ;
        else
            switch (keyCode)
            {
                case WXK_LEFT:
                case WXK_NUMPAD_LEFT:
                    gridL_.setGridCursor(row);
                    gridL_.SetFocus();
                    //since key event is likely originating from right grid, we need to set scrollMaster manually!
                    scrollMaster = &gridL_; //onKeyDown is called *after* onGridAccessL()!
                    return; //swallow event

                case WXK_RIGHT:
                case WXK_NUMPAD_RIGHT:
                    gridR_.setGridCursor(row);
                    gridR_.SetFocus();
                    scrollMaster = &gridR_;
                    return; //swallow event
            }

        event.Skip();
    }

    void onResizeColumnL(GridColumnResizeEvent& event) { resizeOtherSide(gridL_, gridR_, event.colType_, event.offset_); }
    void onResizeColumnR(GridColumnResizeEvent& event) { resizeOtherSide(gridR_, gridL_, event.colType_, event.offset_); }

    void resizeOtherSide(const Grid& src, Grid& trg, ColumnType type, int offset)
    {
        //find stretch factor of resized column: type is unique due to makeConsistent()!
        std::vector<Grid::ColumnAttribute> cfgSrc = src.getColumnConfig();
        auto it = std::find_if(cfgSrc.begin(), cfgSrc.end(), [&](Grid::ColumnAttribute& ca) { return ca.type_ == type; });
        if (it == cfgSrc.end())
            return;
        const int stretchSrc = it->stretch_;

        //we do not propagate resizings on stretched columns to the other side: awkward user experience
        if (stretchSrc > 0)
            return;

        //apply resized offset to other side, but only if stretch factors match!
        std::vector<Grid::ColumnAttribute> cfgTrg = trg.getColumnConfig();
        std::for_each(cfgTrg.begin(), cfgTrg.end(), [&](Grid::ColumnAttribute& ca)
        {
            if (ca.type_ == type && ca.stretch_ == stretchSrc)
                ca.offset_ = offset;
        });
        trg.setColumnConfig(cfgTrg);
    }

    void onGridAccessL(wxEvent& event) { scrollMaster = &gridL_; event.Skip(); }
    void onGridAccessC(wxEvent& event) { scrollMaster = &gridC_; event.Skip(); }
    void onGridAccessR(wxEvent& event) { scrollMaster = &gridR_; event.Skip(); }

    void onPaintGridL(wxEvent& event) { onPaintGrid(gridL_); event.Skip(); }
    void onPaintGridC(wxEvent& event) { onPaintGrid(gridC_); event.Skip(); }
    void onPaintGridR(wxEvent& event) { onPaintGrid(gridR_); event.Skip(); }

    void onPaintGrid(const Grid& grid)
    {
        //align scroll positions of all three grids *synchronously* during paint event! (wxGTK has visible delay when this is done asynchronously, no delay on Windows)

        //determine lead grid
        const Grid* lead = nullptr;
        Grid* follow1    = nullptr;
        Grid* follow2    = nullptr;
        auto setGrids = [&](const Grid& l, Grid& f1, Grid& f2) { lead = &l; follow1 = &f1; follow2 = &f2; };

        if (&gridC_ == scrollMaster)
            setGrids(gridC_, gridL_, gridR_);
        else if (&gridR_ == scrollMaster)
            setGrids(gridR_, gridL_, gridC_);
        else //default: left panel
            setGrids(gridL_, gridC_, gridR_);

        //align other grids only while repainting the lead grid to avoid scrolling and updating a grid at the same time!
        if (lead != &grid) return;

        auto scroll = [](Grid& target, int y) //support polling
        {
            //scroll vertically only - scrolling horizontally becomes annoying if left and right sides have different widths;
            //e.g. h-scroll on left would be undone when scrolling vertically on right which doesn't have a h-scrollbar
            int yOld = 0;
            target.GetViewStart(nullptr, &yOld);
            if (yOld != y)
                target.Scroll(-1, y); //empirical test Windows/Ubuntu: this call does NOT trigger a wxEVT_SCROLLWIN event, which would incorrectly set "scrollMaster" to "&target"!
        };
        int y = 0;
        lead->GetViewStart(nullptr, &y);
        scroll(*follow1, y);
        scroll(*follow2, y);

        //harmonize placement of horizontal scrollbar to avoid grids getting out of sync!
        //since this affects the grid that is currently repainted as well, we do work asynchronously!
        //avoids at least this problem: remaining graphics artifact when changing from Grid::SB_SHOW_ALWAYS to Grid::SB_SHOW_NEVER at location of old scrollbar (Windows only)

        //perf note: send one async event at most, else they may accumulate and create perf issues, see grid.cpp
        if (!scrollbarUpdatePending)
        {
            scrollbarUpdatePending = true;
            wxCommandEvent alignEvent(EVENT_ALIGN_SCROLLBARS);
            AddPendingEvent(alignEvent); //waits until next idle event - may take up to a second if the app is busy on wxGTK!
        }
    }

    void onAlignScrollBars(wxEvent& event)
    {
        ZEN_ON_SCOPE_EXIT(scrollbarUpdatePending = false);
        assert(scrollbarUpdatePending);

        auto needsHorizontalScrollbars = [](const Grid& grid) -> bool
        {
            const wxWindow& mainWin = grid.getMainWin();
            return mainWin.GetVirtualSize().GetWidth() > mainWin.GetClientSize().GetWidth();
            //assuming Grid::updateWindowSizes() does its job well, this should suffice!
            //CAVEAT: if horizontal and vertical scrollbar are circular dependent from each other
            //(h-scrollbar is shown due to v-scrollbar consuming horizontal width, ect...)
            //while in fact both are NOT needed, this special case results in a bogus need for scrollbars!
            //see https://sourceforge.net/tracker/?func=detail&aid=3514183&group_id=234430&atid=1093083
            // => since we're outside the Grid abstraction, we should not duplicate code to handle this special case as it seems to be insignificant
        };

        Grid::ScrollBarStatus sbStatusX = needsHorizontalScrollbars(gridL_) ||
                                          needsHorizontalScrollbars(gridR_) ?
                                          Grid::SB_SHOW_ALWAYS : Grid::SB_SHOW_NEVER;
        gridL_.showScrollBars(sbStatusX, Grid::SB_SHOW_NEVER);
        gridC_.showScrollBars(sbStatusX, Grid::SB_SHOW_NEVER);
        gridR_.showScrollBars(sbStatusX, Grid::SB_SHOW_AUTOMATIC);
    }

    Grid& gridL_;
    Grid& gridC_;
    Grid& gridR_;

    const Grid* scrollMaster; //for address check only; this needn't be the grid having focus!
    //e.g. mouse wheel events should set window under cursor as scrollMaster, but *not* change focus

    GridDataMiddle& provMiddle_;
    bool scrollbarUpdatePending;
};
}

//########################################################################################################

void gridview::init(Grid& gridLeft, Grid& gridCenter, Grid& gridRight, const std::shared_ptr<const zen::GridView>& gridDataView)
{
    auto provLeft_   = std::make_shared<GridDataLeft  >(gridDataView, gridLeft);
    auto provMiddle_ = std::make_shared<GridDataMiddle>(gridDataView, gridCenter);
    auto provRight_  = std::make_shared<GridDataRight >(gridDataView, gridRight);

    gridLeft  .setDataProvider(provLeft_);   //data providers reference grid =>
    gridCenter.setDataProvider(provMiddle_); //ownership must belong *exclusively* to grid!
    gridRight .setDataProvider(provRight_);

    auto evtMgr = std::make_shared<GridEventManager>(gridLeft, gridCenter, gridRight, *provMiddle_);
    provLeft_  ->holdOwnership(evtMgr);
    provMiddle_->holdOwnership(evtMgr);
    provRight_ ->holdOwnership(evtMgr);

    gridCenter.enableColumnMove  (false);
    gridCenter.enableColumnResize(false);

    gridCenter.showRowLabel(false);
    gridRight .showRowLabel(false);

    //gridLeft  .showScrollBars(Grid::SB_SHOW_AUTOMATIC, Grid::SB_SHOW_NEVER); -> redundant: configuration happens in GridEventManager::onAlignScrollBars()
    //gridCenter.showScrollBars(Grid::SB_SHOW_NEVER,     Grid::SB_SHOW_NEVER);

    const int widthCheckbox = getResourceImage(L"checkboxTrue").GetWidth() + 4 + getResourceImage(L"notch").GetWidth();
    const int widthCategory = 30;
    const int widthAction   = 45;
    gridCenter.SetSize(widthCategory + widthCheckbox + widthAction, -1);

    std::vector<Grid::ColumnAttribute> attribMiddle;
    attribMiddle.emplace_back(static_cast<ColumnType>(COL_TYPE_CHECKBOX    ), widthCheckbox, 0, true);
    attribMiddle.emplace_back(static_cast<ColumnType>(COL_TYPE_CMP_CATEGORY), widthCategory, 0, true);
    attribMiddle.emplace_back(static_cast<ColumnType>(COL_TYPE_SYNC_ACTION ), widthAction,   0, true);
    gridCenter.setColumnConfig(attribMiddle);
}


namespace
{
std::vector<ColumnAttributeRim> makeConsistent(const std::vector<ColumnAttributeRim>& attribs)
{
    std::set<ColumnTypeRim> usedTypes;

    std::vector<ColumnAttributeRim> output;
    //remove duplicates: required by GridEventManager::resizeOtherSide() to find corresponding column on other side
    std::copy_if(attribs.begin(), attribs.end(), std::back_inserter(output),
    [&](const ColumnAttributeRim& a) { return usedTypes.insert(a.type_).second; });

    //make sure each type is existing! -> should *only* be a problem if user manually messes with globalsettings.xml
    const auto& defAttr = getDefaultColumnAttributesLeft();
    std::copy_if(defAttr.begin(), defAttr.end(), std::back_inserter(output),
    [&](const ColumnAttributeRim& a) { return usedTypes.insert(a.type_).second; });

    return output;
}
}

std::vector<Grid::ColumnAttribute> gridview::convertConfig(const std::vector<ColumnAttributeRim>& attribs)
{
    const auto& attribClean = makeConsistent(attribs);

    std::vector<Grid::ColumnAttribute> output;
    std::transform(attribClean.begin(), attribClean.end(), std::back_inserter(output),
    [&](const ColumnAttributeRim& ca) { return Grid::ColumnAttribute(static_cast<ColumnType>(ca.type_), ca.offset_, ca.stretch_, ca.visible_); });

    return output;
}


std::vector<ColumnAttributeRim> gridview::convertConfig(const std::vector<Grid::ColumnAttribute>& attribs)
{
    std::vector<ColumnAttributeRim> output;

    std::transform(attribs.begin(), attribs.end(), std::back_inserter(output),
    [&](const Grid::ColumnAttribute& ca) { return ColumnAttributeRim(static_cast<ColumnTypeRim>(ca.type_), ca.offset_, ca.stretch_, ca.visible_); });

    return makeConsistent(output);
}


namespace
{
class IconUpdater : private wxEvtHandler //update file icons periodically: use SINGLE instance to coordinate left and right grids in parallel
{
public:
    IconUpdater(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer& iconBuffer) : provLeft_(provLeft), provRight_(provRight), iconBuffer_(iconBuffer)
    {
        timer.Connect(wxEVT_TIMER, wxEventHandler(IconUpdater::loadIconsAsynchronously), nullptr, this);
    }

    void start() { if (!timer.IsRunning()) timer.Start(100); } //timer interval in [ms]
    //don't check too often! give worker thread some time to fetch data

private:
    void stop() { if (timer.IsRunning()) timer.Stop(); }

    void loadIconsAsynchronously(wxEvent& event) //loads all (not yet) drawn icons
    {
        std::vector<std::pair<ptrdiff_t, Zstring>> prefetchLoad;
        provLeft_ .getUnbufferedIconsForPreload(prefetchLoad);
        provRight_.getUnbufferedIconsForPreload(prefetchLoad);

        //make sure least-important prefetch rows are inserted first into workload (=> processed last)
        typedef std::pair<ptrdiff_t, Zstring> Pft; //priority index nicely considers both grids at the same time!
        std::sort(prefetchLoad.begin(), prefetchLoad.end(), [](const Pft& lhs, const Pft& rhs) { return lhs.first < rhs.first; });

        //last inserted items are processed first in icon buffer:
        std::vector<Zstring> newLoad;
        for (const auto& item : prefetchLoad)
            newLoad.push_back(item.second);

        provRight_.updateNewAndGetUnbufferedIcons(newLoad);
        provLeft_ .updateNewAndGetUnbufferedIcons(newLoad);

        iconBuffer_.setWorkload(newLoad);

        if (newLoad.empty()) //let's only pay for iconupdater when needed
            stop();
    }

    GridDataLeft& provLeft_;
    GridDataRight& provRight_;
    IconBuffer& iconBuffer_;
    wxTimer timer;
};


//resolve circular linker dependencies
inline
void IconManager::startIconUpdater() { if (iconUpdater) iconUpdater->start(); }
}


void gridview::setupIcons(Grid& gridLeft, Grid& gridCenter, Grid& gridRight, bool show, IconBuffer::IconSize sz)
{
    auto* provLeft  = dynamic_cast<GridDataLeft*>(gridLeft .getDataProvider());
    auto* provRight = dynamic_cast<GridDataRight*>(gridRight.getDataProvider());

    if (provLeft && provRight)
    {
        int iconHeight = 0;
        if (show)
        {
            auto iconMgr = std::make_shared<IconManager>(*provLeft, *provRight, sz);
            provLeft ->setIconManager(iconMgr);
            provRight->setIconManager(iconMgr);
            iconHeight = iconMgr->refIconBuffer().getSize();
        }
        else
        {
            provLeft ->setIconManager(nullptr);
            provRight->setIconManager(nullptr);
            iconHeight = IconBuffer::getSize(IconBuffer::SIZE_SMALL);
        }

        const int newRowHeight = std::max(iconHeight, gridLeft.getMainWin().GetCharHeight()) + 1; //add some space

        gridLeft  .setRowHeight(newRowHeight);
        gridCenter.setRowHeight(newRowHeight);
        gridRight .setRowHeight(newRowHeight);
    }
    else
        assert(false);
}


void gridview::refresh(Grid& gridLeft, Grid& gridCenter, Grid& gridRight)
{
    gridLeft  .Refresh();
    gridCenter.Refresh();
    gridRight .Refresh();
}


void gridview::setScrollMaster(Grid& grid)
{
    if (auto prov = dynamic_cast<GridDataBase*>(grid.getDataProvider()))
        if (auto evtMgr = prov->getEventManager())
        {
            evtMgr->setScrollMaster(grid);
            return;
        }
    assert(false);
}


void gridview::setNavigationMarker(Grid& gridLeft,
                                   std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                                   std::unordered_set<const HierarchyObject*>&& markedContainer)
{
    if (auto provLeft = dynamic_cast<GridDataLeft*>(gridLeft.getDataProvider()))
        provLeft->setNavigationMarker(std::move(markedFilesAndLinks), std::move(markedContainer));
    else
        assert(false);
    gridLeft.Refresh();
}


void gridview::highlightSyncAction(Grid& gridCenter, bool value)
{
    if (auto provMiddle = dynamic_cast<GridDataMiddle*>(gridCenter.getDataProvider()))
        provMiddle->highlightSyncAction(value);
    else
        assert(false);
    gridCenter.Refresh();
}


wxBitmap zen::getSyncOpImage(SyncOperation syncOp)
{
    switch (syncOp) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            return getResourceImage(L"so_create_left_small");
        case SO_CREATE_NEW_RIGHT:
            return getResourceImage(L"so_create_right_small");
        case SO_DELETE_LEFT:
            return getResourceImage(L"so_delete_left_small");
        case SO_DELETE_RIGHT:
            return getResourceImage(L"so_delete_right_small");
        case SO_MOVE_LEFT_SOURCE:
            return getResourceImage(L"so_move_left_source_small");
        case SO_MOVE_LEFT_TARGET:
            return getResourceImage(L"so_move_left_target_small");
        case SO_MOVE_RIGHT_SOURCE:
            return getResourceImage(L"so_move_right_source_small");
        case SO_MOVE_RIGHT_TARGET:
            return getResourceImage(L"so_move_right_target_small");
        case SO_OVERWRITE_LEFT:
            return getResourceImage(L"so_update_left_small");
        case SO_OVERWRITE_RIGHT:
            return getResourceImage(L"so_update_right_small");
        case SO_COPY_METADATA_TO_LEFT:
            return getResourceImage(L"so_move_left_small");
        case SO_COPY_METADATA_TO_RIGHT:
            return getResourceImage(L"so_move_right_small");
        case SO_DO_NOTHING:
            return getResourceImage(L"so_none_small");
        case SO_EQUAL:
            return getResourceImage(L"cat_equal_small");
        case SO_UNRESOLVED_CONFLICT:
            return getResourceImage(L"cat_conflict_small");
    }
    return wxNullBitmap;
}


wxBitmap zen::getCmpResultImage(CompareFilesResult cmpResult)
{
    switch (cmpResult)
    {
        case FILE_LEFT_SIDE_ONLY:
            return getResourceImage(L"cat_left_only_small");
        case FILE_RIGHT_SIDE_ONLY:
            return getResourceImage(L"cat_right_only_small");
        case FILE_LEFT_NEWER:
            return getResourceImage(L"cat_left_newer_small");
        case FILE_RIGHT_NEWER:
            return getResourceImage(L"cat_right_newer_small");
        case FILE_DIFFERENT_CONTENT:
            return getResourceImage(L"cat_different_small");
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA: //= sub-category of equal
            return getResourceImage(L"cat_equal_small");
        case FILE_CONFLICT:
            return getResourceImage(L"cat_conflict_small");
    }
    return wxNullBitmap;
}
