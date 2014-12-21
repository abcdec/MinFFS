// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef WX_PLOT_HEADER_2344252459
#define WX_PLOT_HEADER_2344252459

#include <map>
#include <vector>
#include <memory>
#include <wx/panel.h>
#include <wx/settings.h>
#include <zen/string_tools.h>
#include <zen/optional.h>

//elegant 2D graph as wxPanel specialization

namespace zen
{
/*
Example:
    //init graph (optional)
    m_panelGraph->setAttributes(Graph2D::MainAttributes().
                                setLabelX(Graph2D::X_LABEL_BOTTOM, 20, std::make_shared<LabelFormatterTimeElapsed>()).
                                setLabelY(Graph2D::Y_LABEL_RIGHT,  60, std::make_shared<LabelFormatterBytes>()));
    //set graph data
    std::shared_ptr<CurveData> curveDataBytes = ...
	m_panelGraph->setCurve(curveDataBytes, Graph2D::CurveAttributes().setLineWidth(2).setColor(wxColor(0, 192, 0)));
*/

struct CurvePoint
{
    CurvePoint() : x(0), y(0) {}
    CurvePoint(double xVal, double yVal) : x(xVal), y(yVal) {}
    double x;
    double y;
};
inline bool operator==(const CurvePoint& lhs, const CurvePoint& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
inline bool operator!=(const CurvePoint& lhs, const CurvePoint& rhs) { return !(lhs == rhs); }

struct CurveData
{
    virtual ~CurveData() {}

    virtual std::pair<double, double> getRangeX() const = 0;
    virtual void getPoints(double minX, double maxX, int pixelWidth,
                           std::vector<CurvePoint>& points) const = 0; //points outside the draw area are automatically trimmed!
};

//special curve types:
struct ContinuousCurveData : public CurveData
{
    virtual double getValue(double x) const = 0;

private:
    void getPoints(double minX, double maxX, int pixelWidth, std::vector<CurvePoint>& points) const override;
};

struct SparseCurveData : public CurveData
{
    SparseCurveData(bool addSteps = false) : addSteps_(addSteps) {} //addSteps: add points to get a staircase effect or connect points via a direct line

    virtual Opt<CurvePoint> getLessEq   (double x) const = 0;
    virtual Opt<CurvePoint> getGreaterEq(double x) const = 0;

private:
    void getPoints(double minX, double maxX, int pixelWidth, std::vector<CurvePoint>& points) const override;
    bool addSteps_;
};

struct ArrayCurveData : public SparseCurveData
{
    virtual double getValue(size_t pos) const = 0;
    virtual size_t getSize ()           const = 0;

private:
    std::pair<double, double> getRangeX() const override { const size_t sz = getSize(); return std::make_pair(0.0, sz == 0 ? 0.0 : sz - 1.0); }

    Opt<CurvePoint> getLessEq(double x) const override
    {
        const size_t sz = getSize();
        const size_t pos = std::min<ptrdiff_t>(std::floor(x), sz - 1); //[!] expect unsigned underflow if empty!
        if (pos < sz)
            return CurvePoint(pos, getValue(pos));
        return NoValue();
    }

    Opt<CurvePoint> getGreaterEq(double x) const override
    {
        const size_t pos = std::max<ptrdiff_t>(std::ceil(x), 0); //[!] use std::max with signed type!
        if (pos < getSize())
            return CurvePoint(pos, getValue(pos));
        return NoValue();
    }
};

struct VectorCurveData : public ArrayCurveData
{
    std::vector<double>& refData() { return data; }
private:
    double getValue(size_t pos) const override { return pos < data.size() ? data[pos] : 0; }
    size_t getSize()            const override { return data.size(); }
    std::vector<double> data;
};

//------------------------------------------------------------------------------------------------------------

struct LabelFormatter
{
    virtual ~LabelFormatter() {}

    //determine convenient graph label block size in unit of data: usually some small deviation on "sizeProposed"
    virtual double getOptimalBlockSize(double sizeProposed) const = 0;

    //create human-readable text for x or y-axis position
    virtual wxString formatText(double value, double optimalBlockSize) const = 0;
};

double nextNiceNumber(double blockSize); //round to next number which is convenient to read, e.g. 2.13 -> 2; 2.7 -> 2.5

struct DecimalNumberFormatter : public LabelFormatter
{
    double   getOptimalBlockSize(double sizeProposed                  ) const override { return nextNiceNumber(sizeProposed); }
    wxString formatText         (double value, double optimalBlockSize) const override { return zen::numberTo<wxString>(value); }
};

//------------------------------------------------------------------------------------------------------------

//emit data selection event
//Usage: wnd.Connect(wxEVT_GRAPH_SELECTION, GraphSelectEventHandler(MyDlg::OnGraphSelection), nullptr, this);
//       void MyDlg::OnGraphSelection(GraphSelectEvent& event);

extern const wxEventType wxEVT_GRAPH_SELECTION;

struct SelectionBlock
{
    CurvePoint from;
    CurvePoint to;
};

class GraphSelectEvent : public wxCommandEvent
{
public:
    GraphSelectEvent(const SelectionBlock& selBlock) : wxCommandEvent(wxEVT_GRAPH_SELECTION), selBlock_(selBlock) {}
    wxEvent* Clone() const override { return new GraphSelectEvent(selBlock_); }

    SelectionBlock getSelection() { return selBlock_; }

private:
    SelectionBlock selBlock_;
};

typedef void (wxEvtHandler::*GraphSelectEventFunction)(GraphSelectEvent&);

#define GraphSelectEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GraphSelectEventFunction, &func)

//------------------------------------------------------------------------------------------------------------

class Graph2D : public wxPanel
{
public:
    Graph2D(wxWindow* parent,
            wxWindowID winid     = wxID_ANY,
            const wxPoint& pos   = wxDefaultPosition,
            const wxSize& size   = wxDefaultSize,
            long style           = wxTAB_TRAVERSAL | wxNO_BORDER,
            const wxString& name = wxPanelNameStr);

    class CurveAttributes
    {
    public:
        CurveAttributes() : autoColor(true), drawCurveArea(false), lineWidth(2) {}

        CurveAttributes& setColor     (const wxColour& col) { color = col; autoColor = false; return *this; }
        CurveAttributes& fillCurveArea(const wxColour& col) { fillColor = col; drawCurveArea = true; return *this; }
        CurveAttributes& setLineWidth(size_t width) { lineWidth = static_cast<int>(width); return *this; }

    private:
        friend class Graph2D;

        bool autoColor;
        wxColour color;

        bool drawCurveArea;
        wxColour fillColor;

        int lineWidth;
    };

    void setCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca = CurveAttributes());
    void addCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca = CurveAttributes());

