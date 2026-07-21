#pragma once

#include "GEXEngine.h"
#include <string>

/**
 * SpotPriceResult - 现价查询结果
 */
struct SpotPriceResult
{
    double spySpot = 575.0;   // SPY 现价
    double esPrice = 5750.0;  // ES 期货现价 (SPY × 10 基准)
};

/**
 * OptionDataFetcher - 期权数据获取层
 *
 * 负责:
 *   1. 通过 libcurl 从公开 API 获取期权链
 *   2. 网络请求失败时自动回退到 Mock 数据
 *   3. 获取 SPY 和 ES 的当前价格
 */
class OptionDataFetcher
{
public:
    /**
     * FetchOptionChain - 获取指定标的的期权链数据
     * @param symbol    标的代码 (默认 "SPY")
     * @param useMock   是否强制使用 Mock 数据 (默认 false, 网络失败自动回退)
     * @return          期权链数据列表
     */
    static std::vector<OptionData> FetchOptionChain(
        const std::string& symbol = "SPY", bool useMock = false);

    /**
     * FetchSpotPrices - 获取 SPY 和 ES 的当前现价
     * @return SpotPriceResult 包含 spySpot 和 esPrice
     */
    static SpotPriceResult FetchSpotPrices();

    /**
     * GenerateMockChain - 生成模拟期权链数据
     * 生成范围覆盖 SPY 现价 ± 50 点之间的 Strike，共约 20 个行权价
     * @param spot 标的现价 (用于定位 Mock 数据的中心)
     * @return     模拟期权链
     */
    static std::vector<OptionData> GenerateMockChain(double spot = 575.0);

private:
    /** libcurl 写回调函数，将响应写入 string */
    static size_t WriteCallback(void* contents, size_t size,
                                size_t nmemb, std::string* output);

    /** 通过 libcurl 执行 HTTPS GET 请求 */
    static bool HttpGet(const std::string& url, std::string& response);

    /** 从 JSON 响应解析期权链 (预留) */
    static std::vector<OptionData> ParseJsonChain(const std::string& json);
};
