export module cnetnexus.chart.domain.stats;

import std;

export namespace cnetnexus::chart {

/** @brief 单个术数的计数（存档分布与热门术数共用）。 */
struct algorithm_count
{
    std::string algorithm; ///< 术数标识（规范值）
    std::uint64_t count{}; ///< 计数
};

/** @brief 概览统计（契约 §5.4；字段名与前端 types.ts 的 StatsOverview 对齐）。 */
struct stats_overview
{
    std::uint64_t users{};                       ///< 用户总数
    std::uint64_t cases{};                       ///< 案例总数
    std::vector<algorithm_count> archives;       ///< 各术数存档数
    std::uint64_t today_logins{};                ///< 今日登录次数
    std::uint64_t today_charts{};                ///< 今日新增存档数
    std::vector<algorithm_count> top_algorithms; ///< 按存档数排序的热门术数
};

} // namespace cnetnexus::chart