    enum PosLabelY
    {
        Y_LABEL_LEFT,
        Y_LABEL_RIGHT,
        Y_LABEL_NONE
    };

    enum PosLabelX
    {
        X_LABEL_TOP,
        X_LABEL_BOTTOM,
        X_LABEL_NONE
    };

    enum PosCorner
    {
        CORNER_TOP_LEFT,
        CORNER_TOP_RIGHT,
        CORNER_BOTTOM_LEFT,
        CORNER_BOTTOM_RIGHT,
    };

    enum SelMode
    {
        SELECT_NONE,
        SELECT_RECTANGLE,
        SELECT_X_AXIS,
        SELECT_Y_AXIS,
    };

    class MainAttributes
    {
    public:
        MainAttributes() :
            minXauto(true),
            maxXauto(true),
            minX(0),
            maxX(0),
            minYauto(true),
            maxYauto(true),
            minY(0),
            maxY(0),
            labelposX(X_LABEL_BOTTOM),
            xLabelHeight(25),
            labelFmtX(std::make_shared<DecimalNumberFormatter>()),
            labelposY(Y_LABEL_LEFT),
            yLabelWidth(60),
            labelFmtY(std::make_shared<DecimalNumberFormatter>()),
            backgroundColor(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)),
            mouseSelMode(SELECT_RECTANGLE) {}

        MainAttributes& setMinX(double newMinX) { minX = newMinX; minXauto = false; return *this; }
        MainAttributes& setMaxX(double newMaxX) { maxX = newMaxX; maxXauto = false; return *this; }

