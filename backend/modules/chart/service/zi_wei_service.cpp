module cnetnexus.chart.service.zi_wei_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

namespace {

constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;
constexpr int k_min_age = 1;
constexpr int k_max_age = 150;
constexpr int k_min_lunar_month = 1;
constexpr int k_max_lunar_month = 12;
constexpr int k_min_lunar_day = 1;
constexpr int k_max_lunar_day = 30;

/// @brief 请求范围校验；只在 service 层判定，不把用户输入直接透给上游。
auto valid(const cnetnexus::chart::zi_wei::chart_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month == 0 || input.month < -12 || input.month > 12)
        return false;
    if (input.calendar == cnetnexus::chart::zi_wei::calendar_kind::solar && input.month < 0)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    // 运限参数可选，但一旦提供就必须落在合法范围，越界一律 REQUEST.INVALID_ARGUMENT，
    // 不静默忽略、不悄悄退回“不算运限”。
    if (input.target_year && (*input.target_year < k_min_year || *input.target_year > k_max_year))
        return false;
    if (input.current_age && (*input.current_age < k_min_age || *input.current_age > k_max_age))
        return false;

    // 目标农历月/日/时是成套参数（见 domain 契约）：三项齐备、且与 target_year 同时给出才
    // 生效；只给其中一部分、或没有 target_year 时一律 400，不静默忽略、不替调用方补默认值。
    // 月/日的取值范围在这里判；时辰地支汉字→地支的映射在 infra 边界取上游
    // ZhouYi::GanZhi::Mapper::from_zh_zhi（本仓库不维护第二份对照表），非地支汉字同样 400。
    const bool has_target_month = input.target_lunar_month.has_value();
    const bool has_target_day = input.target_lunar_day.has_value();
    const bool has_target_hour = input.target_hour.has_value();
    if (has_target_month || has_target_day || has_target_hour)
    {
        if (!input.target_year || !has_target_month || !has_target_day || !has_target_hour)
            return false;
        if (*input.target_lunar_month < k_min_lunar_month || *input.target_lunar_month > k_max_lunar_month)
            return false;
        if (*input.target_lunar_day < k_min_lunar_day || *input.target_lunar_day > k_max_lunar_day)
            return false;
    }
    return true;
}

} // namespace

namespace cnetnexus::chart {

auto zi_wei_service::pai_pan(const cnetnexus::chart::zi_wei::chart_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::zi_wei::chart_result, cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    co_return co_await compute(*compute_,
        [this, input] { return algorithm_->pai_pan(input); });
}

} // namespace cnetnexus::chart
