// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef GENERIC_GRID_HEADER_83470213483173
#define GENERIC_GRID_HEADER_83470213483173

#include <memory>
#include <numeric>
#include <vector>
#include <wx/scrolwin.h>
#include <zen/basic_math.h>
#include <zen/optional.h>

//a user-friendly, extensible and high-performance grid control

namespace zen
{
typedef enum { DUMMY_COLUMN_TYPE = static_cast<unsigned int>(-1) } ColumnType;

//----- events ------------------------------------------------------------------------
extern const wxEventType EVENT_GRID_COL_LABEL_MOUSE_LEFT;  //generates: GridClickEvent
extern const wxEventType EVENT_GRID_COL_LABEL_MOUSE_RIGHT; //
extern const wxEventType EVENT_GRID_COL_RESIZE;   //generates: GridColumnResizeEvent

extern const wxEventType EVENT_GRID_MOUSE_LEFT_DOUBLE; //
extern const wxEventType EVENT_GRID_MOUSE_LEFT_DOWN;   //
extern const wxEventType EVENT_GRID_MOUSE_LEFT_UP;     //generates: GridClickEvent
extern const wxEventType EVENT_GRID_MOUSE_RIGHT_DOWN;  //
extern const wxEventType EVENT_GRID_MOUSE_RIGHT_UP;    //

extern const wxEventType EVENT_GRID_SELECT_RANGE; //generates: GridRangeSelectEvent
//NOTE: neither first nor second row need to match EVENT_GRID_MOUSE_LEFT_DOWN/EVENT_GRID_MOUSE_LEFT_UP: user holding SHIFT; moving out of window...

//example: wnd.Connect(EVENT_GRID_COL_LABEL_LEFT_CLICK, GridClickEventHandler(MyDlg::OnLeftClick), nullptr, this);

struct GridClickEvent : public wxMouseEvent
{
    GridClickEvent(wxEventType et, const wxMouseEvent& me, ptrdiff_t row, ColumnType colType) : wxMouseEvent(me), row_(row), colType_(colType) { SetEventType(et); }
    wxEvent* Clone() const override { return new GridClickEvent(*this); }

    const ptrdiff_t row_; //-1 for invalid position, >= rowCount if out of range
    const ColumnType colType_; //may be DUMMY_COLUMN_TYPE
};

struct GridColumnResizeEvent : public wxCommandEvent
{
    GridColumnResizeEvent(int offset, ColumnType colType) : wxCommandEvent(EVENT_GRID_COL_RESIZE), colType_(colType), offset_(offset) {}
    wxEvent* Clone() const override { return new GridColumnResizeEvent(*this); }

    const ColumnType colType_;
    const int        offset_;
};

struct GridRangeSelectEvent : public wxCommandEvent
{
    GridRangeSelectEvent(size_t rowFirst, size_t rowLast, bool positive) : wxCommandEvent(EVENT_GRID_SELECT_RANGE), positive_(positive), rowFirst_(rowFirst), rowLast_(rowLast) { assert(rowFirst <= rowLast); }
    wxEvent* Clone() const override { return new GridRangeSelectEvent(*this); }

    const bool   positive_; //"false" when clearing selection!
    const size_t rowFirst_; //selected range: [rowFirst_, rowLast_)
    const size_t rowLast_;
};

typedef void (wxEvtHandler::*GridClickEventFunction       )(GridClickEvent&);
typedef void (wxEvtHandler::*GridColumnResizeEventFunction)(GridColumnResizeEvent&);
typedef void (wxEvtHandler::*GridRangeSelectEventFunction )(GridRangeSelectEvent&);

#define GridClickEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridClickEventFunction, &func)

#define GridColumnResizeEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridColumnResizeEventFunction, &func)

#define GridRangeSelectEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridRangeSelectEventFunction, &func)
//------------------------------------------------------------------------------------------------------------
class Grid;
wxColor getColorSelectionGradientFrom();
wxColor getColorSelectionGradientTo();

void clearArea(wxDC& dc, const wxRect& rect, const wxColor& col);

class GridData
{
public:
    virtual ~GridData() {}

    virtual size_t getRowCount() const = 0;

    //grid area
    virtual wxString getValue(size_t row, ColumnType colType) const = 0;
    virtual void     renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row,                     bool enabled, bool selected); //default implementation
    virtual void     renderCell        (wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected); //
    virtual int      getBestSize       (wxDC& dc, size_t row, ColumnType colType                                                 ); //must correspond to renderCell()!
    virtual wxString getToolTip        (size_t row, ColumnType colType) const { return wxString(); }

    //label area
    virtual wxString getColumnLabel(ColumnType colType) const = 0;
    virtual void renderColumnLabel(Grid& grid, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted); //default implementation
    virtual wxString getToolTip(ColumnType colType) const { return wxString(); }

