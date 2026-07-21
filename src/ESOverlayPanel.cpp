#include "ESOverlayPanel.h"
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

wxBEGIN_EVENT_TABLE(ESOverlayPanel, wxPanel)
    EVT_PAINT(ESOverlayPanel::OnPaint)
    EVT_SIZE(ESOverlayPanel::OnSize)
wxEND_EVENT_TABLE()

ESOverlayPanel::ESOverlayPanel(wxWindow* parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size)
    : wxPanel(parent, id, pos, size, wxBORDER_SIMPLE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxWHITE);
}

void ESOverlayPanel::SetChartData(double esPrice, const KeyLevels& keyLevels)
{
    m_esPrice   = esPrice;
    m_keyLevels = keyLevels;

    // ── 生成模拟 ES 历史价格走势 ────────────────────────────
    // 以当前价格为中心，模拟约 50 个数据点的随机漫步价格路径
    m_priceHistory.clear();
    const int NUM_POINTS = 50;
    std::mt19937 rng(12345);  // 固定种子保证可复现
    std::normal_distribution<double> dist(0.0, 8.0);  // 每步波动 ~8 点

    double price = esPrice - 30.0;  // 从略低于现价开始
    for (int i = 0; i < NUM_POINTS; ++i)
    {
        // 添加均值回归趋势
        double drift = (esPrice - price) * 0.02;
        double step = dist(rng) + drift;
        price += step;

        // 价格不能偏离太远
        if (price < esPrice - 80.0)  price += 5.0;
        if (price > esPrice + 80.0)  price -= 5.0;

        m_priceHistory.push_back(price);
    }

    // 最后一个点强制对齐到当前价格附近
    if (!m_priceHistory.empty())
    {
        m_priceHistory.back() = esPrice + dist(rng) * 0.5;
    }

    Refresh();
}

// ═══════════════════════════════════════════════════════════════
//  事件处理
// ═══════════════════════════════════════════════════════════════

