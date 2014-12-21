// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "grid.h"
#include <cassert>
#include <set>
#include <wx/settings.h>
#include <wx/listbox.h>
#include <wx/tooltip.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <zen/tick_count.h>
#include <zen/string_tools.h>
#include <zen/scope_guard.h>
#include <zen/utf.h>
#include <zen/format_unit.h>
#include "dc.h"

#ifdef ZEN_LINUX
    #include <gtk/gtk.h>
#endif

using namespace zen;


wxColor zen::getColorSelectionGradientFrom() { return wxColor(137, 172, 255); } //blue: HSL: 158, 255, 196   HSV: 222, 0.46, 1
wxColor zen::getColorSelectionGradientTo  () { return wxColor(225, 234, 255); } //      HSL: 158, 255, 240   HSV: 222, 0.12, 1

void zen::clearArea(wxDC& dc, const wxRect& rect, const wxColor& col)
{
    wxDCPenChanger   dummy (dc, col);
    wxDCBrushChanger dummy2(dc, col);
    dc.DrawRectangle(rect);
}

const int GridData::COLUMN_GAP_LEFT = 4;

namespace
{
//------------ Grid Constants --------------------------------
const double MOUSE_DRAG_ACCELERATION = 1.5; //unit: [rows / (pixel * sec)] -> same value as Explorer!
const int DEFAULT_COL_LABEL_BORDER = 6; //top + bottom border in addition to label height
//const int COLUMN_LABEL_BORDER      = GridData::COLUMN_GAP_LEFT;
const int COLUMN_MOVE_DELAY        = 5;  //unit: [pixel] (from Explorer)
const int COLUMN_MIN_WIDTH         = 40; //only honored when resizing manually!
const int ROW_LABEL_BORDER         = 3;
const int COLUMN_RESIZE_TOLERANCE  = 6; //unit [pixel]
const int COLUMN_FILL_GAP_TOLERANCE = 10; //enlarge column to fill full width when resizing

const wxColor COLOR_LABEL_GRADIENT_FROM = *wxWHITE;
const wxColor COLOR_LABEL_GRADIENT_TO   = wxColour(200, 200, 200); //light grey

const wxColor COLOR_LABEL_GRADIENT_FROM_FOCUS = COLOR_LABEL_GRADIENT_FROM;
const wxColor COLOR_LABEL_GRADIENT_TO_FOCUS   = getColorSelectionGradientFrom();

const wxColor colorGridLine = wxColour(192, 192, 192); //light grey

const bool fillGapAfterColumns = true; //draw rows/column label to fill full window width; may become an instance variable some time?
}

//----------------------------------------------------------------------------------------------------------------
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_DOUBLE     = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_LABEL_MOUSE_LEFT  = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_LABEL_MOUSE_RIGHT = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_RESIZE            = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_DOWN       = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_UP         = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_RIGHT_DOWN      = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_RIGHT_UP        = wxNewEventType();
const wxEventType zen::EVENT_GRID_SELECT_RANGE          = wxNewEventType();
//----------------------------------------------------------------------------------------------------------------

void GridData::renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected)
{
    drawCellBackground(dc, rect, enabled, selected, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}


void GridData::renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected)
{
    wxRect rectTmp = drawCellBorder(dc, rect);

    rectTmp.x     += COLUMN_GAP_LEFT;
    rectTmp.width -= COLUMN_GAP_LEFT;
    drawCellText(dc, rectTmp, getValue(row, colType), true);
}


int GridData::getBestSize(wxDC& dc, size_t row, ColumnType colType)
{
    return dc.GetTextExtent(getValue(row, colType)).GetWidth() + 2 * COLUMN_GAP_LEFT + 1; //gap on left and right side + border
}


wxRect GridData::drawCellBorder(wxDC& dc, const wxRect& rect) //returns remaining rectangle
{
    wxDCPenChanger dummy2(dc, wxPen(colorGridLine, 1, wxSOLID));
    dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight());
    dc.DrawLine(rect.GetBottomRight(), rect.GetTopRight() + wxPoint(0, -1));

    return wxRect(rect.GetTopLeft(), wxSize(rect.width - 1, rect.height - 1));
}


void GridData::drawCellBackground(wxDC& dc, const wxRect& rect, bool enabled, bool selected, const wxColor& backgroundColor)
{
    if (enabled)
    {
        if (selected)
            dc.GradientFillLinear(rect, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
        else
            clearArea(dc, rect, backgroundColor);
    }
    else
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
}


namespace
{
const wchar_t ELLIPSIS = L'\u2026'; //...

template <class Function> inline
wxString getTruncatedText(const wxString& text, Function textFits)
{
    if (textFits(text))
        return text;

    //unlike Windows 7 Explorer, we truncate UTF-16 correctly: e.g. CJK-Ideogramm encodes to TWO wchar_t: utfCvrtTo<wxString>("\xf0\xa4\xbd\x9c");
    size_t low  = 0; //number of unicode chars!
    size_t high = unicodeLength(text); //

    for (;;)
    {
        const size_t middle = (low + high) / 2;

        wxString candidate(strBegin(text), findUnicodePos(text, middle));
        candidate += ELLIPSIS;

        if (high - low <= 1)
            return candidate;

        if (textFits(candidate))
            low = middle;
        else
            high = middle;
    }
}

void drawTextLabelFitting(wxDC& dc, const wxString& text, const wxRect& rect, int alignment)
{
    RecursiveDcClipper clip(dc, rect); //wxDC::DrawLabel doesn't care about width, WTF?

    /*
    performance notes:
    wxDC::DrawLabel() is implemented in terms of both wxDC::GetMultiLineTextExtent() and wxDC::DrawText()
    wxDC::GetMultiLineTextExtent() is implemented in terms of wxDC::GetTextExtent()

    average total times:
    							Windows Linux
    single wxDC::DrawText()		 7탎     50탎
    wxDC::DrawLabel() +			10탎     90탎
    repeated GetTextExtent()
    */

    //truncate large texts and add ellipsis
    auto textFits = [&](const wxString& phrase) { return dc.GetTextExtent(phrase).GetWidth() <= rect.GetWidth(); };
    dc.DrawLabel(getTruncatedText(text, textFits), rect, alignment);
}
}


void GridData::drawCellText(wxDC& dc, const wxRect& rect, const wxString& text, bool enabled, int alignment)
{
    wxDCTextColourChanger dummy(dc, enabled ? dc.GetTextForeground() : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    drawTextLabelFitting(dc, text, rect, alignment);
}


void GridData::renderColumnLabel(Grid& grid, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted)
{
    wxRect rectTmp = drawColumnLabelBorder(dc, rect);
    drawColumnLabelBackground(dc, rectTmp, highlighted);

    rectTmp.x     += COLUMN_GAP_LEFT;
    rectTmp.width -= COLUMN_GAP_LEFT;
    drawColumnLabelText(dc, rectTmp, getColumnLabel(colType));
}


wxRect GridData::drawColumnLabelBorder(wxDC& dc, const wxRect& rect) //returns remaining rectangle
{
    //draw white line
    {
        wxDCPenChanger dummy(dc, *wxWHITE_PEN);
        dc.DrawLine(rect.GetTopLeft(), rect.GetBottomLeft());
    }

    //draw border (with gradient)
    {
        wxDCPenChanger dummy(dc, wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW), 1, wxSOLID));
        dc.GradientFillLinear(wxRect(rect.GetTopRight(), rect.GetBottomRight()), COLOR_LABEL_GRADIENT_FROM, dc.GetPen().GetColour(), wxSOUTH);
        dc.DrawLine(rect.GetBottomLeft(), rect.GetBottomRight() + wxPoint(1, 0));
    }

    return wxRect(rect.x + 1, rect.y, rect.width - 2, rect.height - 1); //we really don't like wxRect::Deflate, do we?
}


void GridData::drawColumnLabelBackground(wxDC& dc, const wxRect& rect, bool highlighted)
{
    if (highlighted)
        dc.GradientFillLinear(rect, COLOR_LABEL_GRADIENT_FROM_FOCUS, COLOR_LABEL_GRADIENT_TO_FOCUS, wxSOUTH);
    else //regular background gradient
        dc.GradientFillLinear(rect, COLOR_LABEL_GRADIENT_FROM, COLOR_LABEL_GRADIENT_TO, wxSOUTH); //clear overlapping cells
}


