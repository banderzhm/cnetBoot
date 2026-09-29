import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
class fake_zi_wei_algorithm final : public cnetnexus::chart::zi_wei_algorithm
{
public:
    int calls{0};
    cnetnexus::chart::zi_wei::chart_input last{};
    std::optional<error_code> failure{};

    auto pai_pan(const cnetnexus::chart::zi_wei::chart_input& input)
        -> std::expected<cnetnexus::chart::zi_wei::chart_result, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::zi_wei::chart_result result{};
        result.input = input;
        result.chart_projection = cnetmod::json::object(
            {{"palaces", cnetmod::json::array()}});
        return result;
    }
};

/// @brief 合法排盘输入；具体字段由各用例覆盖。
auto base_input() -> cnetnexus::chart::zi_wei::chart_input
{
    return cnetnexus::chart::zi_wei::chart_input{.year = 2000, .month = 6, .day = 15, .hour = 16, .is_male = true};
}

} // namespace

/// @brief service 用例：范围校验必须先于算法调用，失败必须原样传播。
auto main() -> int
{
    fake_zi_wei_algorithm algorithm;
    const cnetnexus::chart::zi_wei_service service{algorithm, cnetnexus::chart::inline_compute()};

    const auto ok = cnetmod::sync_wait(service.pai_pan(base_input()));
    if (!ok || algorithm.calls != 1)
        return 1;
    if (!ok->chart_projection.is_object() || ok->input.hour != 16)
        return 2;

    const int before = algorithm.calls;
    auto bad_hour = base_input();
    bad_hour.hour = 24;
    if (cnetmod::sync_wait(service.pai_pan(bad_hour)).error() != error_code::request_invalid_argument)
        return 3;
    auto bad_month = base_input();
    bad_month.month = 13;
    if (cnetmod::sync_wait(service.pai_pan(bad_month)).error() != error_code::request_invalid_argument)
        return 4;
    auto bad_year = base_input();
    bad_year.year = 1800;
    if (cnetmod::sync_wait(service.pai_pan(bad_year)).error() != error_code::request_invalid_argument)
        return 5;
    auto bad_day = base_input();
    bad_day.day = 32;
    if (cnetmod::sync_wait(service.pai_pan(bad_day)).error() != error_code::request_invalid_argument)
        return 6;
    if (algorithm.calls != before)
        return 7;

    // 农历允许负数月份（闰月），公历不允许。
    auto lunar = base_input();
    lunar.calendar = cnetnexus::chart::zi_wei::calendar_kind::lunar;
    lunar.month = -6;
    if (!cnetmod::sync_wait(service.pai_pan(lunar)))
        return 8;
    auto solar_negative = base_input();
    solar_negative.month = -6;
    if (cnetmod::sync_wait(service.pai_pan(solar_negative)).error() != error_code::request_invalid_argument)
        return 9;

    // 运限参数口径（读 zi_wei_service.cpp 的真实校验）：
    //  - 只带 target_year（不带月/日/时）是**合法**的：只算大限/流年，不算流月/流日/流时；
    //  - target_lunar_month/day/hour 是成套参数：出现任意一项就必须三项齐备且带 target_year。
    auto only_year = base_input();
    only_year.target_year = 2026;
    if (!cnetmod::sync_wait(service.pai_pan(only_year)))
        return 10;
    auto only_month = base_input();
    only_month.target_lunar_month = 3;
    if (cnetmod::sync_wait(service.pai_pan(only_month)).error() != error_code::request_invalid_argument)
        return 11;
    auto partial = base_input();
    partial.target_year = 2026;
    partial.target_lunar_month = 3;
    if (cnetmod::sync_wait(service.pai_pan(partial)).error() != error_code::request_invalid_argument)
        return 11;
    auto complete = base_input();
    complete.target_year = 2026;
    complete.current_age = 27;
    complete.target_lunar_month = 3;
    complete.target_lunar_day = 4;
    complete.target_hour = "子";
    if (!cnetmod::sync_wait(service.pai_pan(complete)))
        return 12;
    auto out_of_range_month = complete;
    out_of_range_month.target_lunar_month = 13;
    if (cnetmod::sync_wait(service.pai_pan(out_of_range_month)).error() != error_code::request_invalid_argument)
        return 13;

    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.pai_pan(base_input())).error() != error_code::algorithm_failed)
        return 14;
    return 0;
}