void ESOverlayPanel::OnPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    if (m_priceHistory.empty())
    {
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

void ESOverlayPanel::OnSize(wxSizeEvent& /*event*/)
{
    Refresh();
}

// ═══════════════════════════════════════════════════════════════
//  坐标转换
// ═══════════════════════════════════════════════════════════════

int ESOverlayPanel::PriceToY(double price, double minPrice, double maxPrice) const
{
    wxSize sz = GetClientSize();
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;
    if (chartH <= 0) return MARGIN_TOP;

    double range = maxPrice - minPrice;
    if (range <= 0) return MARGIN_TOP;

    double ratio = (price - minPrice) / range;
    return MARGIN_TOP + chartH - static_cast<int>(ratio * chartH);
}

int ESOverlayPanel::IndexToX(int index, int totalPoints) const
{
    wxSize sz = GetClientSize();
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    if (chartW <= 0 || totalPoints <= 1) return MARGIN_LEFT;

    return MARGIN_LEFT + static_cast<int>(
        (double)index / (totalPoints - 1) * chartW);
}

// ═══════════════════════════════════════════════════════════════
//  主绘制函数
// ═══════════════════════════════════════════════════════════════

void ESOverlayPanel::DrawChart(wxAutoBufferedPaintDC& dc)
{
    wxSize sz = GetClientSize();

    // 绘制背景
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, sz.GetWidth(), sz.GetHeight());

    // 计算 Y 轴范围 (以现价为中心, ±50 点)
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;
    double minPrice = m_esPrice - 50.0;
    double maxPrice = m_esPrice + 50.0;

    // 扩展范围以包含所有关键位
    auto expand = [&](double level) {
        if (level > 0)
        {
            minPrice = std::min(minPrice, level - 10.0);
            maxPrice = std::max(maxPrice, level + 10.0);
        }
    };
    expand(m_keyLevels.callWall_ES);
    expand(m_keyLevels.putWall_ES);
    expand(m_keyLevels.zeroGamma_ES);

    // 也考虑历史价格范围
    for (double p : m_priceHistory)
    {
        minPrice = std::min(minPrice, p - 5.0);
        maxPrice = std::max(maxPrice, p + 5.0);
    }

    // 按顺序绘制
    DrawGrid(dc, minPrice, maxPrice);
    DrawESLine(dc, minPrice, maxPrice);
    DrawOverlayLines(dc, minPrice, maxPrice);

    // 标题
    dc.SetTextForeground(wxColour(0, 0, 0));
    dc.SetFont(wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    dc.DrawText("E-mini S&P 500 (ES) 期货", 10, 3);
}

/**
 * DrawGrid - 绘制图表网格线和坐标轴
 */
void ESOverlayPanel::DrawGrid(wxAutoBufferedPaintDC& dc,
                              double minPrice, double maxPrice)
{
    wxSize sz = GetClientSize();
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    int chartH = sz.GetHeight() - MARGIN_TOP - MARGIN_BOTTOM;

    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    // ── 水平网格线 (Y 轴刻度) ─────────────────────────────
    int numYTicks = 6;
    for (int i = 0; i <= numYTicks; ++i)
    {
        double ratio = (double)i / numYTicks;
        double price = minPrice + ratio * (maxPrice - minPrice);
        int y = PriceToY(price, minPrice, maxPrice);

        // 网格线 (浅灰虚线)
        dc.SetPen(wxPen(wxColour(208, 208, 208), 1, wxPENSTYLE_DOT));
        dc.DrawLine(MARGIN_LEFT, y, MARGIN_LEFT + chartW, y);

        // Y 轴标签
        dc.SetTextForeground(wxColour(0, 0, 0));
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << price;
        wxString label = oss.str();
        wxSize labelSz = dc.GetTextExtent(label);
        dc.DrawText(label, MARGIN_LEFT - labelSz.GetWidth() - 4,
                    y - labelSz.GetHeight() / 2);
    }

    // ── 边框 ──────────────────────────────────────────────
    dc.SetPen(wxPen(wxColour(128, 128, 128), 1));
    dc.DrawLine(MARGIN_LEFT, MARGIN_TOP, MARGIN_LEFT, MARGIN_TOP + chartH);
    dc.DrawLine(MARGIN_LEFT, MARGIN_TOP + chartH,
                MARGIN_LEFT + chartW, MARGIN_TOP + chartH);

    // ── X 轴时间标签 ──────────────────────────────────────
    dc.SetTextForeground(wxColour(0, 0, 0));
    int bottomY = MARGIN_TOP + chartH;
    dc.DrawText("09:30", MARGIN_LEFT, bottomY + 8);
    dc.DrawText("12:00", MARGIN_LEFT + chartW / 2 - 20, bottomY + 8);
    dc.DrawText("16:00", MARGIN_LEFT + chartW - 40, bottomY + 8);
}

/**
 * DrawESLine - 绘制 ES 价格折线图
 * 使用蓝色粗线连接各数据点，并在当前收盘位置标注价格
 */
void ESOverlayPanel::DrawESLine(wxAutoBufferedPaintDC& dc,
                                double minPrice, double maxPrice)
{
    int n = static_cast<int>(m_priceHistory.size());
    if (n < 2) return;

    // ── 价格折线 ──────────────────────────────────────────
    dc.SetPen(wxPen(wxColour(0, 0, 255), 2, wxPENSTYLE_SOLID));

    for (int i = 0; i < n - 1; ++i)
    {
        int x1 = IndexToX(i, n);
        int y1 = PriceToY(m_priceHistory[i], minPrice, maxPrice);
        int x2 = IndexToX(i + 1, n);
        int y2 = PriceToY(m_priceHistory[i + 1], minPrice, maxPrice);
        dc.DrawLine(x1, y1, x2, y2);
    }

    // ── 最后一个数据点 (当前价格标记) ─────────────────────
    int lastX = IndexToX(n - 1, n);
    int lastY = PriceToY(m_priceHistory.back(), minPrice, maxPrice);

    // 绘制当前价格圆点
    dc.SetBrush(wxBrush(wxColour(0, 0, 255)));
    dc.SetPen(wxPen(wxColour(0, 0, 192), 1));
    dc.DrawCircle(lastX, lastY, 4);

    // 当前价格标签
    dc.SetTextForeground(wxColour(0, 0, 128));
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << m_esPrice;
    dc.DrawText(oss.str(), lastX + 6, lastY - 10);

    // 价格区域填充 (半透明浅蓝)
    int areaRight = IndexToX(n - 1, n);
    int areaBottom = PriceToY(minPrice, minPrice, maxPrice);
    dc.SetPen(wxPen(wxColour(0, 0, 255, 30), 1));
    dc.SetBrush(wxBrush(wxColour(0, 0, 255, 15)));
    std::vector<wxPoint> points;
    points.push_back(wxPoint(MARGIN_LEFT, areaBottom));
    for (int i = 0; i < n; ++i)
    {
        points.push_back(wxPoint(IndexToX(i, n),
                          PriceToY(m_priceHistory[i], minPrice, maxPrice)));
    }
    points.push_back(wxPoint(areaRight, areaBottom));
    dc.DrawPolygon(static_cast<int>(points.size()), points.data());
}

/**
 * DrawOverlayLines - 在 ES 价格图上叠加支撑阻力线
 *
 * 三条横向虚线:
 *   - 红色:  Put Wall (强支撑线)
 *   - 绿色:  Call Wall (强阻力线)
 *   - 黄色:  Zero Gamma (多空分界线)
 *
 * 每个标签包含 ES 点位数值
 */
void ESOverlayPanel::DrawOverlayLines(wxAutoBufferedPaintDC& dc,
                                      double minPrice, double maxPrice)
{
    wxSize sz = GetClientSize();
    int chartW = sz.GetWidth() - MARGIN_LEFT - MARGIN_RIGHT;
    int lineRight = MARGIN_LEFT + chartW;

    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

    // ── Put Wall (强支撑线) - 红色虚线 ─────────────────────
    if (m_keyLevels.putWall_ES > 0 &&
        m_keyLevels.putWall_ES >= minPrice &&
        m_keyLevels.putWall_ES <= maxPrice)
    {
        int y = PriceToY(m_keyLevels.putWall_ES, minPrice, maxPrice);

        // 虚线
        dc.SetPen(wxPen(wxColour(204, 0, 0), 2, wxPENSTYLE_LONG_DASH));
        dc.DrawLine(MARGIN_LEFT, y, lineRight, y);

        // 标签 (右侧)
        std::ostringstream oss;
        oss << "Put Wall " << std::fixed << std::setprecision(0)
            << m_keyLevels.putWall_ES << " (ES)";
        dc.SetTextForeground(wxColour(204, 0, 0));
        dc.DrawText(oss.str(), lineRight + 4, y - 8);

        // 图例标注 (左侧)
        dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        dc.DrawText("强支撑", MARGIN_LEFT + 2, y - 14);
    }

    // ── Call Wall (强阻力线) - 绿色虚线 ────────────────────
    if (m_keyLevels.callWall_ES > 0 &&
        m_keyLevels.callWall_ES >= minPrice &&
        m_keyLevels.callWall_ES <= maxPrice)
    {
        int y = PriceToY(m_keyLevels.callWall_ES, minPrice, maxPrice);

        dc.SetPen(wxPen(wxColour(0, 128, 0), 2, wxPENSTYLE_LONG_DASH));
        dc.DrawLine(MARGIN_LEFT, y, lineRight, y);

        std::ostringstream oss;
        oss << "Call Wall " << std::fixed << std::setprecision(0)
            << m_keyLevels.callWall_ES << " (ES)";
        dc.SetTextForeground(wxColour(0, 128, 0));
        dc.DrawText(oss.str(), lineRight + 4, y - 8);

        dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        dc.DrawText("强阻力", MARGIN_LEFT + 2, y - 14);
    }

    // ── Zero Gamma (多空分界线) - 橙色/黄色虚线 ─────────────
    if (m_keyLevels.zeroGamma_ES > 0 &&
        m_keyLevels.zeroGamma_ES >= minPrice &&
        m_keyLevels.zeroGamma_ES <= maxPrice)
    {
        int y = PriceToY(m_keyLevels.zeroGamma_ES, minPrice, maxPrice);

        dc.SetPen(wxPen(wxColour(128, 128, 128), 2, wxPENSTYLE_LONG_DASH));
        dc.DrawLine(MARGIN_LEFT, y, lineRight, y);

        std::ostringstream oss;
        oss << "Zero Gamma " << std::fixed << std::setprecision(0)
            << m_keyLevels.zeroGamma_ES << " (ES)";
        dc.SetTextForeground(wxColour(128, 128, 128));
        dc.DrawText(oss.str(), lineRight + 4, y - 8);

        dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        dc.DrawText("多空分界", MARGIN_LEFT + 2, y - 14);
    }
}