void GridData::drawColumnLabelText(wxDC& dc, const wxRect& rect, const wxString& text)
{
    wxDCTextColourChanger dummy(dc, *wxBLACK); //accessibility: always set both foreground AND background colors!
    drawTextLabelFitting(dc, text, rect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
}

//----------------------------------------------------------------------------------------------------------------
/*
                  SubWindow
                     /|\
                      |
     -----------------------------------
    |            |           |          |
CornerWin  RowLabelWin  ColLabelWin  MainWin

*/
class Grid::SubWindow : public wxWindow
{
public:
    SubWindow(Grid& parent) :
        wxWindow(&parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE, wxPanelNameStr),
        parent_(parent)
    {
        Connect(wxEVT_PAINT, wxPaintEventHandler(SubWindow::onPaintEvent), nullptr, this);
        Connect(wxEVT_SIZE,  wxSizeEventHandler (SubWindow::onSizeEvent),  nullptr, this);
        //http://wiki.wxwidgets.org/Flicker-Free_Drawing
        Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(SubWindow::onEraseBackGround), nullptr, this);

        //SetDoubleBuffered(true); slow as hell!

        SetBackgroundStyle(wxBG_STYLE_PAINT);

        Connect(wxEVT_SET_FOCUS,  wxFocusEventHandler(SubWindow::onFocus), nullptr, this);
        Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(SubWindow::onFocus), nullptr, this);
        Connect(wxEVT_CHILD_FOCUS, wxEventHandler(SubWindow::onChildFocus), nullptr, this);

        Connect(wxEVT_LEFT_DOWN,    wxMouseEventHandler(SubWindow::onMouseLeftDown  ), nullptr, this);
        Connect(wxEVT_LEFT_UP,      wxMouseEventHandler(SubWindow::onMouseLeftUp    ), nullptr, this);
        Connect(wxEVT_LEFT_DCLICK,  wxMouseEventHandler(SubWindow::onMouseLeftDouble), nullptr, this);
        Connect(wxEVT_RIGHT_DOWN,   wxMouseEventHandler(SubWindow::onMouseRightDown ), nullptr, this);
        Connect(wxEVT_RIGHT_UP,     wxMouseEventHandler(SubWindow::onMouseRightUp   ), nullptr, this);
        Connect(wxEVT_MOTION,       wxMouseEventHandler(SubWindow::onMouseMovement  ), nullptr, this);
        Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(SubWindow::onLeaveWindow    ), nullptr, this);
        Connect(wxEVT_MOUSEWHEEL,   wxMouseEventHandler(SubWindow::onMouseWheel     ), nullptr, this);
        Connect(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(SubWindow::onMouseCaptureLost), nullptr, this);

        Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(SubWindow::onKeyDown), nullptr, this);

        assert(GetClientAreaOrigin() == wxPoint()); //generally assumed when dealing with coordinates below
    }
    Grid& refParent() { return parent_; }
    const Grid& refParent() const { return parent_; }

    template <class T>
    bool sendEventNow(T&& event) //take both "rvalue + lvalues", return "true" if a suitable event handler function was found and executed, and the function did not call wxEvent::Skip.
    {
        if (wxEvtHandler* evtHandler = parent_.GetEventHandler())
            return evtHandler->ProcessEvent(event);
        return false;
    }

protected:
    void setToolTip(const wxString& text) //proper fix for wxWindow
    {
        wxToolTip* tt = GetToolTip();

        const wxString oldText = tt ? tt->GetTip() : wxString();
        if (text != oldText)
        {
            if (text.IsEmpty())
                SetToolTip(nullptr); //wxGTK doesn't allow wxToolTip with empty text!
            else
            {
                //wxWidgets bug: tooltip multiline property is defined by first tooltip text containing newlines or not (same is true for maximum width)
                if (!tt)
                    SetToolTip(new wxToolTip(L"a                                                                b\n\
                                                           a                                                                b")); //ugly, but is working (on Windows)
                tt = GetToolTip(); //should be bound by now
                assert(tt);
                if (tt)
                    tt->SetTip(text);
            }
        }
    }

private:
    virtual void render(wxDC& dc, const wxRect& rect) = 0;

    virtual void onFocus(wxFocusEvent& event) { event.Skip(); }
    virtual void onChildFocus(wxEvent& event) {} //wxGTK::wxScrolledWindow automatically scrolls to child window when child gets focus -> prevent!

    virtual void onMouseLeftDown  (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftUp    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftDouble(wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightDown (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightUp   (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseMovement  (wxMouseEvent& event) { event.Skip(); }
    virtual void onLeaveWindow    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseCaptureLost(wxMouseCaptureLostEvent& event) { event.Skip(); }

    void onKeyDown(wxKeyEvent& event)
    {
        if (!sendEventNow(event)) //let parent collect all key events
            event.Skip();
    }

    void onMouseWheel(wxMouseEvent& event)
    {
        /*
          MSDN, WM_MOUSEWHEEL: "Sent to the focus window when the mouse wheel is rotated.
          The DefWindowProc function propagates the message to the window's parent.
          There should be no internal forwarding of the message, since DefWindowProc propagates
          it up the parent chain until it finds a window that processes it."

          On OS X there is no such propagation! => we need a redirection (the same wxGrid implements)
         */

        //new wxWidgets 3.0 screw-up for GTK2: wxScrollHelperEvtHandler::ProcessEvent() ignores wxEVT_MOUSEWHEEL events
        //thereby breaking the scenario of redirection to parent we need here (but also breaking their very own wxGrid sample)
        //=> call wxScrolledWindow mouse wheel handler directly
        parent_.HandleOnMouseWheel(event);

        //if (!sendEventNow(event))
        //   event.Skip();
    }

    void onPaintEvent(wxPaintEvent& event)
    {
        //wxAutoBufferedPaintDC dc(this); -> this one happily fucks up for RTL layout by not drawing the first column (x = 0)!
        BufferedPaintDC dc(*this, doubleBuffer);

        assert(GetSize() == GetClientSize());

        const wxRegion& updateReg = GetUpdateRegion();
        for (wxRegionIterator it = updateReg; it; ++it)
            render(dc, it.GetRect());
    }

    void onSizeEvent(wxSizeEvent& event)
    {
        Refresh();
        event.Skip();
    }

    void onEraseBackGround(wxEraseEvent& event) {}

    Grid& parent_;
    std::unique_ptr<wxBitmap> doubleBuffer;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::CornerWin : public SubWindow
{
public:
    CornerWin(Grid& parent) : SubWindow(parent) {}

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        const wxRect& clientRect = GetClientRect();

        dc.GradientFillLinear(clientRect, COLOR_LABEL_GRADIENT_FROM, COLOR_LABEL_GRADIENT_TO, wxSOUTH);

        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW), 1, wxSOLID));

        {
            wxDCPenChanger dummy(dc, COLOR_LABEL_GRADIENT_FROM);
            dc.DrawLine(clientRect.GetTopLeft(), clientRect.GetTopRight());
        }

        dc.GradientFillLinear(wxRect(clientRect.GetBottomLeft (), clientRect.GetTopLeft ()), COLOR_LABEL_GRADIENT_FROM, dc.GetPen().GetColour(), wxSOUTH);
        dc.GradientFillLinear(wxRect(clientRect.GetBottomRight(), clientRect.GetTopRight()), COLOR_LABEL_GRADIENT_FROM, dc.GetPen().GetColour(), wxSOUTH);

        dc.DrawLine(clientRect.GetBottomLeft(), clientRect.GetBottomRight());

        wxRect rectShrinked = clientRect;
        rectShrinked.Deflate(1);
        dc.SetPen(*wxWHITE_PEN);

        //dc.DrawLine(clientRect.GetTopLeft(), clientRect.GetTopRight() + wxPoint(1, 0));
        dc.DrawLine(rectShrinked.GetTopLeft(), rectShrinked.GetBottomLeft() + wxPoint(0, 1));
    }
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::RowLabelWin : public SubWindow
{
public:
    RowLabelWin(Grid& parent) :
        SubWindow(parent),
        rowHeight(parent.GetCharHeight() + 2 + 1) {} //default height; don't call any functions on "parent" other than those from wxWindow during construction!
    //2 for some more space, 1 for bottom border (gives 15 + 2 + 1 on Windows, 17 + 2 + 1 on Ubuntu)

    int getBestWidth(ptrdiff_t rowFrom, ptrdiff_t rowTo)
    {
        wxClientDC dc(this);

        wxFont labelFont = GetFont();
        //labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont); //harmonize with RowLabelWin::render()!

        int bestWidth = 0;
        for (ptrdiff_t i = rowFrom; i <= rowTo; ++i)
            bestWidth = std::max(bestWidth, dc.GetTextExtent(formatRow(i)).GetWidth() + 2 * ROW_LABEL_BORDER);
        return bestWidth;
    }

    size_t getLogicalHeight() const { return refParent().getRowCount() * rowHeight; }

    ptrdiff_t getRowAtPos(ptrdiff_t posY) const //returns < 0 on invalid input, else row number within: [0, rowCount]; rowCount if out of range
    {
        if (posY >= 0 && rowHeight > 0)
        {
            const size_t row = posY / rowHeight;
            return std::min(row, refParent().getRowCount());
        }
        return -1;
    }

    int getRowHeight() const { return std::max(1, rowHeight); } //guarantees to return size >= 1 !
    void setRowHeight(int height) { rowHeight = height; }

    wxRect getRowLabelArea(ptrdiff_t row) const
    {
        assert(GetClientAreaOrigin() == wxPoint());
        return wxRect(wxPoint(0, rowHeight * row),
                      wxSize(GetClientSize().GetWidth(), rowHeight));
    }

    std::pair<ptrdiff_t, ptrdiff_t> getRowsOnClient(const wxRect& clientRect) const //returns range [begin, end)
    {
        const int yFrom = refParent().CalcUnscrolledPosition(clientRect.GetTopLeft    ()).y;
        const int yTo   = refParent().CalcUnscrolledPosition(clientRect.GetBottomRight()).y;

        return std::make_pair(std::max(yFrom / rowHeight, 0),
                              std::min<ptrdiff_t>((yTo  / rowHeight) + 1, refParent().getRowCount()));
    }

private:
    static wxString formatRow(size_t row) { return toGuiString(row + 1); } //convert number to std::wstring including thousands separator

    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {

        /*
        IsEnabled() vs IsThisEnabled() since wxWidgets 2.9.5:

        void wxWindowBase::NotifyWindowOnEnableChange(), called from bool wxWindowBase::Enable(), has this buggy exception of NOT
        refreshing child elements when disabling a IsTopLevel() dialog, e.g. when showing a modal dialog.
        The unfortunate effect on XP for using IsEnabled() when rendering the grid is that the user can move the modal dialog
        and *draw* with it on the background while the grid refreshes as disabled incrementally!

        => Don't use IsEnabled() since it considers the top level window. The brittle wxWidgets implementation is right in their intention,
        but wrong when not refreshing child-windows: the control designer decides how his control should be rendered!

        => IsThisEnabled() OTOH is too shallow and does not consider parent windows which are not top level.

        The perfect solution would be a bool ShouldBeDrawnActive() { return "IsEnabled() but ignore effects of showing a modal dialog"; }

        However "IsThisEnabled()" is good enough (same like the old IsEnabled() on wxWidgets 2.8.12) and it avoids this pathetic behavior on XP.
        (Similar problem on Win 7: e.g. directly click sync button without comparing first)
        */
        if (IsThisEnabled())
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

        wxFont labelFont = GetFont();
        //labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont); //harmonize with RowLabelWin::getBestWidth()!

        auto rowRange = getRowsOnClient(rect); //returns range [begin, end)
        for (auto row = rowRange.first; row < rowRange.second; ++row)
        {
            wxRect singleLabelArea = getRowLabelArea(row);
            if (singleLabelArea.GetHeight() > 0)
            {
                singleLabelArea.y = refParent().CalcScrolledPosition(singleLabelArea.GetTopLeft()).y;
                drawRowLabel(dc, singleLabelArea, row);
            }
        }
    }

    void drawRowLabel(wxDC& dc, const wxRect& rect, size_t row)
    {
        //clearArea(dc, rect, getColorRowLabel());
        dc.GradientFillLinear(rect, COLOR_LABEL_GRADIENT_FROM, COLOR_LABEL_GRADIENT_TO, wxEAST); //clear overlapping cells
        wxDCTextColourChanger dummy3(dc, *wxBLACK); //accessibility: always set both foreground AND background colors!

        //label text
        wxRect textRect = rect;
        textRect.Deflate(1);
        {
            RecursiveDcClipper clip(dc, textRect); //wxDC::DrawLabel doesn't care about with, WTF?
            dc.DrawLabel(formatRow(row), textRect, wxALIGN_CENTRE);
        }

        //border lines
        {
            wxDCPenChanger dummy(dc, *wxWHITE_PEN);
            dc.DrawLine(rect.GetTopLeft(), rect.GetTopRight());
        }
        {
            wxDCPenChanger dummy(dc, wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW), 1, wxSOLID));
            dc.DrawLine(rect.GetTopLeft(),     rect.GetBottomLeft());
            dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight());
            dc.DrawLine(rect.GetBottomRight(), rect.GetTopRight() + wxPoint(0, -1));
        }
    }

    void onMouseLeftDown(wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }
    void onMouseMovement(wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }
    void onMouseLeftUp  (wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }

    int rowHeight;
};


