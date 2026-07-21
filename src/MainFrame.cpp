#include "MainFrame.h"
#include <wx/msgdlg.h>
#include <sstream>
#include <iomanip>

// 事件表: 关联控件 ID 和事件处理函数
enum
{
    ID_REFRESH = 1001,
    ID_AUTO_REFRESH,
    ID_TIMER
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_REFRESH,       MainFrame::OnRefresh)
    EVT_BUTTON(ID_AUTO_REFRESH,  MainFrame::OnAutoRefreshToggle)
    EVT_TIMER(ID_TIMER,          MainFrame::OnTimerTick)
    EVT_MENU(wxID_EXIT,          MainFrame::OnQuit)
    EVT_MENU(wxID_ABOUT,         MainFrame::OnAbout)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title,
              wxDefaultPosition, wxSize(1280, 800))
{
    SetMinSize(wxSize(900, 600));

    // ── 菜单栏 ────────────────────────────────────────────
    CreateMenuBar();

    // ── 主布局: 垂直 Sizer ────────────────────────────────
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 控制栏
    wxPanel* controlPanel = new wxPanel(this, wxID_ANY);
    CreateControlBar(controlPanel);
    mainSizer->Add(controlPanel, 0, wxEXPAND | wxALL, 4);

    // Splitter 分栏
    CreateSplitterPanels();
    mainSizer->Add(m_splitter, 1, wxEXPAND | wxALL, 4);

    // 底部状态栏
    m_lblStatus = new wxStaticText(this, wxID_ANY, "就绪");
    mainSizer->Add(m_lblStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(mainSizer);

    // ── 定时器 ────────────────────────────────────────────
    m_timer = new wxTimer(this, ID_TIMER);

    // ── 启动时自动加载数据 ────────────────────────────────
    CallAfter(&MainFrame::UpdateAllData);
}

// ═══════════════════════════════════════════════════════════════
//  UI 创建
// ═══════════════════════════════════════════════════════════════

void MainFrame::CreateMenuBar()
{
    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT, "退出\tAlt+F4");

    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "关于...");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "文件");
    menuBar->Append(helpMenu, "帮助");
    SetMenuBar(menuBar);
}

/**
 * CreateControlBar - 创建顶部控制栏
 * 包含: 数据汇总标签 和 刷新/自动刷新按钮
 */
