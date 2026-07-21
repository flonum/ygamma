#pragma once

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include "GEXEngine.h"
#include <vector>

/**
 * ESOverlayPanel - 右侧 ES 期货图表面板
 *
 * 使用双缓冲自绘 (wxAutoBufferedPaintDC) 渲染:
 *   1. ES 价格折线图 (模拟 K 线走势)
 *   2. 横向虚线的支撑阻力 Overlay:
 *      - 红色虚线: Put Wall (强支撑线)
 *      - 绿色虚线: Call Wall (强阻力线)
 *      - 黄色虚线: Zero Gamma Level (多空分界线)
 */
class ESOverlayPanel : public wxPanel
{
public:
    ESOverlayPanel(wxWindow* parent,
                   wxWindowID id = wxID_ANY,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize);

    /**
     * SetChartData - 设置图表数据并触发重绘
     * @param esPrice    当前 ES 期货价格
     * @param keyLevels  关键位 (已含 ES 映射后的值)
     */
    void SetChartData(double esPrice, const KeyLevels& keyLevels);

private:
    // ── 绘制事件 ──────────────────────────────────────────
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    // ── 内部绘制辅助 ──────────────────────────────────────
    /** 绘制主图表: 网格 → 折线 → Overlay */
    void DrawChart(wxAutoBufferedPaintDC& dc);

    /** 绘制图表网格和 Y 轴刻度 */
    void DrawGrid(wxAutoBufferedPaintDC& dc,
                  double minPrice, double maxPrice);

    /** 绘制 ES 价格折线图 */
    void DrawESLine(wxAutoBufferedPaintDC& dc,
                    double minPrice, double maxPrice);

    /** 叠加支撑阻力线和标签 */
    void DrawOverlayLines(wxAutoBufferedPaintDC& dc,
                          double minPrice, double maxPrice);

    // ── 坐标转换 ──────────────────────────────────────────
    /** 价格 → 像素 Y */
    int PriceToY(double price, double minPrice, double maxPrice) const;

    /** 索引导数 → 像素 X */
    int IndexToX(int index, int totalPoints) const;

    // ── 数据成员 ──────────────────────────────────────────
    double m_esPrice = 5750.0;
    KeyLevels m_keyLevels;

    // 模拟 ES 历史价格走势 (含波动和趋势)
    std::vector<double> m_priceHistory;

    // 边距常量
    static constexpr int MARGIN_LEFT   = 80;   // 左侧边距 (Y 轴标签)
    static constexpr int MARGIN_RIGHT  = 50;   // 右侧边距 (Overlay 标签)
    static constexpr int MARGIN_TOP    = 20;   // 顶部边距
    static constexpr int MARGIN_BOTTOM = 40;   // 底部边距 (X 轴标签)

    wxDECLARE_EVENT_TABLE();
};
