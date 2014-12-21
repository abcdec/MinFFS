// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "graph.h"
#include <cassert>
#include <algorithm>
#include <numeric>
#include <zen/basic_math.h>
#include <zen/scope_guard.h>
#include "dc.h"

using namespace zen;


//todo: support zoom via mouse wheel?

const wxEventType zen::wxEVT_GRAPH_SELECTION = wxNewEventType();

//for some buggy reason MSVC isn't able to use a temporary as a default argument
const std::shared_ptr<LabelFormatter> Graph2D::MainAttributes::defaultFormat = std::make_shared<DecimalNumberFormatter>();


double zen::nextNiceNumber(double blockSize) //round to next number which is a convenient to read block size
{
    if (blockSize <= 0)
        return 0;

    const double k = std::floor(std::log10(blockSize));
    const double e = std::pow(10, k);
    if (numeric::isNull(e))
        return 0;
    const double a = blockSize / e; //blockSize = a * 10^k with a in [1, 10)
    assert(1 <= a && a < 10);

    //have a look at leading two digits: "nice" numbers start with 1, 2, 2.5 and 5
    const double steps[] = { 1, 2, 2.5, 5, 10 };
    return e * numeric::nearMatch(a, std::begin(steps), std::end(steps));
}


namespace
{
wxColor getDefaultColor(size_t pos)
{
    switch (pos % 10)
    {
        case 0:
            return wxColor(0, 69, 134); //blue
        case 1:
            return wxColor(255, 66, 14); //red
        case 2:
            return wxColor(255, 211, 32); //yellow
        case 3:
            return wxColor(87, 157, 28); //green
        case 4:
            return wxColor(126, 0, 33); //royal
        case 5:
            return wxColor(131, 202, 255); //light blue
        case 6:
            return wxColor(49, 64, 4); //dark green
        case 7:
            return wxColor(174, 207, 0); //light green
        case 8:
            return wxColor(75, 31, 111); //purple
        case 9:
            return wxColor(255, 149, 14); //orange
    }
    assert(false);
    return *wxBLACK;
}


class ConvertCoord //convert between screen and input data coordinates
{
public:
    ConvertCoord(double valMin, double valMax, size_t screenSize) :
        min_(valMin),
        scaleToReal(screenSize == 0 ? 0 : (valMax - valMin) / screenSize),
        scaleToScr(numeric::isNull((valMax - valMin)) ? 0 : screenSize / (valMax - valMin)),
        outOfBoundsLow (-1 * scaleToReal + valMin),
        outOfBoundsHigh((screenSize + 1) * scaleToReal + valMin) { if (outOfBoundsLow > outOfBoundsHigh) std::swap(outOfBoundsLow, outOfBoundsHigh); }

    double screenToReal(double screenPos) const //map [0, screenSize] -> [valMin, valMax]
    {
        return screenPos * scaleToReal + min_;
    }
    double realToScreen(double realPos) const //return screen position in pixel (but with double precision!)
    {
        return (realPos - min_) * scaleToScr;
    }
    int realToScreenRound(double realPos) const //returns -1 and screenSize + 1 if out of bounds!
    {
        //catch large double values: if double is larger than what int can represent => undefined behavior!
        numeric::clamp(realPos , outOfBoundsLow, outOfBoundsHigh);
        return numeric::round(realToScreen(realPos));
    }

private:
    double min_;
    double scaleToReal;
    double scaleToScr;