namespace
{
class ColumnResizing
{
public:
    ColumnResizing(wxWindow& wnd, size_t col, int startWidth, int clientPosX) :
        wnd_(wnd), col_(col), startWidth_(startWidth), clientPosX_(clientPosX)
    {
        wnd_.CaptureMouse();
    }
    ~ColumnResizing()
    {
        if (wnd_.HasCapture())
            wnd_.ReleaseMouse();
    }

    size_t getColumn      () const { return col_; }
    int    getStartWidth  () const { return startWidth_; }
    int    getStartPosX   () const { return clientPosX_; }

private:
    wxWindow& wnd_;
    const size_t col_;
    const int    startWidth_;
    const int    clientPosX_;
};


class ColumnMove
{
public:
    ColumnMove(wxWindow& wnd, size_t colFrom, int clientPosX) :
        wnd_(wnd),
        colFrom_(colFrom),
        colTo_(colFrom),
        clientPosX_(clientPosX),
        singleClick_(true) { wnd_.CaptureMouse(); }
    ~ColumnMove() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

    size_t  getColumnFrom() const { return colFrom_; }
    size_t& refColumnTo() { return colTo_; }
    int     getStartPosX () const { return clientPosX_; }

    bool isRealMove() const { return !singleClick_; }
    void setRealMove() { singleClick_ = false; }

private:
    wxWindow& wnd_;
    const size_t colFrom_;
    size_t colTo_;
    const int clientPosX_;
    bool singleClick_;
};
}

//----------------------------------------------------------------------------------------------------------------

class Grid::ColLabelWin : public SubWindow
{
public:
    ColLabelWin(Grid& parent) : SubWindow(parent) {}

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        if (IsThisEnabled())
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

