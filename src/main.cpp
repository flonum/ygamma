#include <wx/wx.h>
#include "MainFrame.h"

/**
 * GEXApp - wxWidgets 应用程序入口类
 * 负责初始化 GUI 框架并创建主窗口
 */
class GEXApp : public wxApp
{
public:
    virtual bool OnInit() override
    {
        // 初始化所有图像处理器
        wxInitAllImageHandlers();

        // 创建主窗口
        MainFrame* frame = new MainFrame("GEX-ES 量化图表分析系统");
        frame->SetSize(1280, 800);
        frame->Center();
        frame->Show(true);

        return true;
    }
};

wxIMPLEMENT_APP(GEXApp);
