#define _USE_MATH_DEFINES  // Windows: 启用 M_PI 等数学常量
#include "GEXEngine.h"
#include <stdexcept>

// ═══════════════════════════════════════════════════════════════
//  内部辅助函数
// ═══════════════════════════════════════════════════════════════

/**
 * 标准正态分布概率密度函数 (PDF)
 * N'(x) = (1 / sqrt(2π)) * e^(-x²/2)
 */
double GEXEngine::NormalPDF(double x)
{
    static const double INV_SQRT_2PI = 1.0 / std::sqrt(2.0 * M_PI);
    return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

/**
 * 标准正态分布累积分布函数 (CDF)
 * 采用 Abramowitz & Stegun 近似公式，精度约 1e-7
 */
double GEXEngine::NormalCDF(double x)
{
    const double a1 =  0.254829592;
    const double a2 = -0.284496736;
    const double a3 =  1.421413741;
    const double a4 = -1.453152027;
    const double a5 =  1.061405429;
    const double p  =  0.3275911;

    int sign = (x >= 0) ? 1 : -1;
    x = std::fabs(x) / std::sqrt(2.0);

    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1)
               * t * std::exp(-x * x);

    return 0.5 * (1.0 + sign * y);
}

/**
 * 计算 Black-Scholes 参数 d1
 * d1 = [ln(S/K) + (r - q + σ²/2) × T] / (σ × √T)
 */