        MainAttributes& setMinY(double newMinY) { minY = newMinY; minYauto = false; return *this; }
        MainAttributes& setMaxY(double newMaxY) { maxY = newMaxY; maxYauto = false; return *this; }

        MainAttributes& setAutoSize() { minXauto = maxXauto = minYauto = maxYauto = true; return *this; }

        static const std::shared_ptr<LabelFormatter> defaultFormat;

        MainAttributes& setLabelX(PosLabelX posX, size_t height = 25, const std::shared_ptr<LabelFormatter>& newLabelFmt = defaultFormat)
        {
            labelposX    = posX;
            xLabelHeight = static_cast<int>(height);
            labelFmtX    = newLabelFmt;
            return *this;
        }
        MainAttributes& setLabelY(PosLabelY posY, size_t width = 60, const std::shared_ptr<LabelFormatter>& newLabelFmt = defaultFormat)
        {
            labelposY    = posY;
            yLabelWidth  = static_cast<int>(width);
            labelFmtY    = newLabelFmt;
            return *this;
        }

        MainAttributes& setCornerText(const wxString& txt, PosCorner pos) { cornerTexts[pos] = txt; return *this; }

        MainAttributes& setBackgroundColor(const wxColour& col) { backgroundColor = col; return *this; }

        MainAttributes& setSelectionMode(SelMode mode) { mouseSelMode = mode; return *this; }

    private:
        friend class Graph2D;

        bool minXauto; //autodetect range for X value
        bool maxXauto;
        double minX; //x-range to visualize
        double maxX;

        bool minYauto; //autodetect range for Y value
        bool maxYauto;
        double minY; //y-range to visualize
        double maxY;

        PosLabelX labelposX;
        int xLabelHeight;
        std::shared_ptr<LabelFormatter> labelFmtX;

        PosLabelY labelposY;
        int yLabelWidth;
        std::shared_ptr<LabelFormatter> labelFmtY;

        std::map<PosCorner, wxString> cornerTexts;

        wxColour backgroundColor;
        SelMode mouseSelMode;
    };
    void setAttributes(const MainAttributes& newAttr) { attr = newAttr; Refresh(); }
    MainAttributes getAttributes() const { return attr; }

    std::vector<SelectionBlock> getSelections() const { return oldSel; }
    void setSelections(const std::vector<SelectionBlock>& sel)
    {
        oldSel = sel;
        activeSel.reset();
        Refresh();
    }
    void clearSelection() { oldSel.clear(); Refresh(); }

private:
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMovement(wxMouseEvent& event);
    void OnMouseLeftUp  (wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);

    void onPaintEvent(wxPaintEvent& event);
    void onSizeEvent(wxSizeEvent& event) { Refresh(); event.Skip(); }
    void onEraseBackGround(wxEraseEvent& event) {}

    void render(wxDC& dc) const;

    class MouseSelection
    {
    public:
        MouseSelection(wxWindow& wnd, const wxPoint& posDragStart) : wnd_(wnd), posDragStart_(posDragStart), posDragCurrent(posDragStart) { wnd_.CaptureMouse(); }
        ~MouseSelection() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

        wxPoint getStartPos() const { return posDragStart_; }
        wxPoint& refCurrentPos() { return posDragCurrent; }

        SelectionBlock& refSelection() { return selBlock; } //updated in Graph2d::render(): this is fine, since only what's shown is selected!

    private:
        wxWindow& wnd_;
        const wxPoint posDragStart_;
        wxPoint posDragCurrent;
        SelectionBlock selBlock;
    };
    std::vector<SelectionBlock>     oldSel; //applied selections
    std::shared_ptr<MouseSelection> activeSel; //set during mouse selection

    MainAttributes attr; //global attributes

    std::unique_ptr<wxBitmap> doubleBuffer;

    typedef std::vector<std::pair<std::shared_ptr<CurveData>, CurveAttributes>> CurveList;
    CurveList curves_;
    wxFont labelFont; //perf!!! generating the font is *very* expensive! don't do this repeatedly in Graph2D::render()!
};
}

#endif //WX_PLOT_HEADER_2344252459
