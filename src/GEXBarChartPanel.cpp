#include "GEXBarChartPanel.h"
#include <wx/graphics.h>
#include <cmath>
#include <sstream>
#include <iomanip>

wxBEGIN_EVENT_TABLE(GEXBarChartPanel, wxPanel)
    EVT_PAINT(GEXBarChartPanel::OnPaint)
    EVT_SIZE(GEXBarChartPanel::OnSize)
wxEND_EVENT_TABLE()

GEXBarChartPanel::GEXBarChartPanel(wxWindow* parent,
                                   wxWindowID id,
                                   const wxPoint& pos,
                                   const wxSize& size)
    : wxPanel(parent, id, pos, size, wxBORDER_SIMPLE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);  // 双缓冲必需
    SetBackgroundColour(*wxWHITE);
}

void GEXBarChartPanel::SetChartData(const std::vector<GEXResult>& gexData,
                                    double spot,
                                    const KeyLevels& keyLevels)
{
    m_gexData   = gexData;
    m_spot      = spot;
    m_keyLevels = keyLevels;
    Refresh();  // 触发重绘
}

// ═══════════════════════════════════════════════════════════════
//  事件处理
// ═══════════════════════════════════════════════════════════════

void GEXBarChartPanel::OnPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    if (m_gexData.empty())
    {
        // 无数据时显示提示文字
        dc.SetTextForeground(wxColour(0, 0, 0));
        dc.SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        wxSize sz = GetClientSize();
        dc.DrawText("等待数据加载...",
                    (sz.GetWidth() - 120) / 2,
                    (sz.GetHeight() - 20) / 2);
        return;
    }

    DrawChart(dc);
}

void GEXBarChartPanel::OnSize(wxSizeEvent& /*event*/)
{
    Refresh();
}

// ═══════════════════════════════════════════════════════════════
//  坐标转换
// ═══════════════════════════════════════════════════════════════

int GEXBarChartPanel::StrikeToX(double strike,
                                double minStrike, double maxStrike) const
{
    wxSize sz = GetClientSize();
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    if (chartW <= 0) return MARGIN_LEFT;

    double range = maxStrike - minStrike;
    if (range <= 0) return MARGIN_LEFT;

    double ratio = (strike - minStrike) / range;
    return MARGIN_LEFT + static_cast<int>(ratio * chartW);
}

int GEXBarChartPanel::GEXToY(double gex, double maxAbsGEX) const
{
    wxSize sz = GetClientSize();
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;
    if (chartH <= 0) return MARGIN_TOP;

    // Y=0 位于图表中间
    int zeroY = MARGIN_TOP + chartH / 2;

    if (maxAbsGEX <= 0) return zeroY;

    // 正值向上 (Y 减小), 负值向下 (Y 增大)
    int offset = static_cast<int>((gex / maxAbsGEX) * (chartH / 2));
    return zeroY - offset;
}

// ═══════════════════════════════════════════════════════════════
//  主绘制函数
// ═══════════════════════════════════════════════════════════════

/**
 * DrawChart - 总体绘制流程
 * 先计算坐标范围，然后依次绘制坐标轴 → 柱状图 → 标注线
 */
void GEXBarChartPanel::DrawChart(wxAutoBufferedPaintDC& dc)
{
    // 计算数据范围
    double minStrike = m_gexData.front().strike;
    double maxStrike = m_gexData.back().strike;

    double maxAbsGEX = 0.0;
    for (const auto& r : m_gexData)
    {
        maxAbsGEX = std::max(maxAbsGEX, std::abs(r.netGEX));
    }
    if (maxAbsGEX <= 0) maxAbsGEX = 1e6;  // 防止除零

    // 绘制背景
    wxSize sz = GetClientSize();
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, sz.GetWidth(), sz.GetHeight());

    // 依次绘制各层
    DrawAxes(dc, minStrike, maxStrike, maxAbsGEX);
    DrawBars(dc, minStrike, maxStrike, maxAbsGEX);
    DrawKeyLevelMarkers(dc, minStrike, maxStrike, maxAbsGEX);

    // 绘制标题
    dc.SetTextForeground(wxColour(0, 0, 0));
    dc.SetFont(wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    dc.DrawText("SPY Net GEX (按行权价)", 10, 3);
}

/**
 * DrawAxes - 绘制坐标轴、刻度线和标签
 */
