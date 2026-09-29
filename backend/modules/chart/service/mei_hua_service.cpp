module cnetnexus.chart.service.mei_hua_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

namespace {

constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;

/// @brief 请求范围校验；只在 service 层判定，不把用户输入直接透给上游。
auto valid(const cnetnexus::chart::mei_hua::cast_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month == 0 || input.month < -12 || input.month > 12)
        return false;
    if (input.calendar == cnetnexus::chart::mei_hua::calendar_kind::solar && input.month < 0)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    if (input.minute < 0 || input.minute > 59)
        return false;
    // 占类取值由上游 ZhMap 在 infra 边界判定，这里只拒绝空缺。
    return !input.kind.empty();
}

} // namespace

namespace cnetnexus::chart {

auto mei_hua_service::cast(const cnetnexus::chart::mei_hua::cast_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::mei_hua::cast_result, cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    co_return co_await compute(*compute_,
        [this, input] { return algorithm_->cast(input); });
}

} // namespace cnetnexus::chart