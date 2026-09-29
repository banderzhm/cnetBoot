module cnetnexus.chart.service.stats_service;

import std;
import cnetmod.core.time;
import cnetmod.coro;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.chart.mapper.user_count_mapper;
import cnetnexus.framework.error;

namespace {

/// 热门术数榜单长度：前端只展示前几名，避免无界输出。
constexpr std::size_t top_algorithm_limit = 5;

constexpr std::int64_t seconds_per_day = 24 * 60 * 60;

} // namespace

namespace cnetnexus::chart {

auto stats_service::overview()
    -> cnetmod::task<std::expected<cnetnexus::chart::stats_overview,
        cnetnexus::framework::error::error_code>>
{
    cnetnexus::chart::stats_overview result;
    const auto user_count = co_await users_->count();
    if (!user_count)
        co_return std::unexpected{user_count.error()};
    result.users = *user_count;

    const auto case_count = co_await cases_->count();
    if (!case_count)
        co_return std::unexpected{case_count.error()};
    result.cases = *case_count;

    const auto archives = co_await archives_->counts_by_algorithm();
    if (!archives)
        co_return std::unexpected{archives.error()};
    result.archives = *archives;

    const auto today_start =
        cnetnexus::chart::utc_day_start(cnetmod::unix_time_seconds());
    const auto today_end = today_start + seconds_per_day - 1;
    const auto charts = co_await archives_->count_since(today_start, today_end);
    if (!charts)
        co_return std::unexpected{charts.error()};
    result.today_charts = *charts;

    // today.logins 不在这里组装：登录审计行属于 identity 模块，chart 不 import 别的
    // 模块的存储契约；该计数由 transport 层合并进响应。
    result.today_logins = 0;

    // 热门术数：按存档数降序，同数按算法名升序，最多 5 条。
    result.top_algorithms = result.archives;
    std::ranges::sort(result.top_algorithms,
        [](const auto& left, const auto& right)
        {
            if (left.count != right.count)
                return left.count > right.count;
            return left.algorithm < right.algorithm;
        });
    if (result.top_algorithms.size() > top_algorithm_limit)
        result.top_algorithms.resize(top_algorithm_limit);
    co_return result;
}

} // namespace cnetnexus::chart