        //coordinate with "colLabelHeight" in Grid constructor:
        wxFont labelFont = GetFont();
        labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont);

        wxDCTextColourChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

        const int colLabelHeight = refParent().colLabelHeight;

        wxPoint labelAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0)).x, 0); //client coordinates

        std::vector<ColumnWidth> absWidths = refParent().getColWidths(); //resolve stretched widths
        for (auto it = absWidths.begin(); it != absWidths.end(); ++it)
        {
            const size_t col = it - absWidths.begin();
            const int width  = it->width_; //don't use unsigned for calculations!

            if (labelAreaTL.x > rect.GetRight())
                return; //done, rect is fully covered
            if (labelAreaTL.x + width > rect.x)
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(width, colLabelHeight)), col, it->type_);
            labelAreaTL.x += width;
        }
        if (labelAreaTL.x > rect.GetRight())
            return; //done, rect is fully covered

        //fill gap after columns and cover full width
        if (fillGapAfterColumns)
        {
            int totalWidth = 0;
            for (const ColumnWidth& cw : absWidths)
                totalWidth += cw.width_;
            const int clientWidth = GetClientSize().GetWidth(); //need reliable, stable width in contrast to rect.width

            if (totalWidth < clientWidth)
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(clientWidth - totalWidth, colLabelHeight)), absWidths.size(), DUMMY_COLUMN_TYPE);
        }
    }

    void drawColumnLabel(wxDC& dc, const wxRect& rect, size_t col, ColumnType colType)
    {
        if (auto dataView = refParent().getDataProvider())
        {
            const bool isHighlighted = activeResizing ? col == activeResizing->getColumn    () : //highlight column on mouse-over
                                       activeMove     ? col == activeMove    ->getColumnFrom() :
                                       highlightCol   ? col == *highlightCol                   :
                                       false;

            RecursiveDcClipper clip(dc, rect);
            dataView->renderColumnLabel(refParent(), dc, rect, colType, isHighlighted);

            //draw move target location
            if (refParent().allowColumnMove)
                if (activeMove && activeMove->isRealMove())
                {
                    if (col + 1 == activeMove->refColumnTo()) //handle pos 1, 2, .. up to "at end" position
                        dc.GradientFillLinear(wxRect(rect.GetTopRight(), rect.GetBottomRight() + wxPoint(-2, 0)), COLOR_LABEL_GRADIENT_FROM, *wxBLUE, wxSOUTH);
                    else if (col == activeMove->refColumnTo() && col == 0) //pos 0
                        dc.GradientFillLinear(wxRect(rect.GetTopLeft(), rect.GetBottomLeft() + wxPoint(2, 0)), COLOR_LABEL_GRADIENT_FROM, *wxBLUE, wxSOUTH);
                }
        }
    }

    void onMouseLeftDown(wxMouseEvent& event) override
    {
        if (FindFocus() != &refParent().getMainWin())
            refParent().getMainWin().SetFocus();

        activeResizing.reset();
        activeMove.reset();

        if (Opt<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
        {
            if (action->wantResize)
            {
                if (!event.LeftDClick()) //double-clicks never seem to arrive here; why is this checked at all???
                    if (Opt<int> colWidth = refParent().getColWidth(action->col))
                        activeResizing = make_unique<ColumnResizing>(*this, action->col, *colWidth, event.GetPosition().x);
            }
            else //a move or single click
                activeMove = make_unique<ColumnMove>(*this, action->col, event.GetPosition().x);
        }
        event.Skip();
    }

    void onMouseLeftUp(wxMouseEvent& event) override
    {
        activeResizing.reset(); //nothing else to do, actual work done by onMouseMovement()

        if (activeMove)
        {
            if (activeMove->isRealMove())
            {
                if (refParent().allowColumnMove)
                {
                    const auto colFrom = activeMove->getColumnFrom();
                    auto       colTo   = activeMove->refColumnTo();

                    if (colTo > colFrom) //simulate "colFrom" deletion
                        --colTo;

                    refParent().moveColumn(colFrom, colTo);
                }
            }
            else //notify single label click
            {
                if (const Opt<ColumnType> colType = refParent().colToType(activeMove->getColumnFrom()))
                    sendEventNow(GridClickEvent(EVENT_GRID_COL_LABEL_MOUSE_LEFT, event, -1, *colType));
            }
            activeMove.reset();
        }

        refParent().updateWindowSizes(); //looks strange if done during onMouseMovement()
        refParent().Refresh();
        event.Skip();
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        activeResizing.reset();
        activeMove.reset();
        Refresh();
        //event.Skip(); -> we DID handle it!
    }

    void onMouseLeftDouble(wxMouseEvent& event) override
    {
        if (Opt<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
            if (action->wantResize)
            {
                //auto-size visible range on double-click
                const int bestWidth = refParent().getBestColumnSize(action->col); //return -1 on error
                if (bestWidth >= 0)
                {
                    refParent().setColumnWidth(bestWidth, action->col, ALLOW_GRID_EVENT);
                    refParent().Refresh(); //refresh main grid as well!
                }
            }
        event.Skip();
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        if (activeResizing)
        {
            const auto col     = activeResizing->getColumn();
            const int newWidth = activeResizing->getStartWidth() + event.GetPosition().x - activeResizing->getStartPosX();

            //set width tentatively
            refParent().setColumnWidth(newWidth, col, ALLOW_GRID_EVENT);

            //check if there's a small gap after last column, if yes, fill it
            int gapWidth = GetClientSize().GetWidth() - refParent().getColWidthsSum(GetClientSize().GetWidth());
            if (std::abs(gapWidth) < COLUMN_FILL_GAP_TOLERANCE)
                refParent().setColumnWidth(newWidth + gapWidth, col, ALLOW_GRID_EVENT);

            refParent().Refresh(); //refresh columns on main grid as well!
        }
        else if (activeMove)
        {
            const int clientPosX = event.GetPosition().x;
            if (std::abs(clientPosX - activeMove->getStartPosX()) > COLUMN_MOVE_DELAY) //real move (not a single click)
            {
                activeMove->setRealMove();

                const ptrdiff_t col = refParent().clientPosToMoveTargetColumn(event.GetPosition());
                if (col >= 0)
                    activeMove->refColumnTo() = col;
            }
        }
        else
        {
            if (const Opt<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
            {
                highlightCol = make_unique<size_t>(action->col);

                if (action->wantResize)
                    SetCursor(wxCURSOR_SIZEWE); //set window-local only! :)
                else
                    SetCursor(*wxSTANDARD_CURSOR);
            }
            else
            {
                highlightCol.reset();
                SetCursor(*wxSTANDARD_CURSOR);
            }
        }

        //update tooltip
        const wxString toolTip = [&]() -> wxString
        {
            const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            if (const Opt<ColumnType> ct = refParent().getColumnAtPos(absPos.x))
                if (auto prov = refParent().getDataProvider())
                    return prov->getToolTip(*ct);
            return wxString();
        }();
        setToolTip(toolTip);

        Refresh();
        event.Skip();
    }

    void onLeaveWindow(wxMouseEvent& event) override
    {
        highlightCol.reset(); //wxEVT_LEAVE_WINDOW does not respect mouse capture! -> however highlight is drawn unconditionally during move/resize!
        Refresh();
        event.Skip();
    }

    void onMouseRightDown(wxMouseEvent& event) override
    {
        if (const Opt<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
        {
            if (const Opt<ColumnType> colType = refParent().colToType(action->col))
                sendEventNow(GridClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, event, -1, *colType)); //notify right click
            else assert(false);
        }
        else
            //notify right click (on free space after last column)
            if (fillGapAfterColumns)
                sendEventNow(GridClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, event, -1, DUMMY_COLUMN_TYPE));

        event.Skip();
    }

    std::unique_ptr<ColumnResizing> activeResizing;
    std::unique_ptr<ColumnMove>     activeMove;
    std::unique_ptr<size_t>         highlightCol; //column during mouse-over
};

//----------------------------------------------------------------------------------------------------------------
namespace
{
const wxEventType EVENT_GRID_HAS_SCROLLED = wxNewEventType(); //internal to Grid::MainWin::ScrollWindow()
}
//----------------------------------------------------------------------------------------------------------------

class Grid::MainWin : public SubWindow
{
public:
    MainWin(Grid& parent,
            RowLabelWin& rowLabelWin,
            ColLabelWin& colLabelWin) : SubWindow(parent),
        rowLabelWin_(rowLabelWin),
        colLabelWin_(colLabelWin),
        cursorRow(0),
        selectionAnchor(0),
        gridUpdatePending(false)
    {
        Connect(EVENT_GRID_HAS_SCROLLED, wxEventHandler(MainWin::onRequestWindowUpdate), nullptr, this);
    }

    ~MainWin() { assert(!gridUpdatePending); }

    size_t getCursor() const { return cursorRow; }
    size_t getAnchor() const { return selectionAnchor; }

    void setCursor(size_t newCursorRow, size_t newAnchorRow)
    {
        cursorRow       = newCursorRow;
        selectionAnchor = newAnchorRow;
        activeSelection.reset(); //e.g. user might search with F3 while holding down left mouse button
    }

private:
    void render(wxDC& dc, const wxRect& rect) override
    {
        if (IsThisEnabled())
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

        dc.SetFont(GetFont()); //harmonize with Grid::getBestColumnSize()

        wxDCTextColourChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

        const int rowHeight = rowLabelWin_.getRowHeight();

        //why again aren't we using RowLabelWin::getRowsOnClient() here?
        const wxPoint topLeft     = refParent().CalcUnscrolledPosition(rect.GetTopLeft());
        const wxPoint bottomRight = refParent().CalcUnscrolledPosition(rect.GetBottomRight());

        const int rowFirst = std::max(topLeft    .y / rowHeight, 0); // [rowFirst, rowLast)
        const int rowLast  = std::min(bottomRight.y / rowHeight + 1, static_cast<int>(refParent().getRowCount()));

        wxPoint cellAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0))); //client coordinates

        std::vector<ColumnWidth> absWidths = refParent().getColWidths(); //resolve stretched widths
        {
            int totalRowWidth = 0;
            for (const ColumnWidth& cw : absWidths)
                totalRowWidth += cw.width_;

            //fill gap after columns and cover full width
            if (fillGapAfterColumns)
                totalRowWidth = std::max(totalRowWidth, GetClientSize().GetWidth());

            if (auto prov = refParent().getDataProvider())
            {
                RecursiveDcClipper dummy2(dc, rect); //do NOT draw background on cells outside of invalidated rect invalidating foreground text!

                //draw background lines
                for (int row = rowFirst; row < rowLast; ++row)
                {
                    const wxRect rowRect(cellAreaTL + wxPoint(0, row * rowHeight), wxSize(totalRowWidth, rowHeight));
                    RecursiveDcClipper dummy3(dc, rowRect);
                    prov->renderRowBackgound(dc, rowRect, row, refParent().IsThisEnabled(), drawAsSelected(row));
                }

                //draw single cells, column by column
                for (const ColumnWidth& cw : absWidths)
                {
                    if (cellAreaTL.x > rect.GetRight())
                        return; //done

                    if (cellAreaTL.x + cw.width_ > rect.x)
                        for (int row = rowFirst; row < rowLast; ++row)
                        {
                            const wxRect cellRect(cellAreaTL.x, cellAreaTL.y + row * rowHeight, cw.width_, rowHeight);
                            RecursiveDcClipper dummy3(dc, cellRect);
                            prov->renderCell(dc, cellRect, row, cw.type_, refParent().IsThisEnabled(), drawAsSelected(row));
                        }
                    cellAreaTL.x += cw.width_;
                }
            }
        }
    }

    bool drawAsSelected(size_t row) const
    {
        if (activeSelection) //check if user is currently selecting with mouse
        {
            const size_t rowFrom = std::min(activeSelection->getStartRow(), activeSelection->getCurrentRow());
            const size_t rowTo   = std::max(activeSelection->getStartRow(), activeSelection->getCurrentRow());

            if (rowFrom <= row && row <= rowTo)
                return activeSelection->isPositiveSelect(); //overwrite default
        }
        return refParent().isSelected(row);
    }

    void onMouseLeftDown (wxMouseEvent& event) override { onMouseDown(event); }
    void onMouseLeftUp   (wxMouseEvent& event) override { onMouseUp  (event); }
    void onMouseRightDown(wxMouseEvent& event) override { onMouseDown(event); }
    void onMouseRightUp  (wxMouseEvent& event) override { onMouseUp  (event); }

    void onMouseLeftDouble(wxMouseEvent& event) override
    {
        const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
        const auto row       = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
        if (row >= 0)
        {
            const Opt<ColumnType> ct = refParent().getColumnAtPos(absPos.x);
            const ColumnType colType = ct ? *ct : DUMMY_COLUMN_TYPE;
            //client is interested in all double-clicks, even those outside of the grid!
            sendEventNow(GridClickEvent(EVENT_GRID_MOUSE_LEFT_DOUBLE, event, row, colType));
        }
        event.Skip();
    }

    void onMouseDown(wxMouseEvent& event) //handle left and right mouse button clicks (almost) the same
    {
        if (wxWindow::FindFocus() != this) //doesn't seem to happen automatically for right mouse button
            SetFocus();

        const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
        const ptrdiff_t  row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
        if (row >= 0)
        {
            const Opt<ColumnType> ct = refParent().getColumnAtPos(absPos.x);
            const ColumnType colType = ct ? *ct : DUMMY_COLUMN_TYPE;

            if (!event.RightDown() || !refParent().isSelected(row)) //do NOT start a new selection if user right-clicks on a selected area!
            {
                if (event.ControlDown())
                    activeSelection = make_unique<MouseSelection>(*this, row, !refParent().isSelected(row));
                else if (event.ShiftDown())
                {
                    activeSelection = make_unique<MouseSelection>(*this, selectionAnchor, true);
                    refParent().clearSelection(ALLOW_GRID_EVENT);
                }
                else
                {
                    activeSelection = make_unique<MouseSelection>(*this, row, true);
                    refParent().clearSelection(ALLOW_GRID_EVENT);
                }
            }

            //notify event *after* potential "clearSelection(true)" above: a client should first receive a GridRangeSelectEvent for clearing the grid, if necessary,
            //then GridClickEvent and the associated GridRangeSelectEvent one after the other
            GridClickEvent mouseEvent(event.RightDown() ? EVENT_GRID_MOUSE_RIGHT_DOWN : EVENT_GRID_MOUSE_LEFT_DOWN, event, row, colType);
            sendEventNow(mouseEvent);

            Refresh();
        }
        event.Skip(); //allow changing focus
    }

    void onMouseUp(wxMouseEvent& event)
    {
        if (activeSelection)
        {
            const size_t rowCount = refParent().getRowCount();
            if (rowCount > 0)
            {
                if (activeSelection->getCurrentRow() < rowCount)
                {
                    cursorRow = activeSelection->getCurrentRow();
                    selectionAnchor = activeSelection->getStartRow(); //allowed to be "out of range"
                }
                else if (activeSelection->getStartRow() < rowCount) //don't change cursor if "to" and "from" are out of range
                {
                    cursorRow = rowCount - 1;
                    selectionAnchor = activeSelection->getStartRow(); //allowed to be "out of range"
                }
                else //total selection "out of range"
                    selectionAnchor = cursorRow;
            }
            //slight deviation from Explorer: change cursor while dragging mouse! -> unify behavior with shift + direction keys

            refParent().selectRangeAndNotify(activeSelection->getStartRow  (), //from
                                             activeSelection->getCurrentRow(), //to
                                             activeSelection->isPositiveSelect());
            activeSelection.reset();
        }

        //this one may point to row which is not in visible area!
        const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());

        const ptrdiff_t      row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
        const Opt<ColumnType> ct = refParent().getColumnAtPos(absPos.x);
        const ColumnType colType = ct ? *ct : DUMMY_COLUMN_TYPE; //we probably should notify even if colInfo is invalid!

        //notify click event after the range selection! e.g. this makes sure the selection is applied before showing a context menu
        sendEventNow(GridClickEvent(event.RightUp() ? EVENT_GRID_MOUSE_RIGHT_UP : EVENT_GRID_MOUSE_LEFT_UP, event, row, colType));

        Refresh();
        event.Skip(); //allow changing focus
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        activeSelection.reset();
        Refresh();
        //event.Skip(); -> we DID handle it!
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        if (activeSelection)
            activeSelection->evalMousePos(); //eval on both mouse movement + timer event!

        //change tooltip
        const wxString toolTip = [&]() -> wxString
        {
            const ptrdiff_t rowCount = refParent().getRowCount();
            const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());

            const ptrdiff_t row     = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            const Opt<ColumnType> ct = refParent().getColumnAtPos(absPos.x);
            if (ct && 0 <= row && row < rowCount)
                if (auto prov = refParent().getDataProvider())
                    return prov->getToolTip(row, *ct);
            return wxString();
        }();

        setToolTip(toolTip);

        event.Skip();
    }

    void onFocus(wxFocusEvent& event) override { Refresh(); event.Skip(); }

    class MouseSelection : private wxEvtHandler
    {
    public:
        MouseSelection(MainWin& wnd, size_t rowStart, bool positiveSelect) :
            wnd_(wnd), rowStart_(rowStart), rowCurrent_(rowStart), positiveSelect_(positiveSelect), toScrollX(0), toScrollY(0),
            tickCountLast(getTicks()),
            ticksPerSec_(ticksPerSec())
        {
            wnd_.CaptureMouse();
            timer.Connect(wxEVT_TIMER, wxEventHandler(MouseSelection::onTimer), nullptr, this);
            timer.Start(100); //timer interval in ms
            evalMousePos();
        }
        ~MouseSelection() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

        size_t getStartRow     () const { return rowStart_;       }
        size_t getCurrentRow   () const { return rowCurrent_;     }
        bool   isPositiveSelect() const { return positiveSelect_; } //are we selecting or unselecting?

        void evalMousePos()
        {
            double deltaTime = 0;
            if (ticksPerSec_ > 0)
            {
                const TickVal now = getTicks(); //isValid() on error
                deltaTime = static_cast<double>(dist(tickCountLast, now)) / ticksPerSec_; //unit: [sec]
                tickCountLast = now;
            }

            wxMouseState mouseState = wxGetMouseState();
            const wxPoint clientPos = wnd_.ScreenToClient(wxPoint(mouseState.GetX(), mouseState.GetY()));
            const wxSize clientSize = wnd_.GetClientSize();
            assert(wnd_.GetClientAreaOrigin() == wxPoint());

            //scroll while dragging mouse
            const int overlapPixY = clientPos.y < 0 ? clientPos.y :
                                    clientPos.y >= clientSize.GetHeight() ? clientPos.y - (clientSize.GetHeight() - 1) : 0;
            const int overlapPixX = clientPos.x < 0 ? clientPos.x :
                                    clientPos.x >= clientSize.GetWidth() ? clientPos.x - (clientSize.GetWidth() - 1) : 0;

            int pixelsPerUnitY = 0;
            wnd_.refParent().GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
            if (pixelsPerUnitY <= 0) return;

            const double mouseDragSpeedIncScrollU = pixelsPerUnitY > 0 ? MOUSE_DRAG_ACCELERATION * wnd_.rowLabelWin_.getRowHeight() / pixelsPerUnitY : 0; //unit: [scroll units / (pixel * sec)]

            auto autoScroll = [&](int overlapPix, double& toScroll)
            {
                if (overlapPix != 0)
                {
                    const double scrollSpeed = overlapPix * mouseDragSpeedIncScrollU; //unit: [scroll units / sec]
                    toScroll += scrollSpeed * deltaTime;
                }
                else
                    toScroll = 0;
            };

            autoScroll(overlapPixX, toScrollX);
            autoScroll(overlapPixY, toScrollY);

            if (toScrollX != 0 || toScrollY != 0)
            {
                wnd_.refParent().scrollDelta(static_cast<int>(toScrollX), static_cast<int>(toScrollY)); //
                toScrollX -= static_cast<int>(toScrollX); //rounds down for positive numbers, up for negative,
                toScrollY -= static_cast<int>(toScrollY); //exactly what we want
            }

            {
                //select current row *after* scrolling
                wxPoint clientPosTrimmed = clientPos;
                numeric::clamp(clientPosTrimmed.y, 0, clientSize.GetHeight() - 1); //do not select row outside client window!

                const wxPoint absPos = wnd_.refParent().CalcUnscrolledPosition(clientPosTrimmed);
                const ptrdiff_t newRow = wnd_.rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
                if (newRow >= 0)
                    if (rowCurrent_ != newRow)
                    {
                        rowCurrent_ = newRow;
                        wnd_.Refresh();
                    }
            }
        }

    private:
        void onTimer(wxEvent& event) { evalMousePos(); }

        MainWin& wnd_;
        const size_t rowStart_;
        ptrdiff_t rowCurrent_;
        const bool positiveSelect_;
        wxTimer timer;
        double toScrollX; //count outstanding scroll units to scroll while dragging mouse
        double toScrollY; //
        TickVal tickCountLast;
        const std::int64_t ticksPerSec_;
    };

    void ScrollWindow(int dx, int dy, const wxRect* rect) override
    {
        wxWindow::ScrollWindow(dx, dy, rect);
        rowLabelWin_.ScrollWindow(0, dy, rect);
        colLabelWin_.ScrollWindow(dx, 0, rect);

        //attention, wxGTK call sequence: wxScrolledWindow::Scroll() -> wxScrolledHelperNative::Scroll() -> wxScrolledHelperNative::DoScroll()
        //which *first* calls us, MainWin::ScrollWindow(), and *then* internally updates m_yScrollPosition
        //=> we cannot use CalcUnscrolledPosition() here which gives the wrong/outdated value!!!
        //=> we need to update asynchronously:
        //=> don't use plain async event => severe performance issues on wxGTK!
        //=> can't use idle event neither: too few idle events on Windows, e.g. NO idle events while mouse drag-scrolling!
        //=> solution: send single async event at most!
        if (!gridUpdatePending) //without guarding, the number of outstanding async events can get very high during scrolling!! test case: Ubuntu: 170; Windows: 20
        {
            gridUpdatePending = true;
            wxCommandEvent scrollEvent(EVENT_GRID_HAS_SCROLLED);
            AddPendingEvent(scrollEvent); //asynchronously call updateAfterScroll()
        }
    }

    void onRequestWindowUpdate(wxEvent& event)
    {
        assert(gridUpdatePending);
        ZEN_ON_SCOPE_EXIT(gridUpdatePending = false);

        refParent().updateWindowSizes(false); //row label width has changed -> do *not* update scrollbars: recursion on wxGTK! -> still a problem, now that we're called async??
        rowLabelWin_.Update(); //update while dragging scroll thumb
    }

    RowLabelWin& rowLabelWin_;
    ColLabelWin& colLabelWin_;

    std::unique_ptr<MouseSelection> activeSelection; //bound while user is selecting with mouse

    ptrdiff_t cursorRow;
    size_t selectionAnchor;
    bool gridUpdatePending;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

