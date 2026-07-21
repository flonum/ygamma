# GEX-ES 量化图表分析系统

基于 C++17 / wxWidgets 的跨平台桌面应用，抓取 SPY 期权链数据，计算 Gamma 敞口 (GEX)，识别 Call Wall / Put Wall / Zero Gamma 关键位，并实时映射到 ES 期货价格图表上呈现。

## 功能

- **数据获取** — libcurl 从 Yahoo Finance 获取 SPY/ES 实时价格，网络失败自动回退 Mock 数据
- **GEX 计算引擎** — Black-Scholes Gamma、Net GEX 汇总、Zero Gamma 二分定位、SPY→ES 基差映射
- **左侧 GEX 柱状图** — 按行权价展示 Net GEX（绿柱 Call 占优 / 红柱 Put 占优），标注 Call Wall / Put Wall / Zero Gamma
- **右侧 ES 折线图** — ES 价格走势叠加三条横向支撑阻力虚线（Put Wall 强支撑 / Call Wall 强阻力 / Zero Gamma 多空分界）
- **控制栏** — 实时显示 SPY Spot、ES 价格、基差、关键位，支持自动刷新 (30s)
- **浏览器演示版** — `demo.html` + `server.py` 可在浏览器中直接运行，无需编译

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI | wxWidgets 3.2+ |
| 网络 | libcurl |
| JSON | nlohmann/json (单头文件) |
| 构建 | CMake 3.16+ |

## 编译

### 依赖

- **wxWidgets 3.2+** (core / base / aui)
- **libcurl** (含开发头文件)
- **nlohmann/json** (已内嵌 `src/json.hpp`)
- **CMake 3.16+**
- **C++17 编译器** (MSVC 2022 / GCC 11+ / Clang 14+)

### Windows (MinGW)

```batch
# 1. 编译 wxWidgets
cd wxWidgets-3.2.7.1\build\msw
mingw32-make -f makefile.gcc BUILD=release SHARED=0

# 2. 构建项目
cd ygamma
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ^
  -DwxWidgets_ROOT_DIR=C:\wxWidgets-3.2.7.1 ^
  -DwxWidgets_LIB_DIR=C:\wxWidgets-3.2.7.1\lib\gcc_lib
mingw32-make

# 3. 运行
ygamma.exe
```

### Windows (MSVC + vcpkg)

```batch
vcpkg install wxwidgets curl nlohmann-json

cmake -B build -S . ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

### 浏览器演示版

无需编译，直接运行：

```bash
python server.py
# 浏览器打开 http://localhost:8080/demo.html
```

## 项目架构

```
ygamma/
├── CMakeLists.txt              # CMake 构建配置
├── demo.html                   # 浏览器演示版 (完整功能)
├── server.py                   # 演示版本地服务器 + CORS 代理
└── src/
    ├── main.cpp                # wxApp 入口
    ├── GEXEngine.h/.cpp        # 算法引擎
    ├── OptionDataFetcher.h/.cpp # 数据获取层
    ├── GEXBarChartPanel.h/.cpp  # 左侧 GEX 柱状图
    ├── ESOverlayPanel.h/.cpp    # 右侧 ES 图表面板
    ├── MainFrame.h/.cpp         # 主窗口 + 控制栏
    └── json.hpp                 # nlohmann/json 单头文件
```

### 三层架构

```
┌─────────────────────────────────────────────┐
│  UI 层                                      │
│  MainFrame → wxSplitterWindow               │
│    ├── GEXBarChartPanel (左 40%)             │
│    └── ESOverlayPanel  (右 60%)             │
├─────────────────────────────────────────────┤
│  算法引擎                                    │
│  GEXEngine                                  │
│    ├── CalcGamma()        BS Gamma          │
│    ├── CalcNetGEX()       按 Strike 汇总     │
│    ├── FindZeroGamma()    二分定位          │
│    ├── FindCallWall/PutWall()               │
│    └── MapSPYtoES()       SPY→ES 基差映射   │
├─────────────────────────────────────────────┤
│  数据层                                      │
│  OptionDataFetcher                          │
│    ├── FetchSpotPrices()   SPY/ES 现价      │
│    ├── FetchOptionChain()  期权链           │
│    └── GenerateMockChain() Mock 回退        │
└─────────────────────────────────────────────┘
```

### 数据流

```
FetchSpotPrices() / FetchOptionChain()
        │
        ▼
GEXEngine::CalcAllKeyLevels()
        │
        ├──▶ GEXBarChartPanel::SetChartData()   左侧柱状图
        ├──▶ ESOverlayPanel::SetChartData()      右侧折线图 + Overlay
        └──▶ MainFrame 控制栏标签更新
```

## 核心公式

**Black-Scholes Gamma**

$$\Gamma = \frac{e^{-qT} \cdot N'(d_1)}{S \cdot \sigma \cdot \sqrt{T}}$$

**GEX 计算**

$$\text{Call GEX} = \Gamma \times \text{CallOI} \times 100 \times S$$
$$\text{Put GEX} = -\Gamma \times \text{PutOI} \times 100 \times S$$
$$\text{Net GEX} = \text{Call GEX} + \text{Put GEX}$$

**SPY → ES 基差映射**

$$\text{Basis} = P_{ES} - (P_{SPY} \times 10)$$
$$\text{ES Level} = (\text{SPY Level} \times 10) + \text{Basis}$$

## License

MIT