    static const int COLUMN_GAP_LEFT; //for left-aligned text

protected: //optional helper routines
    static wxRect drawCellBorder  (wxDC& dc, const wxRect& rect); //returns inner rectangle
    static void drawCellBackground(wxDC& dc, const wxRect& rect, bool enabled, bool selected, const wxColor& backgroundColor);
    static void drawCellText      (wxDC& dc, const wxRect& rect, const wxString& text, bool enabled, int alignment = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    static wxRect drawColumnLabelBorder  (wxDC& dc, const wxRect& rect); //returns inner rectangle
    static void drawColumnLabelBackground(wxDC& dc, const wxRect& rect, bool highlighted);
    static void drawColumnLabelText      (wxDC& dc, const wxRect& rect, const wxString& text);
};

enum GridEventPolicy
{
    ALLOW_GRID_EVENT,
    DENY_GRID_EVENT
};


class Grid : public wxScrolledWindow
{
public:
    Grid(wxWindow* parent,
         wxWindowID id        = wxID_ANY,
         const wxPoint& pos   = wxDefaultPosition,
         const wxSize& size   = wxDefaultSize,
         long style           = wxTAB_TRAVERSAL | wxNO_BORDER,
         const wxString& name = wxPanelNameStr);

    size_t getRowCount() const;

    void setRowHeight(int height);

    struct ColumnAttribute
    {
        ColumnAttribute(ColumnType type, int offset, int stretch, bool visible = true) : type_(type), visible_(visible), stretch_(std::max(stretch, 0)), offset_(offset) { assert(stretch >=0 ); }
        ColumnType type_;
        bool visible_;
        //first, client width is partitioned according to all available stretch factors, then "offset_" is added
        //universal model: a non-stretched column has stretch factor 0 with the "offset" becoming identical to final width!
        int stretch_; //>= 0
        int offset_;
    };

    void setColumnConfig(const std::vector<ColumnAttribute>& attr); //set column count + widths
    std::vector<ColumnAttribute> getColumnConfig() const;

    void setDataProvider(const std::shared_ptr<GridData>& dataView) { dataView_ = dataView; }
    /**/  GridData* getDataProvider()       { return dataView_.get(); }
    const GridData* getDataProvider() const { return dataView_.get(); }
    //-----------------------------------------------------------------------------

    void setColumnLabelHeight(int height);
    void showRowLabel(bool visible);

    enum ScrollBarStatus
    {
        SB_SHOW_AUTOMATIC,
        SB_SHOW_ALWAYS,
        SB_SHOW_NEVER,
    };
    //alternative until wxScrollHelper::ShowScrollbars() becomes available in wxWidgets 2.9
    void showScrollBars(ScrollBarStatus horizontal, ScrollBarStatus vertical);

    std::vector<size_t> getSelectedRows() const { return selection.get(); }
    void selectAllRows (GridEventPolicy rangeEventPolicy);
    void clearSelection(GridEventPolicy rangeEventPolicy); //turn off range selection event when calling this function in an event handler to avoid recursion!

    void scrollDelta(int deltaX, int deltaY); //in scroll units

    wxWindow& getCornerWin  ();
    wxWindow& getRowLabelWin();
    wxWindow& getColLabelWin();
    wxWindow& getMainWin    ();
    const wxWindow& getMainWin() const;

    ptrdiff_t getRowAtPos(int posY) const; //return -1 for invalid position, >= rowCount if out of range; absolute coordinates!
    Opt<ColumnType> getColumnAtPos(int posX) const;

    wxRect getCellArea(size_t row, ColumnType colType) const; //returns empty rect if column not found; absolute coordinates!

    void enableColumnMove  (bool value) { allowColumnMove   = value; }
    void enableColumnResize(bool value) { allowColumnResize = value; }

    void setGridCursor(size_t row); //set + show + select cursor (+ emit range selection event)
    size_t getGridCursor() const; //returns row

    void scrollTo(size_t row);

    void Refresh(bool eraseBackground = true, const wxRect* rect = nullptr) override;
    bool Enable( bool enable = true) override { Refresh(); return wxScrolledWindow::Enable(enable); }
    //############################################################################################################

private:
    void onPaintEvent(wxPaintEvent& event);
    void onEraseBackGround(wxEraseEvent& event) {} //[!]
    void onSizeEvent(wxSizeEvent& event) { updateWindowSizes(); event.Skip(); }
    void onKeyDown(wxKeyEvent& event);

    void updateWindowSizes(bool updateScrollbar = true);

    void selectWithCursor(ptrdiff_t row);
    void makeRowVisible(size_t row);

    void redirectRowLabelEvent(wxMouseEvent& event);