Grid::Grid(wxWindow* parent,
           wxWindowID id,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name) : wxScrolledWindow(parent, id, pos, size, style | wxWANTS_CHARS, name),
    showScrollbarX(SB_SHOW_AUTOMATIC),
    showScrollbarY(SB_SHOW_AUTOMATIC),
    colLabelHeight(0), //dummy init
    drawRowLabel(true),
    allowColumnMove(true),
    allowColumnResize(true),
    rowCountOld(0)
{
    cornerWin_   = new CornerWin  (*this); //
    rowLabelWin_ = new RowLabelWin(*this); //owership handled by "this"
    colLabelWin_ = new ColLabelWin(*this); //
    mainWin_     = new MainWin    (*this, *rowLabelWin_, *colLabelWin_); //

    colLabelHeight = 2 * DEFAULT_COL_LABEL_BORDER + [&]() -> int
    {
        //coordinate with ColLabelWin::render():
        wxFont labelFont = colLabelWin_->GetFont();
        labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        return labelFont.GetPixelSize().GetHeight();
    }();

    SetTargetWindow(mainWin_);

    SetInitialSize(size); //"Most controls will use this to set their initial size" -> why not

    assert(GetClientSize() == GetSize()); //borders are NOT allowed for Grid
    //reason: updateWindowSizes() wants to use "GetSize()" as a "GetClientSize()" including scrollbars

    Connect(wxEVT_PAINT,            wxPaintEventHandler(Grid::onPaintEvent     ), nullptr, this);
    Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(Grid::onEraseBackGround), nullptr, this);
    Connect(wxEVT_SIZE,             wxSizeEventHandler (Grid::onSizeEvent      ), nullptr, this);

    Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(Grid::onKeyDown), nullptr, this);
}