double GEXEngine::CalcD1(double S, double K, double T,
                         double r, double sigma, double q)
{
    // 边界保护: 行权价不能为 0，波动率不能为 0 或过期时间不能为 0
    if (K <= 0.0 || sigma <= 0.0 || T <= 0.0)
        return 0.0;

    double sigmaSqrtT = sigma * std::sqrt(T);
    return (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / sigmaSqrtT;
}

// ═══════════════════════════════════════════════════════════════
//  公开接口实现
// ═══════════════════════════════════════════════════════════════

/**
 * Black-Scholes 期权 Gamma 计算
 *
 * 公式: Γ = e^(-qT) × N'(d1) / (S × σ × √T)
 *
 * Gamma 表示 Delta 对标的价格变动的敏感度。
 * Call 和 Put 的 Gamma 值相同（均为正值）。
 */
double GEXEngine::CalcGamma(double S, double K, double T,
                            double r, double sigma, double q)
{
    // 边界条件检查
    if (S <= 0.0 || K <= 0.0 || T <= 0.0 || sigma <= 0.0)
        return 0.0;

    double d1 = CalcD1(S, K, T, r, sigma, q);
    double sigmaSqrtT = sigma * std::sqrt(T);

    // Γ = e^(-qT) × N'(d1) / (S × σ × √T)
    double gamma = std::exp(-q * T) * NormalPDF(d1) / (S * sigmaSqrtT);
    return gamma;
}

/**
 * 计算单个 Strike 的 GEX
 *
 * Call GEX  = Gamma × CallOI × 100 × S   (正值)
 * Put GEX   = -Gamma × PutOI × 100 × S   (负值, Put 的 Gamma Exposure 为负)
 * Net GEX   = Call GEX + Put GEX
 */
GEXResult GEXEngine::CalcSingleStrikeGEX(double S, const OptionData& data)
{
    GEXResult result;
    result.strike = data.strike;

    // 使用均值波动率提高稳健性
    double avgIV = (data.callIV + data.putIV) * 0.5;

    double gamma = CalcGamma(S, data.strike, data.expiration,
                             RISK_FREE_RATE, avgIV, DIVIDEND_YIELD);

    // GEX = Gamma × OI × 100 (合约乘数) × S (标的现价)
    const double CONTRACT_MULTIPLIER = 100.0;
    result.callGEX =  gamma * data.callOI * CONTRACT_MULTIPLIER * S;
    result.putGEX  = -gamma * data.putOI  * CONTRACT_MULTIPLIER * S;
    result.netGEX  = result.callGEX + result.putGEX;

    return result;
}

/**
 * 对整个期权链计算 Net GEX 并排序
 * 按 Strike 升序排列，方便柱状图从左到右绘制
 */
std::vector<GEXResult> GEXEngine::CalcNetGEX(
    const std::vector<OptionData>& chain, double S)
{
    std::vector<GEXResult> results;
    results.reserve(chain.size());

    for (const auto& data : chain)
    {
        results.push_back(CalcSingleStrikeGEX(S, data));
    }

    // 按行权价升序排序
    std::sort(results.begin(), results.end(),
              [](const GEXResult& a, const GEXResult& b) {
                  return a.strike < b.strike;
              });

    return results;
}

/**
 * 找出 Call Wall (Call GEX 最大的 Strike)
 * Call Wall 是多头集中卖 Call 的位置，构成强阻力位
 */
double GEXEngine::FindCallWall(const std::vector<GEXResult>& results)
{
    if (results.empty()) return 0.0;

    double maxCallGEX = -1e18;
    double callWall   = results[0].strike;

    for (const auto& r : results)
    {
        if (r.callGEX > maxCallGEX)
        {
            maxCallGEX = r.callGEX;
            callWall   = r.strike;
        }
    }
    return callWall;
}

/**
 * 找出 Put Wall (Put GEX 绝对值最大的 Strike)
 * Put GEX 为负值 (Put Gamma × OI × 100 × S 取反号)，
 * 我们找 putGEX 最小 (最负) 的那个 Strike，其绝对值最大。
 * Put Wall 是多头集中买 Put 的位置，构成强支撑位。
 */
double GEXEngine::FindPutWall(const std::vector<GEXResult>& results)
{
    if (results.empty()) return 0.0;

    double minPutGEX = 1e18;   // 找最负的 putGEX
    double putWall   = results[0].strike;

    for (const auto& r : results)
    {
        if (r.putGEX < minPutGEX)
        {
            minPutGEX = r.putGEX;
            putWall   = r.strike;
        }
    }
    return putWall;
}

/**
 * 在给定标的价格 S 下计算全部期权链的 Net GEX 总和
 * 用于 Zero Gamma 的寻找
 */
double GEXEngine::SumNetGEXAtS(const std::vector<OptionData>& chain, double S)
{
    double total = 0.0;
    for (const auto& data : chain)
    {
        auto result = CalcSingleStrikeGEX(S, data);
        total += result.netGEX;
    }
    return total;
}

/**
 * 二分法寻找 Zero Gamma 水平
 *
 * Zero Gamma = 使 Net GEX 总和等于 0 的标的价格
 * 当价格高于 Zero Gamma 时，做市商对冲行为推动价格上涨（正反馈）
 * 当价格低于 Zero Gamma 时，做市商对冲行为推动价格下跌（负反馈）
 *
 * @param chain  期权链数据
 * @param S_min  扫描下限 (通常 SPY 现价 × 0.9)
 * @param S_max  扫描上限 (通常 SPY 现价 × 1.1)
 * @return        Net GEX = 0 对应的 SPY 价格
 */
double GEXEngine::FindZeroGamma(const std::vector<OptionData>& chain,
                                double S_min, double S_max)
{
    if (chain.empty()) return 0.0;

    double lo = S_min;
    double hi = S_max;

    double gexLo = SumNetGEXAtS(chain, lo);
    double gexHi = SumNetGEXAtS(chain, hi);

    // 如果两端同号，说明在此区间内无 Zero Gamma
    // 返回中点作为近似值
    if (gexLo * gexHi > 0.0)
    {
        return (lo + hi) * 0.5;
    }

    // 二分法迭代 (最多 50 次迭代)
    const int MAX_ITER = 50;
    const double TOLERANCE = 1e-6;

    for (int i = 0; i < MAX_ITER; ++i)
    {
        double mid = (lo + hi) * 0.5;
        double gexMid = SumNetGEXAtS(chain, mid);

        if (std::fabs(gexMid) < TOLERANCE || (hi - lo) < SCAN_STEP * 0.01)
        {
            return mid;
        }

        if (gexLo * gexMid < 0.0)
        {
            hi = mid;
            gexHi = gexMid;
        }
        else
        {
            lo = mid;
            gexLo = gexMid;
        }
    }

    return (lo + hi) * 0.5;
}

/**
 * SPY → ES 基差映射
 *
 * 由于 SPY ETF 和 ES 期货并非同一标的，它们之间存在基差:
 *   Basis = P_ES - (P_SPY × 10)
 *
 * 映射公式:
 *   ES Level = (SPY Level × 10) + Basis
 *
 * 这样可以将 SPY 期权链识别出的 Call Wall / Put Wall / Zero Gamma
 * 换算为 ES 期货图表上对应的支撑阻力位置。
 */
double GEXEngine::MapSPYtoES(double spyLevel, double spySpot, double esPrice)
{
    double basis = esPrice - spySpot * 10.0;
    return spyLevel * 10.0 + basis;
}

/**
 * 一站式计算所有关键位 (Call Wall / Put Wall / Zero Gamma)，
 * 并同时返回 SPY 和 ES 两套坐标。
 */
KeyLevels GEXEngine::CalcAllKeyLevels(const std::vector<OptionData>& chain,
                                      double spySpot, double esPrice)
{
    KeyLevels levels;

    // Step 1: 计算 Net GEX
    auto gexResults = CalcNetGEX(chain, spySpot);

    // Step 2: 识别 Call Wall 和 Put Wall
    levels.callWall_SPY = FindCallWall(gexResults);
    levels.putWall_SPY  = FindPutWall(gexResults);

    // Step 3: 二分法找 Zero Gamma
    levels.zeroGamma_SPY = FindZeroGamma(chain,
                                         spySpot * 0.90,
                                         spySpot * 1.10);

    // Step 4: SPY → ES 基差映射
    levels.basis = esPrice - spySpot * 10.0;

    levels.callWall_ES  = MapSPYtoES(levels.callWall_SPY,  spySpot, esPrice);
    levels.putWall_ES   = MapSPYtoES(levels.putWall_SPY,   spySpot, esPrice);
    levels.zeroGamma_ES = MapSPYtoES(levels.zeroGamma_SPY, spySpot, esPrice);

    return levels;
}
