cmake_minimum_required(VERSION 3.16)
project(ygamma LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 自动处理 MOC / UIC 等 (wxWidgets 需要)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── wxWidgets (自动探测常见 MinGW 路径) ────────────
if(NOT DEFINED wxWidgets_ROOT_DIR)
    foreach(DIR "C:/wxWidgets" "C:/wxWidgets-3.2.7.1"
              "$ENV{USERPROFILE}/wxWidgets")
        if(EXISTS "${DIR}/include/wx/wx.h")
            set(wxWidgets_ROOT_DIR "${DIR}" CACHE PATH "wxWidgets root")
            set(wxWidgets_LIB_DIR "${DIR}/lib/gcc_lib" CACHE PATH "wxWidgets lib")
            break()
        endif()
    endforeach()
endif()
find_package(wxWidgets REQUIRED COMPONENTS core base aui)
include(${wxWidgets_USE_FILE})

# ── CURL (MinGW 手动安装路径提示) ─────────────────────
if(NOT DEFINED CURL_ROOT)
    file(GLOB CURL_DIRS "C:/Users/Administrator/AppData/Local/Microsoft/WinGet/Packages/cURL.*/curl-*")
    if(CURL_DIRS)
        list(GET CURL_DIRS 0 CURL_ROOT_TMP)
        set(CURL_ROOT "${CURL_ROOT_TMP}" CACHE PATH "cURL root")
        set(CURL_INCLUDE_DIR "${CURL_ROOT}/include" CACHE PATH "cURL include")
        set(CURL_LIBRARY "${CURL_ROOT}/lib/libcurl.dll.a" CACHE FILEPATH "cURL lib")
    endif()
endif()
find_package(CURL REQUIRED)

# ── nlohmann_json ─────────────────────────────────────
find_package(nlohmann_json REQUIRED)

# ── 源文件 ─────────────────────────────────────────────
file(GLOB SOURCES
    src/*.cpp
    src/*.h
)

# ── 可执行目标 ─────────────────────────────────────────
add_executable(ygamma WIN32 ${SOURCES})

target_include_directories(ygamma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ygamma PRIVATE
    ${wxWidgets_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# ── Windows 特定链接 ───────────────────────────────────
if(WIN32)
    target_link_libraries(ygamma PRIVATE winmm ws2_32)
endif()

# ── 编译器警告 ─────────────────────────────────────────
if(MSVC)
    target_compile_options(ygamma PRIVATE /W4)
else()
    target_compile_options(ygamma PRIVATE -Wall -Wextra)
endif()
