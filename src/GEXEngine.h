#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <map>

/**
 * OptionData - 单个行权价对应的完整期权数据
 * 用于存储从数据源获取或 Mock 生成的期权链信息
 */
struct OptionData
{
    double strike;      // 行权价
    double callOI;      // Call 持仓量 (Open Interest)
    double putOI;       // Put 持仓量
    double callIV;      // Call 隐含波动率 (Implied Volatility)
    double putIV;       // Put 隐含波动率
    double expiration;  // 距到期年数 T = days / 365
};

/**
 * GEXResult - 单个 Strike 的 GEX 汇总结果
 */
struct GEXResult
{
    double strike;
    double callGEX;     // Call 贡献的 GEX
    double putGEX;      // Put 贡献的 GEX (负值)
    double netGEX;      // 净 GEX = callGEX + putGEX
};

/**
 * KeyLevels - 关键价位集合
 * 包含 Call Wall, Put Wall, Zero Gamma 三个核心参考位
 */
struct KeyLevels
{
    double callWall_SPY   = 0.0;   // SPY 标尺下的 Call Wall
    double putWall_SPY    = 0.0;   // SPY 标尺下的 Put Wall
    double zeroGamma_SPY  = 0.0;   // SPY 标尺下的 Zero Gamma
    double callWall_ES    = 0.0;   // 映射到 ES 的 Call Wall
    double putWall_ES     = 0.0;   // 映射到 ES 的 Put Wall
    double zeroGamma_ES   = 0.0;   // 映射到 ES 的 Zero Gamma
    double basis          = 0.0;   // 当前基差
};

/**
 * GEXEngine - Gamma 敞口计算引擎 (纯静态方法)
 *
 * 核心功能:
 *   1. Black-Scholes Gamma 计算
 *   2. 按 Strike 汇总 Net GEX
 *   3. 识别 Call Wall / Put Wall / Zero Gamma
 *   4. SPY → ES 基差映射
 */
class GEXEngine
{
public:
    // ── 模型参数 (可配置) ─────────────────────────────────
    static constexpr double RISK_FREE_RATE = 0.05;   // 无风险利率 r
    static constexpr double DIVIDEND_YIELD = 0.01;   // 股息率 q
    static constexpr double SCAN_STEP      = 0.5;    // Zero Gamma 扫描步长

    /**
     * CalcGamma - Black-Scholes 模型期权 Gamma 计算
     * @param S     标的现价 (Spot Price)
     * @param K     行权价 (Strike)
     * @param T     距到期时间 (年)
     * @param r     无风险利率
     * @param sigma 隐含波动率
     * @param q     股息率
     * @return      Gamma 值
     *
     * 公式: Γ = e^(-qT) * N'(d1) / (S * σ * √T)
     */
    static double CalcGamma(double S, double K, double T,
                            double r, double sigma, double q);

    /**
     * CalcSingleStrikeGEX - 计算单个 Strike 的 Call/Put/Net GEX
     * @param S     标的现价
     * @param data  单个期权行权价数据
     * @return      GEXResult 包含 callGEX, putGEX, netGEX
     */
    static GEXResult CalcSingleStrikeGEX(double S, const OptionData& data);

    /**
     * CalcNetGEX - 对整个期权链按 Strike 汇总 Net GEX
     * @param chain 期权链数据
     * @param S     标的现价
     * @return      按 Strike 排序的 GEX 结果列表
     */
    static std::vector<GEXResult> CalcNetGEX(
        const std::vector<OptionData>& chain, double S);

    /**
     * FindCallWall - 找出 Call GEX 最大的 Strike (Call Wall)
     */
    static double FindCallWall(const std::vector<GEXResult>& results);

    /**
     * FindPutWall - 找出 Put GEX 绝对值最大的 Strike (Put Wall)
     * 注: Put GEX 为负值，取绝对值最大的
     */
    static double FindPutWall(const std::vector<GEXResult>& results);

    /**
     * FindZeroGamma - 二分法寻找 Net GEX = 0 对应的标的价格
     * @param chain 期权链数据
     * @param S_min 扫描区间下限 (SPY 现价附近)
     * @param S_max 扫描区间上限
     * @return      Zero Gamma 对应的 SPY 价格
     */
    static double FindZeroGamma(const std::vector<OptionData>& chain,
                                double S_min, double S_max);

    /**
     * MapSPYtoES - 将 SPY 价格映射为 ES 期货价格
     * @param spyLevel  待转换的 SPY 价格 (Strike 或 Spot)
     * @param spySpot   当前 SPY 现价
     * @param esPrice   当前 ES 期货价格
     * @return          映射后的 ES 价格
     *
     * 公式: Basis = P_ES - (P_SPY × 10)
     *       ES Level = (SPY Level × 10) + Basis
     */
    static double MapSPYtoES(double spyLevel, double spySpot, double esPrice);

    /**
     * CalcAllKeyLevels - 一站式计算所有关键位
     * @param chain    期权链数据
     * @param spySpot  SPY 现价
     * @param esPrice  ES 期货价格
     * @return         KeyLevels 含 SPY 和 ES 两套坐标的关键位
     */
    static KeyLevels CalcAllKeyLevels(const std::vector<OptionData>& chain,
                                      double spySpot, double esPrice);

private:
    /** 标准正态分布概率密度函数 N'(x) */
    static double NormalPDF(double x);

    /** 标准正态分布累积函数 N(x) (用于验证，通过近似公式实现) */
    static double NormalCDF(double x);

    /** 计算 d1 (Black-Scholes 参数) */
    static double CalcD1(double S, double K, double T,
                         double r, double sigma, double q);

    /** 扫描法: 在给定 S 下的总 Net GEX */
    static double SumNetGEXAtS(const std::vector<OptionData>& chain, double S);
};