void GEXBarChartPanel::DrawAxes(wxAutoBufferedPaintDC& dc,
                                double minStrike, double maxStrike,
                                double maxAbsGEX)
{
    wxSize sz = GetClientSize();
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;

    int zeroY = MARGIN_TOP + chartH / 2;
    int rightX = MARGIN_LEFT + chartW;

    dc.SetPen(wxPen(wxColour(128, 128, 128), 1));
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    // ── Y=0 基准线 ──────────────────────────────────────
    dc.SetPen(wxPen(wxColour(100, 100, 100), 1, wxPENSTYLE_SOLID));
    dc.DrawLine(MARGIN_LEFT, zeroY, rightX, zeroY);

    // ── Y 轴 (左侧) ──────────────────────────────────────
    dc.SetPen(wxPen(wxColour(80, 80, 80), 1));
    dc.DrawLine(MARGIN_LEFT, MARGIN_TOP, MARGIN_LEFT, MARGIN_TOP + chartH);

    // Y 轴刻度 (以百万为单位)
    int numYTicks = 5;
    for (int i = 0; i <= numYTicks; ++i)
    {
        double ratio = (double)i / numYTicks;
        double gexVal = maxAbsGEX * ratio;

        // 正半轴
        int yPos = GEXToY(gexVal, maxAbsGEX);
        dc.DrawLine(MARGIN_LEFT - 4, yPos, MARGIN_LEFT, yPos);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (gexVal / 1e6) << "M";
        wxString label = oss.str();
        wxSize labelSz = dc.GetTextExtent(label);
        dc.DrawText(label, MARGIN_LEFT - labelSz.GetWidth() - 4, yPos - labelSz.GetHeight() / 2);

        // 负半轴 (对称)
        if (i > 0)
        {
            int yNeg = GEXToY(-gexVal, maxAbsGEX);
            dc.DrawLine(MARGIN_LEFT - 4, yNeg, MARGIN_LEFT, yNeg);

            std::ostringstream oss2;
            oss2 << std::fixed << std::setprecision(1) << (-gexVal / 1e6) << "M";
            wxString label2 = oss2.str();
            wxSize labelSz2 = dc.GetTextExtent(label2);
            dc.DrawText(label2, MARGIN_LEFT - labelSz2.GetWidth() - 4,
                        yNeg - labelSz2.GetHeight() / 2);
        }
    }

    // ── X 轴 (底部) ──────────────────────────────────────
    int bottomY = MARGIN_TOP + chartH;
    dc.DrawLine(MARGIN_LEFT, bottomY, rightX, bottomY);

    // X 轴刻度 (每隔几个 Strike 标注一个)
    double strikeRange = maxStrike - minStrike;
    int numXTicks = std::min(10, (int)m_gexData.size());
    double tickInterval = strikeRange / numXTicks;

    for (int i = 0; i <= numXTicks; ++i)
    {
        double strike = minStrike + i * tickInterval;
        int xPos = StrikeToX(strike, minStrike, maxStrike);

        dc.DrawLine(xPos, bottomY, xPos, bottomY + 4);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << strike;
        wxString label = oss.str();
        wxSize labelSz = dc.GetTextExtent(label);
        dc.DrawText(label, xPos - labelSz.GetWidth() / 2, bottomY + 6);
    }

    // ── Y 轴标题 ────────────────────────────────────────
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    dc.DrawText("GEX ($)", 4, MARGIN_TOP - 14);

    // ── X 轴标题 ────────────────────────────────────────
    dc.DrawText("Strike", rightX - 40, bottomY + 30);
}

/**
 * DrawBars - 绘制 Net GEX 柱状图
 * 正值 (Call 占优) → 绿色; 负值 (Put 占优) → 红色
 */