    double outOfBoundsLow;
    double outOfBoundsHigh;
};


//enlarge value range to display to a multiple of a "useful" block size
void widenRange(double& valMin, double& valMax, //in/out
                int& blockCount, //out
                int graphAreaSize,      //in pixel
                int optimalBlockSizePx, //
                const LabelFormatter& labelFmt)
{
    if (graphAreaSize > 0)
    {
        double valRangePerBlock = (valMax - valMin) * optimalBlockSizePx / graphAreaSize; //proposal
        valRangePerBlock = labelFmt.getOptimalBlockSize(valRangePerBlock);
        if (numeric::isNull(valRangePerBlock)) //handle valMin == valMax
            valRangePerBlock = 1;
        warn_static("/|  arbitrary!?")

        int blockMin = std::floor(valMin / valRangePerBlock);
        int blockMax = std::ceil (valMax / valRangePerBlock);
        if (blockMin == blockMax) //handle valMin == valMax == integer
            ++blockMax;

        valMin = blockMin * valRangePerBlock;
        valMax = blockMax * valRangePerBlock;
        blockCount = blockMax - blockMin;
        return;
    }
    blockCount = 0;
}


void drawXLabel(wxDC& dc, double xMin, double xMax, int blockCount, const ConvertCoord& cvrtX, const wxRect& graphArea, const wxRect& labelArea, const LabelFormatter& labelFmt)
{
    assert(graphArea.width == labelArea.width && graphArea.x == labelArea.x);
    if (blockCount <= 0)
        return;

    wxDCPenChanger dummy(dc, wxPen(wxColor(192, 192, 192))); //light grey => not accessible! but no big deal...
    wxDCTextColourChanger dummy2(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

    const double valRangePerBlock = (xMax - xMin) / blockCount;

    for (int i = 1; i < blockCount; ++i)
    {
        //draw grey vertical lines
        const double valX = xMin + i * valRangePerBlock; //step over raw data, not graph area pixels, to not lose precision
        const int x = graphArea.x + cvrtX.realToScreenRound(valX);

        if (graphArea.height > 0)
            dc.DrawLine(wxPoint(x, graphArea.y), wxPoint(x, graphArea.y + graphArea.height)); //wxDC::DrawLine() doesn't draw last pixel

        //draw x axis labels
        const wxString label = labelFmt.formatText(valX, valRangePerBlock);
        const wxSize labelExtent = dc.GetMultiLineTextExtent(label);
        dc.DrawText(label, wxPoint(x - labelExtent.GetWidth() / 2, labelArea.y + (labelArea.height - labelExtent.GetHeight()) / 2)); //center
    }
}


void drawYLabel(wxDC& dc, double yMin, double yMax, int blockCount, const ConvertCoord& cvrtY, const wxRect& graphArea, const wxRect& labelArea, const LabelFormatter& labelFmt)
{
    assert(graphArea.height == labelArea.height && graphArea.y == labelArea.y);
    if (blockCount <= 0)
        return;

    wxDCPenChanger dummy(dc, wxPen(wxColor(192, 192, 192))); //light grey => not accessible! but no big deal...
    wxDCTextColourChanger dummy2(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

    const double valRangePerBlock = (yMax - yMin) / blockCount;

    for (int i = 1; i < blockCount; ++i)
    {
        //draw grey horizontal lines
        const double valY = yMin + i * valRangePerBlock; //step over raw data, not graph area pixels, to not lose precision
        const int y = graphArea.y + cvrtY.realToScreenRound(valY);

        if (graphArea.width > 0)
            dc.DrawLine(wxPoint(graphArea.x, y), wxPoint(graphArea.x + graphArea.width, y)); //wxDC::DrawLine() doesn't draw last pixel

        //draw y axis labels
        const wxString label = labelFmt.formatText(valY, valRangePerBlock);
        const wxSize labelExtent = dc.GetMultiLineTextExtent(label);
        dc.DrawText(label, wxPoint(labelArea.x + (labelArea.width - labelExtent.GetWidth()) / 2, y - labelExtent.GetHeight() / 2)); //center
    }
}


void drawCornerText(wxDC& dc, const wxRect& graphArea, const wxString& txt, Graph2D::PosCorner pos)
{
    if (txt.empty()) return;
    const int borderX = 5;
    const int borderY = 2; //it looks like wxDC::GetMultiLineTextExtent() precisely returns width, but too large a height: maybe they consider "text row height"?

    wxDCTextColourChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels
    wxSize txtExtent = dc.GetMultiLineTextExtent(txt);
    txtExtent.x += 2 * borderX;
    txtExtent.y += 2 * borderY;

    wxPoint drawPos = graphArea.GetTopLeft();
    switch (pos)
    {
        case Graph2D::CORNER_TOP_LEFT:
            break;
        case Graph2D::CORNER_TOP_RIGHT:
            drawPos.x += graphArea.width - txtExtent.GetWidth();
            break;
        case Graph2D::CORNER_BOTTOM_LEFT:
            drawPos.y += graphArea.height - txtExtent.GetHeight();
            break;
        case Graph2D::CORNER_BOTTOM_RIGHT:
            drawPos.x += graphArea.width  - txtExtent.GetWidth();
            drawPos.y += graphArea.height - txtExtent.GetHeight();
            break;
    }
    dc.DrawText(txt, drawPos + wxPoint(borderX, borderY));
}


//calculate intersection of polygon with half-plane
template <class Function, class Function2>
void cutPoints(std::vector<CurvePoint>& curvePoints, std::vector<char>& oobMarker, Function isInside, Function2 getIntersection)
{
    assert(curvePoints.size() == oobMarker.size());
    if (curvePoints.size() != oobMarker.size() || curvePoints.empty()) return;

    auto isMarkedOob = [&](size_t index) { return oobMarker[index] != 0; }; //test if point is start of a OOB line

    std::vector<CurvePoint> curvePointsTmp;
    std::vector<char>       oobMarkerTmp;
    curvePointsTmp.reserve(curvePoints.size()); //allocating memory for these containers is one
    oobMarkerTmp  .reserve(oobMarker  .size()); //of the more expensive operations of Graph2D!

    auto savePoint = [&](const CurvePoint& pt, bool markedOob) { curvePointsTmp.push_back(pt); oobMarkerTmp.push_back(markedOob); };

    bool pointInside = isInside(curvePoints[0]);
    if (pointInside)
        savePoint(curvePoints[0], isMarkedOob(0));

    for (size_t index = 1; index < curvePoints.size(); ++index)
    {
        if (isInside(curvePoints[index]) != pointInside)
        {
            pointInside = !pointInside;
            const CurvePoint is = getIntersection(curvePoints[index - 1],
                                                  curvePoints[index]); //getIntersection returns *it when delta is zero
            savePoint(is, !pointInside || isMarkedOob(index - 1));
        }
        if (pointInside)
            savePoint(curvePoints[index], isMarkedOob(index));
    }

    curvePointsTmp.swap(curvePoints);
    oobMarkerTmp  .swap(oobMarker);
}


struct GetIntersectionX
{
    GetIntersectionX(double x) : x_(x) {}
    CurvePoint operator()(const CurvePoint& from, const CurvePoint& to) const
    {
        const double deltaX = to.x - from.x;
        const double deltaY = to.y - from.y;
        return numeric::isNull(deltaX) ? to : CurvePoint(x_, from.y + (x_ - from.x) / deltaX * deltaY);
    };
private:
    double x_;
};

struct GetIntersectionY
{
    GetIntersectionY(double y) : y_(y) {}
    CurvePoint operator()(const CurvePoint& from, const CurvePoint& to) const
    {
        const double deltaX = to.x - from.x;
        const double deltaY = to.y - from.y;
        return numeric::isNull(deltaY) ? to : CurvePoint(from.x + (y_ - from.y) / deltaY * deltaX, y_);
    };
private:
    double y_;
};

void cutPointsOutsideX(std::vector<CurvePoint>& curvePoints, std::vector<char>& oobMarker, double minX, double maxX)
{
    assert(std::find(oobMarker.begin(), oobMarker.end(), true) == oobMarker.end());
    cutPoints(curvePoints, oobMarker, [&](const CurvePoint& pt) { return pt.x >= minX; }, GetIntersectionX(minX));
    cutPoints(curvePoints, oobMarker, [&](const CurvePoint& pt) { return pt.x <= maxX; }, GetIntersectionX(maxX));
}

void cutPointsOutsideY(std::vector<CurvePoint>& curvePoints, std::vector<char>& oobMarker, double minY, double maxY)
{
    cutPoints(curvePoints, oobMarker, [&](const CurvePoint& pt) { return pt.y >= minY; }, GetIntersectionY(minY));
    cutPoints(curvePoints, oobMarker, [&](const CurvePoint& pt) { return pt.y <= maxY; }, GetIntersectionY(maxY));
}
}


void ContinuousCurveData::getPoints(double minX, double maxX, int pixelWidth, std::vector<CurvePoint>& points) const
{
    if (pixelWidth <= 1) return;
    const ConvertCoord cvrtX(minX, maxX, pixelWidth - 1); //map [minX, maxX] to [0, pixelWidth - 1]

    const std::pair<double, double> rangeX = getRangeX();

    const double screenLow  = cvrtX.realToScreen(std::max(rangeX.first,  minX)); //=> xLow >= 0
    const double screenHigh = cvrtX.realToScreen(std::min(rangeX.second, maxX)); //=> xHigh <= pixelWidth - 1
    //if double is larger than what int can represent => undefined behavior!
    //=> convert to int not before checking value range!
    if (screenLow <= screenHigh)
    {
        const int posFrom = std::ceil (screenLow ); //do not step outside [minX, maxX] in loop below!
        const int posTo   = std::floor(screenHigh); //
        //conversion from std::floor/std::ceil double return value to int is loss-free for full value range of 32-bit int! tested successfully on MSVC

        for (int i = posFrom; i <= posTo; ++i)
        {
            const double x = cvrtX.screenToReal(i);
            points.emplace_back(x, getValue(x));
        }
    }
}


void SparseCurveData::getPoints(double minX, double maxX, int pixelWidth, std::vector<CurvePoint>& points) const
{
    if (pixelWidth <= 1) return;
    const ConvertCoord cvrtX(minX, maxX, pixelWidth - 1); //map [minX, maxX] to [0, pixelWidth - 1]
    const std::pair<double, double> rangeX = getRangeX();

    auto addPoint = [&](const CurvePoint& pt)
    {
        if (!points.empty())
        {
            if (pt.x <= points.back().x) //allow ascending x-positions only! algorithm below may cause double-insertion after empty x-ranges!
                return;

            if (addSteps_)
                if (pt.y != points.back().y)
                    points.emplace_back(CurvePoint(pt.x, points.back().y)); //[!] aliasing parameter not yet supported via emplace_back: VS bug! => make copy
        }
        points.push_back(pt);
    };

    const int posFrom = cvrtX.realToScreenRound(std::max(rangeX.first,  minX));
    const int posTo   = cvrtX.realToScreenRound(std::min(rangeX.second, maxX));

    for (int i = posFrom; i <= posTo; ++i)
    {
        const double x = cvrtX.screenToReal(i);
        Opt<CurvePoint> ptLe = getLessEq(x);
        Opt<CurvePoint> ptGe = getGreaterEq(x);
        //both non-existent and invalid return values are mapped to out of expected range: => check on posLe/posGe NOT ptLe/ptGe in the following!
        const int posLe = ptLe ? cvrtX.realToScreenRound(ptLe->x) : i + 1;
        const int posGe = ptGe ? cvrtX.realToScreenRound(ptGe->x) : i - 1;
        assert(!ptLe || posLe <= i); //check for invalid return values
        assert(!ptGe || posGe >= i); //
        /*
        Breakdown of all combinations of posLe, posGe and expected action (n >= 1)
        Note: For every empty x-range of at least one pixel, both next and previous points must be saved to keep the interpolating line stable!!!

          posLe | posGe | action
        +-------+-------+--------
        | none  | none  | break
        |   i   | none  | save ptLe; break
        | i - n | none  | break;
        +-------+-------+--------
        | none  |   i   | save ptGe; continue
        |   i   |   i   | save one of ptLe, ptGe; continue
        | i - n |   i   | save ptGe; continue
        +-------+-------+--------
        | none  | i + n | save ptGe; jump to position posGe + 1
        |   i   | i + n | save ptLe; if n == 1: continue; else: save ptGe; jump to position posGe + 1
        | i - n | i + n | save ptLe, ptGe; jump to position posGe + 1
        +-------+-------+--------
        */
        if (posGe < i)
        {
            if (posLe == i)
                addPoint(*ptLe);
            break;
        }
        else if (posGe == i) //test if point would be mapped to pixel x-position i
        {
            if (posLe == i) //
                addPoint(x - ptLe->x < ptGe->x - x ? *ptLe : *ptGe);
            else
                addPoint(*ptGe);
        }
        else
        {
            if (posLe <= i)
                addPoint(*ptLe);

            if (posLe != i || posGe > i + 1)
            {
                addPoint(*ptGe);
                i = posGe; //skip sparse area: +1 will be added by for-loop!
            }
        }
    }
}


Graph2D::Graph2D(wxWindow* parent,
                 wxWindowID winid,
                 const wxPoint& pos,
                 const wxSize& size,
                 long style,
                 const wxString& name) : wxPanel(parent, winid, pos, size, style, name),
    labelFont(wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, L"Arial")
{
    Connect(wxEVT_PAINT, wxPaintEventHandler(Graph2D::onPaintEvent), nullptr, this);
    Connect(wxEVT_SIZE,  wxSizeEventHandler (Graph2D::onSizeEvent ), nullptr, this);
    //http://wiki.wxwidgets.org/Flicker-Free_Drawing
    Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(Graph2D::onEraseBackGround), nullptr, this);

    //SetDoubleBuffered(true); slow as hell!

    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(Graph2D::OnMouseLeftDown), nullptr, this);
    Connect(wxEVT_MOTION,    wxMouseEventHandler(Graph2D::OnMouseMovement), nullptr, this);
    Connect(wxEVT_LEFT_UP,   wxMouseEventHandler(Graph2D::OnMouseLeftUp),   nullptr, this);
    Connect(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(Graph2D::OnMouseCaptureLost), nullptr, this);
}