    wxSize GetSizeAvailableForScrollTarget(const wxSize& size) override; //required since wxWidgets 2.9 if SetTargetWindow() is used

#if defined ZEN_WIN || defined ZEN_MAC
    void SetScrollbar(int orientation, int position, int thumbSize, int range, bool refresh) override; //get rid of scrollbars, but preserve scrolling behavior!
#endif

    int getBestColumnSize(size_t col) const; //return -1 on error

    void autoSizeColumns(GridEventPolicy columnResizeEventPolicy);

    friend class GridData;
    class SubWindow;
    class CornerWin;
    class RowLabelWin;
    class ColLabelWin;
    class MainWin;

    class Selection
    {
    public:
        void init(size_t rowCount) { rowSelectionValue.resize(rowCount); clear(); }

        size_t size() const { return rowSelectionValue.size(); }

        std::vector<size_t> get() const
        {
            std::vector<size_t> selection;
            for (size_t row = 0; row < rowSelectionValue.size(); ++row)
                if (rowSelectionValue[row] != 0)
                    selection.push_back(row);
            return selection;
        }

        void selectAll() { selectRange(0, rowSelectionValue.size(), true); }
        void clear    () { selectRange(0, rowSelectionValue.size(), false); }

        bool isSelected(size_t row) const { return row < rowSelectionValue.size() ? rowSelectionValue[row] != 0 : false; }

        void selectRange(size_t rowFirst, size_t rowLast, bool positive = true) //select [rowFirst, rowLast), trims if required!
        {
            if (rowFirst <= rowLast)
            {
                numeric::clamp<size_t>(rowFirst, 0, rowSelectionValue.size());
                numeric::clamp<size_t>(rowLast,  0, rowSelectionValue.size());

                std::fill(rowSelectionValue.begin() + rowFirst, rowSelectionValue.begin() + rowLast, positive);
            }
            else assert(false);
        }

    private:
        std::vector<char> rowSelectionValue; //effectively a vector<bool> of size "number of rows"
    };

    struct VisibleColumn
    {
        VisibleColumn(ColumnType type, int offset, int stretch) : type_(type), stretch_(stretch), offset_(offset) {}
        ColumnType type_;
        int stretch_; //>= 0
        int offset_;
    };

    struct ColumnWidth
    {
        ColumnWidth(ColumnType type, int width) : type_(type), width_(width) {}
        ColumnType type_;
        int width_;
    };
    std::vector<ColumnWidth> getColWidths()                 const; //
    std::vector<ColumnWidth> getColWidths(int mainWinWidth) const; //evaluate stretched columns
    int                                   getColWidthsSum(int mainWinWidth) const;
    std::vector<int> getColStretchedWidths(int clientWidth) const; //final width = (normalized) (stretchedWidth + offset)

    Opt<int> getColWidth(size_t col) const
    {
        const auto& widths = getColWidths();
        if (col < widths.size())
            return widths[col].width_;
        return NoValue();
    }

    void setColumnWidth(int width, size_t col, GridEventPolicy columnResizeEventPolicy, bool notifyAsync = false);

    wxRect getColumnLabelArea(ColumnType colType) const; //returns empty rect if column not found

    void selectRangeAndNotify(ptrdiff_t rowFrom, ptrdiff_t rowTo, bool positive = true); //select inclusive range [rowFrom, rowTo] + notify event!

    bool isSelected(size_t row) const { return selection.isSelected(row); }

    struct ColAction
    {
        bool wantResize; //"!wantResize" means "move" or "single click"
        size_t col;
    };
    Opt<ColAction> clientPosToColumnAction(const wxPoint& pos) const;
    void moveColumn(size_t colFrom, size_t colTo);
    ptrdiff_t clientPosToMoveTargetColumn(const wxPoint& pos) const; //return < 0 on error

    Opt<ColumnType> colToType(size_t col) const;

    /*
    Visual layout:
        --------------------------------
        |CornerWin   | ColLabelWin     |
        |------------------------------|
        |RowLabelWin | MainWin         |
        |            |                 |
        --------------------------------
    */
    CornerWin*   cornerWin_;
    RowLabelWin* rowLabelWin_;
    ColLabelWin* colLabelWin_;
    MainWin*     mainWin_;

    ScrollBarStatus showScrollbarX;
    ScrollBarStatus showScrollbarY;

    int colLabelHeight;
    bool drawRowLabel;

    std::shared_ptr<GridData> dataView_;
    Selection selection;
    bool allowColumnMove;
    bool allowColumnResize;

    std::vector<VisibleColumn> visibleCols; //individual widths, type and total column count
    std::vector<ColumnAttribute> oldColAttributes; //visible + nonvisible columns; use for conversion in setColumnConfig()/getColumnConfig() *only*!

    size_t rowCountOld; //at the time of last Grid::Refresh()
};
}

#endif //GENERIC_GRID_HEADER_83470213483173