void GEXBarChartPanel::DrawBars(wxAutoBufferedPaintDC& dc,
                                double minStrike, double maxStrike,
                                double maxAbsGEX)
{
    wxSize sz = GetClientSize();
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;
    int zeroY = MARGIN_TOP + chartH / 2;

    // 计算柱宽 (基于相邻 Strike 的间距)
    int numBars = static_cast<int>(m_gexData.size());
    if (numBars < 2) return;

    double strikeRange = maxStrike - minStrike;
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    // 柱宽取可用宽度的 70%，留出柱子间距
    int barWidth = std::max(3, static_cast<int>((double)chartW / numBars * 0.7));

    for (const auto& r : m_gexData)
    {
        int xCenter = StrikeToX(r.strike, minStrike, maxStrike);
        int xLeft   = xCenter - barWidth / 2;
        int top     = GEXToY(r.netGEX, maxAbsGEX);

        // 正值 → 绿色 (看涨)，负值 → 红色 (看跌)
        if (r.netGEX >= 0)
        {
            dc.SetBrush(wxBrush(wxColour(0, 128, 0)));    // Win2K 经典绿色 (正 GEX)
            dc.SetPen(wxPen(wxColour(0, 96, 0), 1));
            // 从 Y=0 向上绘制
            dc.DrawRectangle(xLeft, top, barWidth, zeroY - top);
        }
        else
        {
            dc.SetBrush(wxBrush(wxColour(204, 0, 0)));    // Win2K 经典红色 (负 GEX)
            dc.SetPen(wxPen(wxColour(160, 0, 0), 1));
            int bottom = GEXToY(r.netGEX, maxAbsGEX);
            // 从 Y=0 向下绘制
            dc.DrawRectangle(xLeft, zeroY, barWidth, bottom - zeroY);
        }
    }

    // ── Spot 价格标记线 (虚线) ────────────────────────────
    int spotX = StrikeToX(m_spot, minStrike, maxStrike);
    dc.SetPen(wxPen(wxColour(0, 0, 128), 1, wxPENSTYLE_DOT));
    dc.DrawLine(spotX, MARGIN_TOP, spotX, MARGIN_TOP + chartH);

    // Spot 标签
    dc.SetTextForeground(wxColour(0, 0, 128));
    dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    std::ostringstream oss;
    oss << "Spot: " << std::fixed << std::setprecision(2) << m_spot;
    dc.DrawText(oss.str(), spotX + 3, MARGIN_TOP + 2);
}

/**
 * DrawKeyLevelMarkers - 在柱状图上叠加关键位标注
 * 在 Call Wall / Put Wall / Zero Gamma 对应 X 坐标处绘制竖线
 */
void GEXBarChartPanel::DrawKeyLevelMarkers(wxAutoBufferedPaintDC& dc,
                                           double minStrike, double maxStrike,
                                           double maxAbsGEX)
{
    wxSize sz = GetClientSize();
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;
    int bottomY = MARGIN_TOP + chartH;

    dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

    // ── Call Wall (绿线, 虚线) ────────────────────────────
    if (m_keyLevels.callWall_SPY > 0)
    {
        int x = StrikeToX(m_keyLevels.callWall_SPY, minStrike, maxStrike);
        dc.SetPen(wxPen(wxColour(0, 128, 0), 1, wxPENSTYLE_DOT));
        dc.DrawLine(x, MARGIN_TOP, x, bottomY);

        dc.SetTextForeground(wxColour(0, 128, 0));
        std::ostringstream oss;
        oss << "Call Wall " << std::fixed << std::setprecision(0)
            << m_keyLevels.callWall_SPY;
        dc.DrawText(oss.str(), x + 3, bottomY - 35);
    }

    // ── Put Wall (红线, 虚线) ─────────────────────────────
    if (m_keyLevels.putWall_SPY > 0)
    {
        int x = StrikeToX(m_keyLevels.putWall_SPY, minStrike, maxStrike);
        dc.SetPen(wxPen(wxColour(204, 0, 0), 1, wxPENSTYLE_DOT));
        dc.DrawLine(x, MARGIN_TOP, x, bottomY);

        dc.SetTextForeground(wxColour(204, 0, 0));
        std::ostringstream oss;
        oss << "Put Wall " << std::fixed << std::setprecision(0)
            << m_keyLevels.putWall_SPY;
        dc.DrawText(oss.str(), x + 3, bottomY - 20);
    }

    // ── Zero Gamma (黄线, 虚线) ──────────────────────────
    if (m_keyLevels.zeroGamma_SPY > 0)
    {
        int x = StrikeToX(m_keyLevels.zeroGamma_SPY, minStrike, maxStrike);
        dc.SetPen(wxPen(wxColour(128, 128, 128), 1, wxPENSTYLE_LONG_DASH));
        dc.DrawLine(x, MARGIN_TOP, x, bottomY);

        dc.SetTextForeground(wxColour(128, 128, 128));
        std::ostringstream oss;
        oss << "Zero Gamma " << std::fixed << std::setprecision(2)
            << m_keyLevels.zeroGamma_SPY;
        dc.DrawText(oss.str(), x + 3, bottomY - 50);
    }
}
