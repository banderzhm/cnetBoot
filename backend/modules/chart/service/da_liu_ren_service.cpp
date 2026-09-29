module cnetnexus.chart.service.da_liu_ren_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

namespace {

constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;

/// @brief 请求范围校验。
/// 占类、地支、性别的取值合法性由上游映射（ZhMap / from_zh_zhi）在 infra
/// 边界判定， 本层只拒绝明显空缺的参数，不复制上游对照表。
auto valid(const cnetnexus::chart::da_liu_ren::cast_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month < 1 || input.month > 12)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    if (input.minute < 0 || input.minute > 59)
        return false;
    if (input.kind.empty())
        return false;
    if (input.ben_ming && input.ben_ming->empty())
        return false;
    if (input.xing_nian && input.xing_nian->empty())
        return false;
    return true;
}

} // namespace

namespace cnetnexus::chart {

auto da_liu_ren_service::cast(const cnetnexus::chart::da_liu_ren::cast_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::da_liu_ren::cast_result, cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    co_return co_await compute(*compute_,
        [this, input] { return algorithm_->cast(input); });
}

} // namespace cnetnexus::chart