void Grid::updateWindowSizes(bool updateScrollbar)
{
    /* We have to deal with TWO nasty circular dependencies:
    1.
    	rowLabelWidth
    	    /|\
    	mainWin::client width
    	    /|\
    	SetScrollbars -> show/hide horizontal scrollbar depending on client width
    	    /|\
    	mainWin::client height -> possibly trimmed by horizontal scrollbars
    	    /|\
    	rowLabelWidth

    2.
    	mainWin_->GetClientSize()
    	    /|\
    	SetScrollbars -> show/hide scrollbars depending on whether client size is big enough
    	    /|\
    	GetClientSize(); -> possibly trimmed by scrollbars
    	    /|\
    	mainWin_->GetClientSize()  -> also trimmed, since it's a sub-window!
    */

    //break this vicious circle:

    //harmonize with Grid::GetSizeAvailableForScrollTarget()!

    //1. calculate row label width independent from scrollbars
    const int mainWinHeightGross = std::max(GetSize().GetHeight() - colLabelHeight, 0); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();                   //

    int rowLabelWidth = 0;
    if (drawRowLabel && logicalHeight > 0)
    {
        ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
        numeric::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
        numeric::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

        const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
        const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
        if (rowFrom >= 0 && rowTo >= 0)
            rowLabelWidth = rowLabelWin_->getBestWidth(rowFrom, rowTo);
    }

    auto getMainWinSize = [&](const wxSize& clientSize) { return wxSize(std::max(0, clientSize.GetWidth() - rowLabelWidth), std::max(0, clientSize.GetHeight() - colLabelHeight)); };

    auto setScrollbars2 = [&](int logWidth, int logHeight) //replace SetScrollbars, which loses precision to pixelsPerUnitX for some brain-dead reason
    {
        int ppsuX = 0; //pixel per scroll unit
        int ppsuY = 0;
        GetScrollPixelsPerUnit(&ppsuX, &ppsuY);

        const int ppsuNew = rowLabelWin_->getRowHeight();
        if (ppsuX != ppsuNew || ppsuY != ppsuNew) //support polling!
            SetScrollRate(ppsuNew, ppsuNew); //internally calls AdjustScrollbars()!

        mainWin_->SetVirtualSize(logWidth, logHeight);
        AdjustScrollbars(); //lousy wxWidgets design decision: internally calls mainWin_->GetClientSize() without considering impact of scrollbars!
        //Attention: setting scrollbars triggers *synchronous* resize event if scrollbars are shown or hidden! => updateWindowSizes() recursion! (Windows)
    };

    //2. update managed windows' sizes: just assume scrollbars are already set correctly, even if they may not be (yet)!
    //this ensures mainWin_->SetVirtualSize() and AdjustScrollbars() are working with the correct main window size, unless sb change later, which triggers a recalculation anyway!
    const wxSize mainWinSize = getMainWinSize(GetClientSize());

    cornerWin_  ->SetSize(0, 0, rowLabelWidth, colLabelHeight);
    rowLabelWin_->SetSize(0, colLabelHeight, rowLabelWidth, mainWinSize.GetHeight());
    colLabelWin_->SetSize(rowLabelWidth, 0, mainWinSize.GetWidth(), colLabelHeight);
    mainWin_    ->SetSize(rowLabelWidth, colLabelHeight, mainWinSize.GetWidth(), mainWinSize.GetHeight());

    //avoid flicker in wxWindowMSW::HandleSize() when calling ::EndDeferWindowPos() where the sub-windows are moved only although they need to be redrawn!
    colLabelWin_->Refresh();
    mainWin_    ->Refresh();

    //3. update scrollbars: "guide wxScrolledHelper to not screw up too much"
    if (updateScrollbar)
    {
        const int mainWinWidthGross = getMainWinSize(GetSize()).GetWidth();

        if (logicalHeight <= mainWinHeightGross &&
            getColWidthsSum(mainWinWidthGross) <= mainWinWidthGross &&
            //this special case needs to be considered *only* when both scrollbars are flexible:
            showScrollbarX == SB_SHOW_AUTOMATIC &&
            showScrollbarY == SB_SHOW_AUTOMATIC)
            setScrollbars2(0, 0); //no scrollbars required at all! -> wxScrolledWindow requires active help to detect this special case!
        else
        {
            const int logicalWidthTmp = getColWidthsSum(mainWinSize.GetWidth()); //assuming vertical scrollbar stays as it is...
            setScrollbars2(logicalWidthTmp, logicalHeight); //if scrollbars are shown or hidden a new resize event recurses into updateWindowSizes()
            /*
            is there a risk of endless recursion? No, 2-level recursion at most, consider the following 6 cases:

            <----------gw---------->
            <----------nw------>
            ------------------------  /|\   /|\
            |                   |  |   |     |
            |     main window   |  |   nh    |
            |                   |  |   |     gh
            ------------------------  \|/    |
            |                   |  |         |
            ------------------------        \|/
            	gw := gross width
            	nw := net width := gross width - sb size
            	gh := gross height
            	nh := net height := gross height - sb size

            There are 6 cases that can occur:
            ---------------------------------
            	lw := logical width
            	lh := logical height

            1. lw <= gw && lh <= gh  => no scrollbars needed

            2. lw > gw  && lh > gh   => need both scrollbars

            3. lh > gh
            	4.1 lw <= nw         => need vertical scrollbar only
            	4.2 nw < lw <= gw    => need both scrollbars

            4. lw > gw
            	3.1 lh <= nh         => need horizontal scrollbar only
            	3.2 nh < lh <= gh    => need both scrollbars
            */
        }
    }
}


wxSize Grid::GetSizeAvailableForScrollTarget(const wxSize& size)
{
    //harmonize with Grid::updateWindowSizes()!

    //1. calculate row label width independent from scrollbars
    const int mainWinHeightGross = std::max(size.GetHeight() - colLabelHeight, 0); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();              //

    int rowLabelWidth = 0;
    if (drawRowLabel && logicalHeight > 0)
    {
        ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
        numeric::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
        numeric::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

        const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
        const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
        if (rowFrom >= 0 && rowTo >= 0)
            rowLabelWidth = rowLabelWin_->getBestWidth(rowFrom, rowTo);
    }

    return size - wxSize(rowLabelWidth, colLabelHeight);
}


void Grid::onPaintEvent(wxPaintEvent& event) { wxPaintDC dc(this); }


void Grid::onKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    if (GetLayoutDirection() == wxLayout_RightToLeft)
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

    const ptrdiff_t rowCount  = getRowCount();
    const ptrdiff_t cursorRow = mainWin_->getCursor();

    auto moveCursorTo = [&](ptrdiff_t row)
    {
        if (rowCount > 0)
        {
            numeric::clamp<ptrdiff_t>(row, 0, rowCount - 1);
            setGridCursor(row);
        }
    };

    auto selectWithCursorTo = [&](ptrdiff_t row)
    {
        if (rowCount > 0)
        {
            numeric::clamp<ptrdiff_t>(row, 0, rowCount - 1);
            selectWithCursor(row);
        }
    };

    switch (keyCode)
    {
        //case WXK_TAB:
        //    if (Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward))
        //        return;
        //    break;

        case WXK_UP:
        case WXK_NUMPAD_UP:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow - 1);
            else if (event.ControlDown())
                scrollDelta(0, -1);
            else
                moveCursorTo(cursorRow - 1);
            return; //swallow event: wxScrolledWindow, wxWidgets 2.9.3 on Kubuntu x64 processes arrow keys: prevent this!

        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow + 1);
            else if (event.ControlDown())
                scrollDelta(0, 1);
            else
                moveCursorTo(cursorRow + 1);
            return; //swallow event

        case WXK_LEFT:
        case WXK_NUMPAD_LEFT:
            if (event.ControlDown())
                scrollDelta(-1, 0);
            else if (event.ShiftDown())
                ;
            else
                moveCursorTo(cursorRow);
            return;

        case WXK_RIGHT:
        case WXK_NUMPAD_RIGHT:
            if (event.ControlDown())
                scrollDelta(1, 0);
            else if (event.ShiftDown())
                ;
            else
                moveCursorTo(cursorRow);
            return;

        case WXK_HOME:
        case WXK_NUMPAD_HOME:
            if (event.ShiftDown())
                selectWithCursorTo(0);
            else if (event.ControlDown())
                moveCursorTo(0);
            else
                moveCursorTo(0);
            return;

        case WXK_END:
        case WXK_NUMPAD_END:
            if (event.ShiftDown())
                selectWithCursorTo(rowCount - 1);
            else if (event.ControlDown())
                moveCursorTo(rowCount - 1);
            else
                moveCursorTo(rowCount - 1);
            return;

        case WXK_PAGEUP:
        case WXK_NUMPAD_PAGEUP:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow - GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            else if (event.ControlDown())
                ;
            else
                moveCursorTo(cursorRow - GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow + GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            else if (event.ControlDown())
                ;
            else
                moveCursorTo(cursorRow + GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case 'A':  //Ctrl + A - select all
            if (event.ControlDown())
                selectRangeAndNotify(0, rowCount);
            break;

        case WXK_NUMPAD_ADD: //CTRL + '+' - auto-size all
            if (event.ControlDown())
                autoSizeColumns(ALLOW_GRID_EVENT);
            return;
    }

    event.Skip();
}


void Grid::setColumnLabelHeight(int height)
{
    colLabelHeight = std::max(0, height);
    updateWindowSizes();
}


void Grid::showRowLabel(bool show)
{
    drawRowLabel = show;
    updateWindowSizes();
}


void Grid::selectAllRows(GridEventPolicy rangeEventPolicy)
{
    selection.selectAll();
    mainWin_->Refresh();

    if (rangeEventPolicy == ALLOW_GRID_EVENT) //notify event, even if we're not triggered by user interaction
    {
        GridRangeSelectEvent selEvent(0, getRowCount(), true);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(selEvent);
    }
}


void Grid::clearSelection(GridEventPolicy rangeEventPolicy)
{
    selection.clear();
    mainWin_->Refresh();

    if (rangeEventPolicy == ALLOW_GRID_EVENT) //notify event, even if we're not triggered by user interaction
    {
        GridRangeSelectEvent unselectionEvent(0, getRowCount(), false);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(unselectionEvent);
    }
}