void Graph2D::onPaintEvent(wxPaintEvent& event)
{
    //wxAutoBufferedPaintDC dc(this); -> this one happily fucks up for RTL layout by not drawing the first column (x = 0)!
    BufferedPaintDC dc(*this, doubleBuffer);
    render(dc);
}


void Graph2D::OnMouseLeftDown(wxMouseEvent& event)
{
    activeSel = zen::make_unique<MouseSelection>(*this, event.GetPosition());

    if (!event.ControlDown())
        oldSel.clear();
    Refresh();
}


void Graph2D::OnMouseMovement(wxMouseEvent& event)
{
    if (activeSel.get())
    {
        activeSel->refCurrentPos() = event.GetPosition(); //corresponding activeSel->refSelection() is updated in Graph2D::render()
        Refresh();
    }
}


void Graph2D::OnMouseLeftUp(wxMouseEvent& event)
{
    if (activeSel.get())
    {
        if (activeSel->getStartPos() != activeSel->refCurrentPos()) //if it's just a single mouse click: discard selection
        {
            GraphSelectEvent selEvent(activeSel->refSelection()); //fire off GraphSelectEvent
            if (wxEvtHandler* handler = GetEventHandler())
                handler->AddPendingEvent(selEvent);

            oldSel.push_back(activeSel->refSelection()); //commit selection
        }

        activeSel.reset();
        Refresh();
    }
}


