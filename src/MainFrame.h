#pragma once

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/timer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "GEXEngine.h"
#include "OptionDataFetcher.h"
#include "GEXBarChartPanel.h"
#include "ESOverlayPanel.h"

/**
 * MainFrame - 应用程序主窗口
 *
 * 布局结构:
 *   ┌──────────────────────────────────────────────┐
 *   │  (顶部控制栏) SPY Spot | ES Price | Basis | ...  │
 *   ├──────────────────────┬───────────────────────┤
 *   │  GEXBarChartPanel    │  ESOverlayPanel       │
 *   │  (左侧 40%)          │  (右侧 60%)            │
 *   └──────────────────────┴───────────────────────┘
 *
 * 职责: 协调 数据获取 → 计算引擎 → UI 更新 的完整数据流
 */
class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title);

private:
    // ── UI 创建 ────────────────────────────────────────────
    void CreateMenuBar();
    void CreateControlBar(wxWindow* parent);
    void CreateSplitterPanels();

    // ── 事件处理 ──────────────────────────────────────────
    void OnRefresh(wxCommandEvent& event);
    void OnTimerTick(wxTimerEvent& event);
    void OnAutoRefreshToggle(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    // ── 核心数据流 ────────────────────────────────────────
    /** 执行完整的数据更新周期: 获取 → 计算 → 刷新 UI */
    void UpdateAllData();

    // ── UI 组件 ────────────────────────────────────────────
    wxSplitterWindow* m_splitter;

    // 控制栏控件
    wxStaticText* m_lblSpySpot;
    wxStaticText* m_lblESPrice;
    wxStaticText* m_lblBasis;
    wxStaticText* m_lblZeroGamma;
    wxStaticText* m_lblCallWall;
    wxStaticText* m_lblPutWall;
    wxStaticText* m_lblStatus;
    wxButton* m_btnRefresh;
    wxButton* m_btnAutoRefresh;

    // 图表面板
    GEXBarChartPanel* m_gexPanel;
    ESOverlayPanel*  m_esPanel;

    // 定时器 (自动刷新)
    wxTimer* m_timer;
    bool m_autoRefresh = false;

    // ── 当前数据 ──────────────────────────────────────────
    std::vector<OptionData> m_optionChain;
    double m_spySpot  = 575.0;
    double m_esPrice  = 5750.0;
    KeyLevels m_keyLevels;

    wxDECLARE_EVENT_TABLE();
};