void MainFrame::CreateControlBar(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    // 字体设置
    wxFont titleFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    wxFont valueFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

    // 辅助: 创建 "标题: 数值" 组合
    auto addInfo = [&](const wxString& title, wxStaticText*& valueLabel) {
        wxStaticText* t = new wxStaticText(parent, wxID_ANY, title);
        t->SetFont(titleFont);
        sizer->Add(t, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

        valueLabel = new wxStaticText(parent, wxID_ANY, "--");
        valueLabel->SetFont(valueFont);
        valueLabel->SetForegroundColour(wxColour(0, 0, 128));
        sizer->Add(valueLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    };

    addInfo("SPY Spot:", m_lblSpySpot);
    addInfo("ES 价格:", m_lblESPrice);
    addInfo("基差:",    m_lblBasis);

    sizer->AddSpacer(16);

    addInfo("Call Wall:",  m_lblCallWall);
    addInfo("Put Wall:",   m_lblPutWall);
    addInfo("Zero Gamma:", m_lblZeroGamma);

    sizer->AddStretchSpacer(1);

    // 按钮
    m_btnRefresh = new wxButton(parent, ID_REFRESH, "刷新数据");
    sizer->Add(m_btnRefresh, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    m_btnAutoRefresh = new wxButton(parent, ID_AUTO_REFRESH, "自动刷新");
    sizer->Add(m_btnAutoRefresh, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    parent->SetSizer(sizer);
}

/**
 * CreateSplitterPanels - 创建左右分栏面板
 * 左侧: GEXBarChartPanel (GEX 柱状图)
 * 右侧: ESOverlayPanel (ES 价格图 + 支撑阻力线)
 */
void MainFrame::CreateSplitterPanels()
{
    m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_3D | wxSP_LIVE_UPDATE);

    m_gexPanel = new GEXBarChartPanel(m_splitter);
    m_esPanel  = new ESOverlayPanel(m_splitter);

    // 左 40% : 右 60% (黄金比例近似)
    m_splitter->SplitVertically(m_gexPanel, m_esPanel, 400);
    m_splitter->SetMinimumPaneSize(250);
}

// ═══════════════════════════════════════════════════════════════
//  核心数据流
// ═══════════════════════════════════════════════════════════════

/**
 * UpdateAllData - 执行完整的数据更新周期
 *
 * 流程:
 *   1. OptionDataFetcher 获取 SPY/ES 现价和期权链
 *   2. GEXEngine 计算 Net GEX 和关键位
 *   3. 更新左右面板图表 和 控制栏标签
 */
void MainFrame::UpdateAllData()
{
    m_lblStatus->SetLabel("正在获取数据...");

    // ── Step 1: 获取数据 ──────────────────────────────────
    // 获取现价
    SpotPriceResult spot = OptionDataFetcher::FetchSpotPrices();
    m_spySpot = spot.spySpot;
    m_esPrice = spot.esPrice;

    // 获取期权链 (网络失败自动回退到 Mock)
    m_optionChain = OptionDataFetcher::FetchOptionChain("SPY", false);

    if (m_optionChain.empty())
    {
        m_lblStatus->SetLabel("错误: 无法获取期权链数据");
        return;
    }

    // ── Step 2: 计算引擎 ──────────────────────────────────
    m_keyLevels = GEXEngine::CalcAllKeyLevels(m_optionChain,
                                              m_spySpot, m_esPrice);

    auto gexResults = GEXEngine::CalcNetGEX(m_optionChain, m_spySpot);

    // ── Step 3: 更新 UI ───────────────────────────────────
    // 控制栏标签
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << m_spySpot;
        m_lblSpySpot->SetLabel(oss.str());
    }
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << m_esPrice;
        m_lblESPrice->SetLabel(oss.str());
    }
    {
        std::ostringstream oss;
        double basis = m_esPrice - m_spySpot * 10.0;
        oss << std::fixed << std::setprecision(2) << basis;
        m_lblBasis->SetLabel(oss.str());
    }
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << m_keyLevels.callWall_ES;
        m_lblCallWall->SetLabel(oss.str());
    }
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << m_keyLevels.putWall_ES;
        m_lblPutWall->SetLabel(oss.str());
    }
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << m_keyLevels.zeroGamma_ES;
        m_lblZeroGamma->SetLabel(oss.str());
    }

    // 图表面板
    m_gexPanel->SetChartData(gexResults, m_spySpot, m_keyLevels);
    m_esPanel->SetChartData(m_esPrice, m_keyLevels);

    // 状态栏
    m_lblStatus->SetLabel("数据更新完成 - " + wxDateTime::Now().Format("%H:%M:%S"));
}

// ═══════════════════════════════════════════════════════════════
//  事件处理
// ═══════════════════════════════════════════════════════════════

void MainFrame::OnRefresh(wxCommandEvent& /*event*/)
{
    UpdateAllData();
}

void MainFrame::OnTimerTick(wxTimerEvent& /*event*/)
{
    if (m_autoRefresh)
    {
        UpdateAllData();
    }
}

/**
 * 切换自动刷新模式 (每 30 秒)
 */
void MainFrame::OnAutoRefreshToggle(wxCommandEvent& /*event*/)
{
    m_autoRefresh = !m_autoRefresh;
    if (m_autoRefresh)
    {
        m_timer->Start(30000);  // 30 秒间隔
        m_btnAutoRefresh->SetLabel("停止刷新");
        m_btnAutoRefresh->SetBackgroundColour(wxColour(128, 128, 128));
        m_lblStatus->SetLabel("自动刷新已开启 (30s)");
    }
    else
    {
        m_timer->Stop();
        m_btnAutoRefresh->SetLabel("自动刷新");
        m_btnAutoRefresh->SetBackgroundColour(wxNullColour);
        m_lblStatus->SetLabel("自动刷新已停止");
    }
    m_btnAutoRefresh->Refresh();
}

void MainFrame::OnQuit(wxCommandEvent& /*event*/)
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& /*event*/)
{
    wxMessageBox(
        "GEX-ES 量化图表分析系统 v1.0\n\n"
        "核心功能:\n"
        "  - 抓取 SPY 期权链数据\n"
        "  - Black-Scholes Gamma 敞口 (GEX) 计算\n"
        "  - 识别 Call Wall / Put Wall / Zero Gamma 关键位\n"
        "  - SPY → ES 期货基差映射与图表叠加\n\n"
        "技术栈: C++17 / wxWidgets / libcurl / nlohmann/json",
        "关于 GEX-ES 量化图表分析系统",
        wxOK | wxICON_INFORMATION);
}