void Graph2D::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    activeSel.reset();
    Refresh();
}


void Graph2D::setCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca)
{
    curves_.clear();
    addCurve(data, ca);
}


void Graph2D::addCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca)
{
    CurveAttributes newAttr = ca;
    if (newAttr.autoColor)
        newAttr.setColor(getDefaultColor(curves_.size()));
    curves_.emplace_back(data, newAttr);
    Refresh();
}


void Graph2D::render(wxDC& dc) const
{
    using namespace numeric;

    //set label font right at the start so that it is considered by wxDC::GetTextExtent() below!
    dc.SetFont(labelFont);

    const wxRect clientRect = GetClientRect(); //DON'T use wxDC::GetSize()! DC may be larger than visible area!
    {
        //clear complete client area; set label background color
        const wxColor backCol = GetBackgroundColour(); //user-configurable!
        //wxPanel::GetClassDefaultAttributes().colBg :
        //wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
        wxDCPenChanger   dummy (dc, backCol);
        wxDCBrushChanger dummy2(dc, backCol);
        dc.DrawRectangle(clientRect);
    }

    /*
    -----------------------
    |        |   x-label  |
    -----------------------
    |y-label | graph area |
    |----------------------
    */
    wxRect graphArea  = clientRect;
    int xLabelPosY = clientRect.y;
    int yLabelPosX = clientRect.x;

    switch (attr.labelposX)
    {
        case X_LABEL_TOP:
            graphArea.y      += attr.xLabelHeight;
            graphArea.height -= attr.xLabelHeight;
            break;
        case X_LABEL_BOTTOM:
            xLabelPosY += clientRect.height - attr.xLabelHeight;
            graphArea.height -= attr.xLabelHeight;
            break;
        case X_LABEL_NONE:
            break;
    }
    switch (attr.labelposY)
    {
        case Y_LABEL_LEFT:
            graphArea.x     += attr.yLabelWidth;
            graphArea.width -= attr.yLabelWidth;
            break;
        case Y_LABEL_RIGHT:
            yLabelPosX += clientRect.width - attr.yLabelWidth;
            graphArea.width -= attr.yLabelWidth;
            break;
        case Y_LABEL_NONE:
            break;
    }

    {
        //paint graph background (excluding label area)
        wxDCPenChanger   dummy (dc, wxColour(130, 135, 144)); //medium grey, the same Win7 uses for other frame borders => not accessible! but no big deal...
        wxDCBrushChanger dummy2(dc, attr.backgroundColor);
        //accessibility: consider system text and background colors; small drawback: color of graphs is NOT connected to the background! => responsibility of client to use correct colors

        dc.DrawRectangle(graphArea);
        graphArea.Deflate(1, 1); //attention more wxWidgets design mistakes: behavior of wxRect::Deflate depends on object being const/non-const!!!
    }

    //set label areas respecting graph area border!
    const wxRect xLabelArea(graphArea.x, xLabelPosY, graphArea.width, attr.xLabelHeight);
    const wxRect yLabelArea(yLabelPosX, graphArea.y, attr.yLabelWidth, graphArea.height);
    const wxPoint graphAreaOrigin = graphArea.GetTopLeft();

    //detect x value range
    double minX = attr.minXauto ?  std::numeric_limits<double>::infinity() : attr.minX; //automatic: ensure values are initialized by first curve
    double maxX = attr.maxXauto ? -std::numeric_limits<double>::infinity() : attr.maxX; //
    for (auto it = curves_.begin(); it != curves_.end(); ++it)
        if (const CurveData* curve = it->first.get())
        {
            const std::pair<double, double> rangeX = curve->getRangeX();
            assert(rangeX.first <= rangeX.second + 1.0e-9);
            //GCC fucks up badly when comparing two *binary identical* doubles and finds "begin > end" with diff of 1e-18

            if (attr.minXauto)
                minX = std::min(minX, rangeX.first);
            if (attr.maxXauto)
                maxX = std::max(maxX, rangeX.second);
        }

    if (minX <= maxX && maxX - minX < std::numeric_limits<double>::infinity()) //valid x-range
    {
        int blockCountX = 0;
        //enlarge minX, maxX to a multiple of a "useful" block size
        if (attr.labelposX != X_LABEL_NONE && attr.labelFmtX.get())
            widenRange(minX, maxX, //in/out
                       blockCountX, //out
                       graphArea.width,
                       dc.GetTextExtent(L"100000000000000").GetWidth(),
                       *attr.labelFmtX);

        //get raw values + detect y value range
        double minY = attr.minYauto ?  std::numeric_limits<double>::infinity() : attr.minY; //automatic: ensure values are initialized by first curve
        double maxY = attr.maxYauto ? -std::numeric_limits<double>::infinity() : attr.maxY; //

        std::vector<std::vector<CurvePoint>> curvePoints(curves_.size());
        std::vector<std::vector<char>>       oobMarker  (curves_.size()); //effectively a std::vector<bool> marking points that start an out-of-bounds line

        for (size_t index = 0; index < curves_.size(); ++index)
            if (const CurveData* curve = curves_[index].first.get())
            {
                std::vector<CurvePoint>& points = curvePoints[index];
                auto&                    marker = oobMarker  [index];

                curve->getPoints(minX, maxX, graphArea.width, points);

                //cut points outside visible x-range now in order to calculate height of visible line fragments only!
                marker.resize(points.size()); //default value: false
                cutPointsOutsideX(points, marker, minX, maxX);

                if ((attr.minYauto || attr.maxYauto) && !points.empty())
                {
                    auto itPair = std::minmax_element(points.begin(), points.end(), [](const CurvePoint& lhs, const CurvePoint& rhs) { return lhs.y < rhs.y; });
                    if (attr.minYauto)
                        minY = std::min(minY, itPair.first->y);
                    if (attr.maxYauto)
                        maxY = std::max(maxY, itPair.second->y);
                }
            }

        if (minY <= maxY) //valid y-range
        {
            int blockCountY = 0;
            //enlarge minY, maxY to a multiple of a "useful" block size
            if (attr.labelposY != Y_LABEL_NONE && attr.labelFmtY.get())
                widenRange(minY, maxY, //in/out
                           blockCountY, //out
                           graphArea.height,
                           3 * dc.GetTextExtent(L"1").GetHeight(),
                           *attr.labelFmtY);

            if (graphArea.width <= 1 || graphArea.height <= 1) return;
            const ConvertCoord cvrtX(minX, maxX, graphArea.width  - 1); //map [minX, maxX] to [0, pixelWidth - 1]
            const ConvertCoord cvrtY(maxY, minY, graphArea.height - 1); //map [minY, maxY] to [pixelHeight - 1, 0]

            //calculate curve coordinates on graph area
            std::vector<std::vector<wxPoint>> drawPoints(curves_.size());

            for (size_t index = 0; index < curves_.size(); ++index)
            {
                //cut points outside visible y-range before calculating pixels:
                //1. realToScreenRound() deforms out-of-range values!
                //2. pixels that are grossly out of range can be a severe performance problem when drawing on the DC (Windows)
                cutPointsOutsideY(curvePoints[index], oobMarker[index], minY, maxY);

                auto& points = drawPoints[index];
                for (const CurvePoint& pt : curvePoints[index])
                    points.push_back(wxPoint(cvrtX.realToScreenRound(pt.x),
                                             cvrtY.realToScreenRound(pt.y)) + graphAreaOrigin);
            }

            //update active mouse selection
            if (activeSel.get() && graphArea.width > 0 && graphArea.height > 0)
            {
                auto widen = [](double* low, double* high)
                {
                    if (*low > *high)
                        std::swap(low, high);
                    *low  -= 0.5;
                    *high += 0.5;
                };

                const wxPoint screenStart   = activeSel->getStartPos()   - graphAreaOrigin; //make relative to graphArea
                const wxPoint screenCurrent = activeSel->refCurrentPos() - graphAreaOrigin;

                //normalize positions: a mouse selection is symmetric and *not* an half-open range!
                double screenFromX = clampCpy(screenStart  .x, 0, graphArea.width  - 1);
                double screenFromY = clampCpy(screenStart  .y, 0, graphArea.height - 1);
                double screenToX   = clampCpy(screenCurrent.x, 0, graphArea.width  - 1);
                double screenToY   = clampCpy(screenCurrent.y, 0, graphArea.height - 1);
                widen(&screenFromX, &screenToX); //use full pixel range for selection!
                widen(&screenFromY, &screenToY);

                //save current selection as "double" coordinates
                activeSel->refSelection().from = CurvePoint(cvrtX.screenToReal(screenFromX),
                                                            cvrtY.screenToReal(screenFromY));

                activeSel->refSelection().to = CurvePoint(cvrtX.screenToReal(screenToX),
                                                          cvrtY.screenToReal(screenToY));
            }

            //#################### begin drawing ####################
            //1. draw colored area under curves
            for (auto it = curves_.begin(); it != curves_.end(); ++it)
                if (it->second.drawCurveArea)
                {
                    std::vector<wxPoint> points = drawPoints[it - curves_.begin()];
                    if (!points.empty())
                    {
                        points.emplace_back(wxPoint(points.back ().x, graphArea.GetBottom())); //add lower right and left corners
                        points.emplace_back(wxPoint(points.front().x, graphArea.GetBottom())); //[!] aliasing parameter not yet supported via emplace_back: VS bug! => make copy

                        wxDCBrushChanger dummy(dc, it->second.fillColor);
                        wxDCPenChanger  dummy2(dc, it->second.fillColor);
                        dc.DrawPolygon(static_cast<int>(points.size()), &points[0]);
                    }
                }

            //2. draw all currently set mouse selections (including active selection)
            std::vector<SelectionBlock> allSelections = oldSel;
            if (activeSel)
                allSelections.push_back(activeSel->refSelection());
            {
                //alpha channel not supported on wxMSW, so draw selection before curves
                wxDCBrushChanger dummy(dc, wxColor(168, 202, 236)); //light blue
                wxDCPenChanger  dummy2(dc, wxColor(51,  153, 255)); //dark blue

                auto shrink = [](double* low, double* high)
                {
                    if (*low > *high)
                        std::swap(low, high);
                    *low  += 0.5;
                    *high -= 0.5;
                    if (*low > *high)
                        *low = *high = (*low + *high) / 2;
                };

                for (const SelectionBlock& sel : allSelections)
                {
                    //harmonize with active mouse selection above
                    double screenFromX = cvrtX.realToScreen(sel.from.x);
                    double screenFromY = cvrtY.realToScreen(sel.from.y);
                    double screenToX   = cvrtX.realToScreen(sel.to.x);
                    double screenToY   = cvrtY.realToScreen(sel.to.y);
                    shrink(&screenFromX, &screenToX);
                    shrink(&screenFromY, &screenToY);

                    clamp(screenFromX, 0.0, graphArea.width  - 1.0);
                    clamp(screenFromY, 0.0, graphArea.height - 1.0);
                    clamp(screenToX,   0.0, graphArea.width  - 1.0);
                    clamp(screenToY,   0.0, graphArea.height - 1.0);

                    const wxPoint pixelFrom = wxPoint(numeric::round(screenFromX),
                                                      numeric::round(screenFromY)) + graphAreaOrigin;
                    const wxPoint pixelTo = wxPoint(numeric::round(screenToX),
                                                    numeric::round(screenToY)) + graphAreaOrigin;
                    switch (attr.mouseSelMode)
                    {
                        case SELECT_NONE:
                            break;
                        case SELECT_RECTANGLE:
                            dc.DrawRectangle(wxRect(pixelFrom, pixelTo));
                            break;
                        case SELECT_X_AXIS:
                            dc.DrawRectangle(wxRect(wxPoint(pixelFrom.x, graphArea.y), wxPoint(pixelTo.x, graphArea.y + graphArea.height - 1)));
                            break;
                        case SELECT_Y_AXIS:
                            dc.DrawRectangle(wxRect(wxPoint(graphArea.x, pixelFrom.y), wxPoint(graphArea.x + graphArea.width - 1, pixelTo.y)));
                            break;
                    }
                }
            }

            //3. draw labels and background grid
            drawXLabel(dc, minX, maxX, blockCountX, cvrtX, graphArea, xLabelArea, *attr.labelFmtX);
            drawYLabel(dc, minY, maxY, blockCountY, cvrtY, graphArea, yLabelArea, *attr.labelFmtY);

            //4. finally draw curves
            {
                dc.SetClippingRegion(graphArea); //prevent thick curves from drawing slightly outside
                ZEN_ON_SCOPE_EXIT(dc.DestroyClippingRegion());

                for (auto it = curves_.begin(); it != curves_.end(); ++it)
                {
                    wxDCPenChanger dummy(dc, wxPen(it->second.color, it->second.lineWidth));

                    const size_t index = it - curves_.begin();
                    std::vector<wxPoint>& points = drawPoints[index]; //alas wxDC::DrawLines() is not const-correct!!!
                    auto&                 marker = oobMarker [index];
                    assert(points.size() == marker.size());

                    //draw all parts of the curve except for the out-of-bounds fragments
                    size_t drawIndexFirst = 0;
                    while (drawIndexFirst < points.size())
                    {
                        size_t drawIndexLast = std::find(marker.begin() + drawIndexFirst, marker.end(), true) - marker.begin();
                        if (drawIndexLast < points.size()) ++ drawIndexLast;

                        const int pointCount = static_cast<int>(drawIndexLast - drawIndexFirst);
                        if (pointCount > 0)
                        {
                            if (pointCount >= 2) //on OS X wxWidgets has a nasty assert on this
                                dc.DrawLines(pointCount, &points[drawIndexFirst]);
                            dc.DrawPoint(points[drawIndexLast - 1]); //wxDC::DrawLines() doesn't draw last pixel
                        }
                        drawIndexFirst = std::find(marker.begin() + drawIndexLast, marker.end(), false) - marker.begin();
                    }
                }
            }

            //5. draw corner texts
            for (const auto& ct : attr.cornerTexts)
                drawCornerText(dc, graphArea, ct.second, ct.first);
        }
    }
}
