module cnetnexus.chart.service.qi_men_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

namespace {

constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;

/// @brief 请求范围校验；占类与年命的取值由上游反射/映射在 infra 边界判定。
auto valid(const cnetnexus::chart::qi_men::chart_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month == 0 || input.month < -12 || input.month > 12)
        return false;
    if (input.calendar == cnetnexus::chart::qi_men::calendar_kind::solar && input.month < 0)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    if (input.minute < 0 || input.minute > 59)
        return false;
    if (input.kind.empty())
        return false;
    if (input.nian_ming && input.nian_ming->empty())
        return false;
    return true;
}

} // namespace

namespace cnetnexus::chart {

auto qi_men_service::pai_pan(const cnetnexus::chart::qi_men::chart_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::qi_men::chart_result, cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    co_return co_await compute(*compute_,
        [this, input] { return algorithm_->pai_pan(input); });
}

} // namespace cnetnexus::chart
