# GEX-ES — Options Gamma Exposure & Futures Chart Mapping

A cross-platform desktop application built with C++17 and wxWidgets. Fetches SPY options chain data, computes Gamma Exposure (GEX), identifies Call Wall / Put Wall / Zero Gamma key levels, and maps them in real-time onto E-mini S&P 500 (ES) futures price charts.

## Features

- **Data Fetching** — Retrieves SPY/ES real-time prices from Yahoo Finance via libcurl; automatically falls back to mock data on network failure
- **GEX Engine** — Black-Scholes Gamma, Net GEX aggregation, Zero Gamma binary search, SPY → ES basis conversion
- **GEX Bar Chart (Left Panel)** — Net GEX bars by strike (green = Call dominant, red = Put dominant), with Call Wall / Put Wall / Zero Gamma markers
- **ES Price Chart (Right Panel)** — ES price line with three horizontal support/resistance overlays (Put Wall / Call Wall / Zero Gamma)
- **Control Bar** — Live display of SPY Spot, ES price, basis, and key levels; auto-refresh (30s)
- **Browser Demo** — `demo.html` + `server.py` runs directly in the browser, no compilation required

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C++17 |
| GUI | wxWidgets 3.2+ |
| HTTP | libcurl |
| JSON | nlohmann/json (single header) |
| Build | CMake 3.16+ |

## Build

### Prerequisites

- **wxWidgets 3.2+** (core / base / aui)
- **libcurl** (with development headers)
- **nlohmann/json** (bundled as `src/json.hpp`)
- **CMake 3.16+**
- **C++17 compiler** (MSVC 2022 / GCC 11+ / Clang 14+)

### Windows (MinGW)

```batch
# 1. Build wxWidgets
cd wxWidgets-3.2.7.1\build\msw
mingw32-make -f makefile.gcc BUILD=release SHARED=0

# 2. Build the project
cd ygamma
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ^
  -DwxWidgets_ROOT_DIR=C:\wxWidgets-3.2.7.1 ^
  -DwxWidgets_LIB_DIR=C:\wxWidgets-3.2.7.1\lib\gcc_lib
mingw32-make

# 3. Run
ygamma.exe
```

### Windows (MSVC + vcpkg)

```batch
vcpkg install wxwidgets curl nlohmann-json

cmake -B build -S . ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

### Browser Demo

No compilation needed:

```bash
python server.py
# Open http://localhost:8080/demo.html in your browser
```

## Architecture

```
ygamma/
├── CMakeLists.txt              # CMake build configuration
├── demo.html                   # Browser demo (full functionality)
├── server.py                   # Local dev server + CORS proxy
└── src/
    ├── main.cpp                # wxApp entry point
    ├── GEXEngine.h/.cpp        # Calculation engine
    ├── OptionDataFetcher.h/.cpp # Data fetching layer
    ├── GEXBarChartPanel.h/.cpp  # GEX bar chart (left panel)
    ├── ESOverlayPanel.h/.cpp    # ES chart + overlays (right panel)
    ├── MainFrame.h/.cpp         # Main window + control bar
    └── json.hpp                 # nlohmann/json single header
```

### Three-Layer Design

```
┌─────────────────────────────────────────┐
│  UI Layer                               │
│  MainFrame → wxSplitterWindow           │
│    ├── GEXBarChartPanel (40% left)      │
│    └── ESOverlayPanel  (60% right)      │
├─────────────────────────────────────────┤
│  Engine Layer                           │
│  GEXEngine                              │
│    ├── CalcGamma()        BS Gamma      │
│    ├── CalcNetGEX()       Net GEX       │
│    ├── FindZeroGamma()    Binary search │
│    ├── FindCallWall/PutWall()           │
│    └── MapSPYtoES()       Basis mapping │
├─────────────────────────────────────────┤
│  Data Layer                             │
│  OptionDataFetcher                      │
│    ├── FetchSpotPrices()   Live prices  │
│    ├── FetchOptionChain()  Options chain│
│    └── GenerateMockChain() Mock fallback│
└─────────────────────────────────────────┘
```

### Data Flow

```
FetchSpotPrices() / FetchOptionChain()
        │
        ▼
GEXEngine::CalcAllKeyLevels()
        │
        ├──▶ GEXBarChartPanel::SetChartData()   Left bar chart
        ├──▶ ESOverlayPanel::SetChartData()      Right chart + overlays
        └──▶ MainFrame control bar update
```

## Core Formulas

**Black-Scholes Gamma**

$$\Gamma = \frac{e^{-qT} \cdot N'(d_1)}{S \cdot \sigma \cdot \sqrt{T}}$$

**GEX Calculation**

$$\text{Call GEX} = \Gamma \times \text{CallOI} \times 100 \times S$$
$$\text{Put GEX} = -\Gamma \times \text{PutOI} \times 100 \times S$$
$$\text{Net GEX} = \text{Call GEX} + \text{Put GEX}$$

**SPY → ES Basis Mapping**

$$\text{Basis} = P_{ES} - (P_{SPY} \times 10)$$
$$\text{ES Level} = (\text{SPY Level} \times 10) + \text{Basis}$$

## License

MIT