void Grid::scrollDelta(int deltaX, int deltaY)
{
    int scrollPosX = 0;
    int scrollPosY = 0;
    GetViewStart(&scrollPosX, &scrollPosY);

    scrollPosX += deltaX;
    scrollPosY += deltaY;

    scrollPosX = std::max(0, scrollPosX); //wxScrollHelper::Scroll() will exit prematurely if input happens to be "-1"!
    scrollPosY = std::max(0, scrollPosY); //

    Scroll(scrollPosX, scrollPosY);
    updateWindowSizes(); //may show horizontal scroll bar
}


void Grid::redirectRowLabelEvent(wxMouseEvent& event)
{
    event.m_x = 0;
    if (wxEvtHandler* evtHandler = mainWin_->GetEventHandler())
        evtHandler->ProcessEvent(event);

    if (event.ButtonDown() && wxWindow::FindFocus() != mainWin_)
        mainWin_->SetFocus();
}


size_t Grid::getRowCount() const
{
    return dataView_ ? dataView_->getRowCount() : 0;
}


void Grid::Refresh(bool eraseBackground, const wxRect* rect)
{
    const size_t rowCountNew = getRowCount();
    if (rowCountOld != rowCountNew)
    {
        rowCountOld = rowCountNew;
        updateWindowSizes();
    }

    if (selection.size() != rowCountNew) //clear selection only when needed (consider setSelectedRows())
        selection.init(rowCountNew);

    wxScrolledWindow::Refresh(eraseBackground, rect);
}


void Grid::setRowHeight(int height)
{
    rowLabelWin_->setRowHeight(height);
    updateWindowSizes();
    Refresh();
}


void Grid::setColumnConfig(const std::vector<Grid::ColumnAttribute>& attr)
{
    //hold ownership of non-visible columns
    oldColAttributes = attr;

    std::vector<VisibleColumn> visCols;
    for (const ColumnAttribute& ca : attr)
        if (ca.visible_)
            visCols.emplace_back(ca.type_, ca.offset_, ca.stretch_);

    //"ownership" of visible columns is now within Grid
    visibleCols = visCols;

    updateWindowSizes();
    Refresh();
}


std::vector<Grid::ColumnAttribute> Grid::getColumnConfig() const
{
    //get non-visible columns (+ outdated visible ones)
    std::vector<ColumnAttribute> output = oldColAttributes;

    auto iterVcols    = visibleCols.begin();
    auto iterVcolsend = visibleCols.end();

    //update visible columns but keep order of non-visible ones!
    for (ColumnAttribute& ca : output)
        if (ca.visible_)
        {
            if (iterVcols != iterVcolsend)
            {
                ca.type_    = iterVcols->type_;
                ca.stretch_ = iterVcols->stretch_;
                ca.offset_  = iterVcols->offset_;
                ++iterVcols;
            }
            else
                assert(false);
        }
    assert(iterVcols == iterVcolsend);

    return output;
}


void Grid::showScrollBars(Grid::ScrollBarStatus horizontal, Grid::ScrollBarStatus vertical)
{
    if (showScrollbarX == horizontal &&
        showScrollbarY == vertical) return; //support polling!

    showScrollbarX = horizontal;
    showScrollbarY = vertical;

#if defined ZEN_WIN || defined ZEN_MAC
    //handled by Grid::SetScrollbar
#elif defined ZEN_LINUX //get rid of scrollbars, but preserve scrolling behavior!
    //the following wxGTK approach is pretty much identical to wxWidgets 2.9 ShowScrollbars() code!

    auto mapStatus = [](ScrollBarStatus sbStatus) -> GtkPolicyType
    {
        switch (sbStatus)
        {
            case SB_SHOW_AUTOMATIC:
                return GTK_POLICY_AUTOMATIC;
            case SB_SHOW_ALWAYS:
                return GTK_POLICY_ALWAYS;
            case SB_SHOW_NEVER:
                return GTK_POLICY_NEVER;
        }
        assert(false);
        return GTK_POLICY_AUTOMATIC;
    };

    GtkWidget* gridWidget = wxWindow::m_widget;
    GtkScrolledWindow* scrolledWindow = GTK_SCROLLED_WINDOW(gridWidget);
    ::gtk_scrolled_window_set_policy(scrolledWindow,
                                     mapStatus(horizontal),
                                     mapStatus(vertical));
#endif

    updateWindowSizes();

    /*
    wxWidgets >= 2.9 ShowScrollbars() is next to useless since it doesn't
    honor wxSHOW_SB_ALWAYS on OS X, so let's ditch it and avoid more non-portability surprises

    #if wxCHECK_VERSION(2, 9, 0)
    auto mapStatus = [](ScrollBarStatus sbStatus) -> wxScrollbarVisibility
    {
        switch (sbStatus)
        {
            case SB_SHOW_AUTOMATIC:
                return wxSHOW_SB_DEFAULT;
            case SB_SHOW_ALWAYS:
                return wxSHOW_SB_ALWAYS;
            case SB_SHOW_NEVER:
                return wxSHOW_SB_NEVER;
        }
        assert(false);
        return wxSHOW_SB_DEFAULT;
    };
    ShowScrollbars(mapStatus(horizontal), mapStatus(vertical));
    #endif
    */
}

#if defined ZEN_WIN || defined ZEN_MAC
void Grid::SetScrollbar(int orientation, int position, int thumbSize, int range, bool refresh)
{
    ScrollBarStatus sbStatus = SB_SHOW_AUTOMATIC;
    if (orientation == wxHORIZONTAL)
        sbStatus = showScrollbarX;
    else if (orientation == wxVERTICAL)
        sbStatus = showScrollbarY;
    else
        assert(false);

    switch (sbStatus)
    {
        case SB_SHOW_AUTOMATIC:
            wxScrolledWindow::SetScrollbar(orientation, position, thumbSize, range, refresh);
            break;

        case SB_SHOW_ALWAYS:
            if (range <= 1) //scrollbars would be hidden for range == 0 or 1!
                wxScrolledWindow::SetScrollbar(orientation, 0, 199999, 200000, refresh);
            else
                wxScrolledWindow::SetScrollbar(orientation, position, thumbSize, range, refresh);
            break;

        case SB_SHOW_NEVER:
            wxScrolledWindow::SetScrollbar(orientation, 0, 0, 0, refresh);
            break;
    }
}
#endif


wxWindow& Grid::getCornerWin  () { return *cornerWin_;   }
wxWindow& Grid::getRowLabelWin() { return *rowLabelWin_; }
wxWindow& Grid::getColLabelWin() { return *colLabelWin_; }
wxWindow& Grid::getMainWin    () { return *mainWin_;     }
const wxWindow& Grid::getMainWin() const { return *mainWin_; }


wxRect Grid::getColumnLabelArea(ColumnType colType) const
{
    std::vector<ColumnWidth> absWidths = getColWidths(); //resolve negative/stretched widths

    auto iterCol = std::find_if(absWidths.begin(), absWidths.end(), [&](const ColumnWidth& cw) { return cw.type_ == colType; });
    if (iterCol != absWidths.end())
    {
        ptrdiff_t posX = 0;
        for (auto it = absWidths.begin(); it != iterCol; ++it)
            posX += it->width_;

        return wxRect(wxPoint(posX, 0), wxSize(iterCol->width_, colLabelHeight));
    }

    return wxRect();
}


Opt<Grid::ColAction> Grid::clientPosToColumnAction(const wxPoint& pos) const
{
    const int absPosX = CalcUnscrolledPosition(pos).x;
    if (absPosX >= 0)
    {
        const int resizeTolerance = allowColumnResize ? COLUMN_RESIZE_TOLERANCE : 0;
        std::vector<ColumnWidth> absWidths = getColWidths(); //resolve stretched widths

        int accuWidth = 0;
        for (size_t col = 0; col < absWidths.size(); ++col)
        {
            accuWidth += absWidths[col].width_;
            if (std::abs(absPosX - accuWidth) < resizeTolerance)
            {
                ColAction out = {};
                out.wantResize = true;
                out.col        = col;
                return out;
            }
            else if (absPosX < accuWidth)
            {
                ColAction out = {};
                out.wantResize = false;
                out.col        = col;
                return out;
            }
        }
    }
    return NoValue();
}


void Grid::moveColumn(size_t colFrom, size_t colTo)
{
    if (colFrom < visibleCols.size() &&
        colTo   < visibleCols.size() &&
        colTo != colFrom)
    {
        const auto colAtt = visibleCols[colFrom];
        visibleCols.erase (visibleCols.begin() + colFrom);
        visibleCols.insert(visibleCols.begin() + colTo, colAtt);
    }
}


ptrdiff_t Grid::clientPosToMoveTargetColumn(const wxPoint& pos) const
{
    std::vector<ColumnWidth> absWidths = getColWidths(); //resolve negative/stretched widths

    const int absPosX = CalcUnscrolledPosition(pos).x;

    int accuWidth = 0;
    for (auto iterCol = absWidths.begin(); iterCol != absWidths.end(); ++iterCol)
    {
        const int width = iterCol->width_; //beware dreaded unsigned conversions!
        accuWidth += width;

        if (absPosX < accuWidth - width / 2)
            return iterCol - absWidths.begin();
    }
    return absWidths.size();
}


Opt<ColumnType> Grid::colToType(size_t col) const
{
    if (col < visibleCols.size())
        return visibleCols[col].type_;
    return NoValue();
}


ptrdiff_t Grid::getRowAtPos(int posY) const { return rowLabelWin_->getRowAtPos(posY); }


Opt<ColumnType> Grid::getColumnAtPos(int posX) const
{
    if (posX >= 0)
    {
        int accWidth = 0;
        for (const ColumnWidth& cw : getColWidths())
        {
            accWidth += cw.width_;
            if (posX < accWidth)
                return cw.type_;
        }
    }
    return NoValue();
}


wxRect Grid::getCellArea(size_t row, ColumnType colType) const
{
    const wxRect& colArea = getColumnLabelArea(colType);
    const wxRect& rowArea = rowLabelWin_->getRowLabelArea(row);
    return wxRect(wxPoint(colArea.x, rowArea.y), wxSize(colArea.width, rowArea.height));
}


