#include "OptionDataFetcher.h"
#include <curl/curl.h>
#include <random>
#include <chrono>
#include <iostream>
#include "json.hpp"  // nlohmann/json 单头文件

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════
//  libcurl 辅助函数
// ═══════════════════════════════════════════════════════════════

/**
 * libcurl 写回调: 将收到的数据追加到 std::string
 */
size_t OptionDataFetcher::WriteCallback(void* contents, size_t size,
                                        size_t nmemb, std::string* output)
{
    size_t totalSize = size * nmemb;
    if (output)
    {
        output->append(static_cast<char*>(contents), totalSize);
    }
    return totalSize;
}

/**
 * 通过 libcurl 执行 HTTPS GET 请求
 * @param url       请求地址
 * @param response  [出参] 服务器响应内容
 * @return          成功返回 true
 */
bool OptionDataFetcher::HttpGet(const std::string& url, std::string& response)
{
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);            // 10 秒超时
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);      // 跟随重定向
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);      // 跳过 SSL 证书校验
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && httpCode >= 200 && httpCode < 300);
}

/**
 * 解析 Yahoo Finance JSON 期权链 (预留实现)
 * 当用户配置真实 API 时可在此处完成 JSON → OptionData 的转换
 */
std::vector<OptionData> OptionDataFetcher::ParseJsonChain(const std::string& json)
{
    // 预留: 解析 Yahoo Finance 或其他 API 返回的 JSON 期权链
    // 示例格式 (Yahoo Finance v8):
    // {
    //   "optionChain": {
    //     "result": [{
    //       "options": [{
    //         "expirationDate": 1234567890,
    //         "calls": [{ "strike": 570.0, "openInterest": 12345, "impliedVolatility": 0.18 }],
    //         "puts":  [{ "strike": 570.0, "openInterest": 9876,  "impliedVolatility": 0.20 }]
    //       }]
    //     }]
    //   }
    // }
    return {};
}

// ═══════════════════════════════════════════════════════════════
//  公开接口
// ═══════════════════════════════════════════════════════════════

/**
 * 获取期权链数据
 * 策略: 优先尝试网络请求，失败则自动回退到 Mock 数据
 */
std::vector<OptionData> OptionDataFetcher::FetchOptionChain(
    const std::string& symbol, bool useMock)
{
    // 如果显式要求 Mock 数据，直接生成
    if (useMock)
    {
        SpotPriceResult spot = FetchSpotPrices();
        return GenerateMockChain(spot.spySpot);
    }

    // 尝试从 Yahoo Finance API 获取数据
    std::string url =
        "https://query1.finance.yahoo.com/v7/finance/options/" + symbol;
    std::string response;

    if (HttpGet(url, response) && !response.empty())
    {
        try
        {
            auto chain = ParseJsonChain(response);
            if (!chain.empty())
            {
                return chain;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[OptionDataFetcher] JSON 解析失败: "
                      << e.what() << std::endl;
        }
    }

    // 网络请求失败或解析失败 → 回退到 Mock 数据
    std::cerr << "[OptionDataFetcher] 网络请求失败, 使用 Mock 数据" << std::endl;
    SpotPriceResult spot = FetchSpotPrices();
    return GenerateMockChain(spot.spySpot);
}

/**
 * 获取 SPY 和 ES 现价
 * 默认返回预设的合理模拟价格
 */
SpotPriceResult OptionDataFetcher::FetchSpotPrices()
{
    SpotPriceResult result;

    // 尝试从网络获取实时价格
    std::string spyUrl =
        "https://query1.finance.yahoo.com/v8/finance/chart/SPY";
    std::string esUrl =
        "https://query1.finance.yahoo.com/v8/finance/chart/ES=F";
    std::string response;

    // 获取 SPY 现价
    if (HttpGet(spyUrl, response) && !response.empty())
    {
        try
        {
            json j = json::parse(response);
            double price = j["chart"]["result"][0]["meta"]["regularMarketPrice"];
            if (price > 0) result.spySpot = price;
        }
        catch (...) { /* 解析失败使用默认值 */ }
    }

    // 获取 ES 现价
    if (HttpGet(esUrl, response) && !response.empty())
    {
        try
        {
            json j = json::parse(response);
            double price = j["chart"]["result"][0]["meta"]["regularMarketPrice"];
            if (price > 0) result.esPrice = price;
        }
        catch (...) { /* 解析失败使用默认值 */ }
    }

    // 如果 ES 获取失败，基于 SPY × 10 做近似
    if (result.esPrice <= 0 || std::abs(result.esPrice - result.spySpot * 10) > 200)
    {
        // 添加约 10-20 点的基差 (ES 通常略高)
        result.esPrice = result.spySpot * 10.0 + 15.0;
    }

    return result;
}

/**
 * 生成模拟期权链数据
 *
 * 设计原则:
 *   - Call/Put OI 在平值附近大致均衡，确保 Zero Gamma 接近现价
 *   - Call Wall 略高于现价 (OTM Call 卖盘集中区)
 *   - Put Wall 略低于现价 (OTM Put 买盘保护区)
 *   - 距到期约 30 天 (T ≈ 0.082), IV ≈ 18%
 */
std::vector<OptionData> OptionDataFetcher::GenerateMockChain(double spot)
{
    // 使用固定种子保证每次生成的 Mock 数据一致，便于调试
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> noise(-0.05, 0.05);

    std::vector<OptionData> chain;

    // 行权价: spot - 50 到 spot + 50, 步长 5
    double strikeMin = std::floor((spot - 50.0) / 5.0) * 5.0;
    double strikeMax = std::ceil((spot + 50.0) / 5.0) * 5.0;
    int numStrikes = static_cast<int>((strikeMax - strikeMin) / 5.0) + 1;

    double timeToExpiry = 30.0 / 365.0;  // 约 30 天到期
    double baseIV = 0.18;                // 平值隐含波动率 18%

    for (int i = 0; i < numStrikes; ++i)
    {
        OptionData data;
        data.strike    = strikeMin + i * 5.0;
        data.expiration = timeToExpiry;

        // 距平值的距离 (标准化)
        double moneyness = (data.strike - spot) / spot;

        // ── 持仓量建模 ──────────────────────────────────────
        // 基础 OI: 平值最高 (约 120K), 远离平值递减 (高斯分布)
        double baseOI = 120000.0 * std::exp(-0.5 * std::pow(moneyness / 0.03, 2));

        // Call OI 偏斜: Call Wall 在 spot 上方 2-3% (OTM Call 卖盘集中)
        double callSkew = 1.0 + 0.4 * std::exp(-0.5 * std::pow((moneyness - 0.025) / 0.015, 2));
        // Put OI 偏斜: Put Wall 在 spot 下方 2-3% (OTM Put 买盘保护)
        double putSkew = 1.0 + 0.4 * std::exp(-0.5 * std::pow((moneyness + 0.025) / 0.015, 2));

        // Call/Put 使用相同的 OI 乘数范围，确保整体均衡
        double oiMultiplier = 0.82 + 0.10 * (0.5 + 0.5 * noise(rng));
        data.callOI = baseOI * callSkew * oiMultiplier;
        data.putOI  = baseOI * putSkew  * oiMultiplier;

        // ── 隐含波动率建模 (波动率微笑) ─────────────────────
        double ivSkew = 1.0 + 0.25 * moneyness * moneyness;
        data.callIV = baseIV * ivSkew * (0.95 + 0.05 * noise(rng));
        data.putIV  = baseIV * ivSkew * (1.0  + 0.05 * noise(rng));

        chain.push_back(data);
    }

    return chain;
}
