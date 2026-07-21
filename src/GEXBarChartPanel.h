#pragma once

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include "GEXEngine.h"
#include <vector>

/**
 * GEXBarChartPanel - 左侧 GEX 柱状图面板
 *
 * 使用双缓冲自绘 (wxAutoBufferedPaintDC) 渲染 Net GEX 柱状图:
 *   - 正值 (Call 占优) → 绿色柱
 *   - 负值 (Put 占优) → 红色柱
 *   - Y=0 基准线
 *   - 标注 Call Wall / Put Wall / Zero Gamma 位置
 */
class GEXBarChartPanel : public wxPanel
{
public:
    GEXBarChartPanel(wxWindow* parent,
                     wxWindowID id = wxID_ANY,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize);

    /**
     * SetChartData - 设置图表数据并触发重绘
     * @param gexData    GEX 汇总结果
     * @param spot       当前 SPY 现价
     * @param keyLevels  关键位 (用于标注)
     */
    void SetChartData(const std::vector<GEXResult>& gexData,
                      double spot,
                      const KeyLevels& keyLevels);

private:
    // ── 绘制事件 ──────────────────────────────────────────
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    // ── 内部绘制辅助 ──────────────────────────────────────
    /** 绘制整个图表: 坐标轴 → 柱状图 → 标注线 */
    void DrawChart(wxAutoBufferedPaintDC& dc);

    /** 绘制坐标轴和刻度标签 */
    void DrawAxes(wxAutoBufferedPaintDC& dc,
                  double minStrike, double maxStrike,
                  double maxAbsGEX);

    /** 绘制 GEX 柱状图 */
    void DrawBars(wxAutoBufferedPaintDC& dc,
                  double minStrike, double maxStrike,
                  double maxAbsGEX);

    /** 在柱状图上叠加标注关键位 */
    void DrawKeyLevelMarkers(wxAutoBufferedPaintDC& dc,
                             double minStrike, double maxStrike,
                             double maxAbsGEX);

    // ── 坐标转换 ──────────────────────────────────────────
    /** Strike → 像素 X */
    int StrikeToX(double strike, double minStrike, double maxStrike) const;

    /** GEX → 像素 Y */
    int GEXToY(double gex, double maxAbsGEX) const;

    // ── 数据成员 ──────────────────────────────────────────
    std::vector<GEXResult> m_gexData;
    double m_spot = 575.0;
    KeyLevels m_keyLevels;

    // 边距常量
    static constexpr int MARGIN_LEFT   = 60;   // 左侧边距 (Y 轴标签)
    static constexpr int MARGIN_RIGHT  = 20;   // 右侧边距
    static constexpr int MARGIN_TOP    = 20;   // 顶部边距 (标题)
    static constexpr int MARGIN_BOTTOM = 50;   // 底部边距 (X 轴标签)

    wxDECLARE_EVENT_TABLE();
};