void Grid::setGridCursor(size_t row)
{
    mainWin_->setCursor(row, row);
    makeRowVisible(row);

    selection.clear(); //clear selection, do NOT fire event
    selectRangeAndNotify(row, row); //set new selection + fire event

    mainWin_->Refresh();
    rowLabelWin_->Refresh(); //row labels! (Kubuntu)
}


void Grid::selectWithCursor(ptrdiff_t row)
{
    const size_t anchorRow = mainWin_->getAnchor();

    mainWin_->setCursor(row, anchorRow);
    makeRowVisible(row);

    selection.clear(); //clear selection, do NOT fire event
    selectRangeAndNotify(anchorRow, row); //set new selection + fire event

    mainWin_->Refresh();
    rowLabelWin_->Refresh();
}


void Grid::makeRowVisible(size_t row)
{
    const wxRect labelRect = rowLabelWin_->getRowLabelArea(row); //returns empty rect if column not found
    if (labelRect.height > 0)
    {
        int scrollPosX = 0;
        GetViewStart(&scrollPosX, nullptr);

        int pixelsPerUnitY = 0;
        GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
        if (pixelsPerUnitY <= 0) return;

        const int clientPosY = CalcScrolledPosition(labelRect.GetTopLeft()).y;
        if (clientPosY < 0)
        {
            const int scrollPosY = labelRect.GetTopLeft().y / pixelsPerUnitY;
            Scroll(scrollPosX, scrollPosY);
            updateWindowSizes(); //may show horizontal scroll bar
        }
        else if (clientPosY + labelRect.GetHeight() > rowLabelWin_->GetClientSize().GetHeight())
        {
            auto execScroll = [&](int clientHeight)
            {
                const int scrollPosY = std::ceil((labelRect.GetTopLeft().y - clientHeight +
                                                  labelRect.GetHeight()) / static_cast<double>(pixelsPerUnitY));
                Scroll(scrollPosX, scrollPosY);
                updateWindowSizes(); //may show horizontal scroll bar
            };

            const int clientHeightBefore = rowLabelWin_->GetClientSize().GetHeight();
            execScroll(clientHeightBefore);

            //client height may decrease after scroll due to a new horizontal scrollbar, resulting in a partially visible last row
            const int clientHeightAfter = rowLabelWin_->GetClientSize().GetHeight();
            if (clientHeightAfter < clientHeightBefore)
                execScroll(clientHeightAfter);
        }
    }
}


void Grid::selectRangeAndNotify(ptrdiff_t rowFrom, ptrdiff_t rowTo, bool positive)
{
    //sort + convert to half-open range
    auto rowFirst = std::min(rowFrom, rowTo);
    auto rowLast  = std::max(rowFrom, rowTo) + 1;

    const size_t rowCount = getRowCount();
    numeric::clamp<ptrdiff_t>(rowFirst, 0, rowCount);
    numeric::clamp<ptrdiff_t>(rowLast,  0, rowCount);

    selection.selectRange(rowFirst, rowLast, positive);

    //notify event
    GridRangeSelectEvent selectionEvent(rowFirst, rowLast, positive);
    if (wxEvtHandler* evtHandler = GetEventHandler())
        evtHandler->ProcessEvent(selectionEvent);

    mainWin_->Refresh();
}


void Grid::scrollTo(size_t row)
{
    const wxRect labelRect = rowLabelWin_->getRowLabelArea(row); //returns empty rect if column not found
    if (labelRect.height > 0)
    {
        int pixelsPerUnitY = 0;
        GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
        if (pixelsPerUnitY > 0)
        {
            const int scrollPosYNew = labelRect.GetTopLeft().y / pixelsPerUnitY;
            int scrollPosXOld = 0;
            int scrollPosYOld = 0;
            GetViewStart(&scrollPosXOld, &scrollPosYOld);

            if (scrollPosYOld != scrollPosYNew) //support polling
            {
                Scroll(scrollPosXOld, scrollPosYNew);
                updateWindowSizes(); //may show horizontal scroll bar
                Refresh();
            }
        }
    }
}


size_t Grid::getGridCursor() const
{
    return mainWin_->getCursor();
}


int Grid::getBestColumnSize(size_t col) const
{
    if (dataView_ && col < visibleCols.size())
    {
        const ColumnType type = visibleCols[col].type_;

        wxClientDC dc(mainWin_);
        dc.SetFont(mainWin_->GetFont()); //harmonize with MainWin::render()

        int maxSize = 0;

        auto rowRange = rowLabelWin_->getRowsOnClient(mainWin_->GetClientRect()); //returns range [begin, end)
        for (auto row = rowRange.first; row < rowRange.second; ++row)
            maxSize = std::max(maxSize, dataView_->getBestSize(dc, row, type));

        return maxSize;
    }
    return -1;
}


void Grid::setColumnWidth(int width, size_t col, GridEventPolicy columnResizeEventPolicy, bool notifyAsync)
{
    if (col < visibleCols.size())
    {
        VisibleColumn& vcRs = visibleCols[col];

        const std::vector<int> stretchedWidths = getColStretchedWidths(mainWin_->GetClientSize().GetWidth());
        if (stretchedWidths.size() != visibleCols.size())
        {
            assert(false);
            return;
        }
        //CAVEATS:
        //I. fixed-size columns: normalize offset so that resulting width is at least COLUMN_MIN_WIDTH: this is NOT enforced by getColWidths()!
        //II. stretched columns: do not allow user to set offsets so small that they result in negative (non-normalized) widths: this gives an
        //unusual delay when enlarging the column again later
        width = std::max(width, COLUMN_MIN_WIDTH);

        vcRs.offset_ = width - stretchedWidths[col]; //width := stretchedWidth + offset

        //III. resizing any column should normalize *all* other stretched columns' offsets considering current mainWinWidth!
        // test case:
        //1. have columns, both fixed-size and stretched, fit whole window width
        //2. shrink main window width so that horizontal scrollbars are shown despite the streched column
        //3. shrink a fixed-size column so that the scrollbars vanish and columns cover full width again
        //4. now verify that the stretched column is resizing immediately if main window is enlarged again
        for (size_t col2 = 0; col2 < visibleCols.size(); ++col2)
            if (visibleCols[col2].stretch_ > 0) //normalize stretched columns only
                visibleCols[col2].offset_ = std::max(visibleCols[col2].offset_, COLUMN_MIN_WIDTH - stretchedWidths[col2]);

        if (columnResizeEventPolicy == ALLOW_GRID_EVENT)
        {
            GridColumnResizeEvent sizeEvent(vcRs.offset_, vcRs.type_);
            if (wxEvtHandler* evtHandler = GetEventHandler())
            {
                if (notifyAsync)
                    evtHandler->AddPendingEvent(sizeEvent);
                else
                    evtHandler->ProcessEvent(sizeEvent);
            }
        }
    }
    else
        assert(false);
}


void Grid::autoSizeColumns(GridEventPolicy columnResizeEventPolicy)
{
    if (allowColumnResize)
    {
        for (size_t col = 0; col < visibleCols.size(); ++col)
        {
            const int bestWidth = getBestColumnSize(col); //return -1 on error
            if (bestWidth >= 0)
                setColumnWidth(bestWidth, col, columnResizeEventPolicy, true);
        }
        updateWindowSizes();
        Refresh();
    }
}


std::vector<int> Grid::getColStretchedWidths(int clientWidth) const //final width = (normalized) (stretchedWidth + offset)
{
    assert(clientWidth >= 0);
    clientWidth = std::max(clientWidth, 0);
    int stretchTotal = 0;
    for (const VisibleColumn& vc : visibleCols)
    {
        assert(vc.stretch_ >= 0);
        stretchTotal += vc.stretch_;
    }

    int remainingWidth = clientWidth;

    std::vector<int> output;

    if (stretchTotal <= 0)
        output.resize(visibleCols.size()); //fill with zeros
    else
        for (const VisibleColumn& vc : visibleCols)
        {
            const int width = clientWidth * vc.stretch_ / stretchTotal; //rounds down!
            output.push_back(width);
            remainingWidth -= width;
        }

    //distribute *all* of clientWidth: should suffice to enlarge the first few stretched columns; no need to minimize total absolute error of distribution
    if (stretchTotal > 0)
        if (remainingWidth > 0)
        {
            for (size_t col2 = 0; col2 < visibleCols.size(); ++col2)
                if (visibleCols[col2].stretch_ > 0)
                {
                    ++output[col2];
                    if (--remainingWidth == 0)
                        return output;
                }
            assert(false);
        }
    return output;
}


std::vector<Grid::ColumnWidth> Grid::getColWidths() const
{
    return getColWidths(mainWin_->GetClientSize().GetWidth());
}


std::vector<Grid::ColumnWidth> Grid::getColWidths(int mainWinWidth) const //evaluate stretched columns
{
    const std::vector<int> stretchedWidths = getColStretchedWidths(mainWinWidth);
    assert(stretchedWidths.size() == visibleCols.size());

    std::vector<ColumnWidth> output;
    for (size_t col2 = 0; col2 < visibleCols.size(); ++col2)
    {
        const auto& vc = visibleCols[col2];
        int width = stretchedWidths[col2] + vc.offset_;

        if (vc.stretch_ > 0)
            width = std::max(width, COLUMN_MIN_WIDTH); //normalization really needed here: e.g. smaller main window would result in negative width
        else
            width = std::max(width, 0); //support smaller width than COLUMN_MIN_WIDTH if set via configuration

        output.emplace_back(vc.type_, width);
    }
    return output;
}


int Grid::getColWidthsSum(int mainWinWidth) const
{
    int sum = 0;
    for (const ColumnWidth& cw : getColWidths(mainWinWidth))
        sum += cw.width_;
    return sum;
};
