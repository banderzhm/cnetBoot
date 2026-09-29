import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
class fake_qi_men_algorithm final : public cnetnexus::chart::qi_men_algorithm
{
public:
    int calls{0};
    cnetnexus::chart::qi_men::chart_input last{};
    std::optional<error_code> failure{};

    auto pai_pan(const cnetnexus::chart::qi_men::chart_input& input)
        -> std::expected<cnetnexus::chart::qi_men::chart_result, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::qi_men::chart_result result{};
        result.input = input;
        result.analysis_zh = "奇门报告";
        result.pan_projection = cnetmod::json::object(
            {{"palaces", cnetmod::json::array()}});
        return result;
    }
};

/// @brief 合法排盘输入；占类用上游枚举名。
auto base_input() -> cnetnexus::chart::qi_men::chart_input
{
    return cnetnexus::chart::qi_men::chart_input{.year = 2000,
        .month = 7,
        .day = 16,
        .hour = 16,
        .minute = 30,
        .kind = "GongMing",
        .question = "所谋事业门径如何"};
}

} // namespace

/// @brief service 用例：范围与空缺校验必须先于算法调用，失败必须原样传播。
auto main() -> int
{
    fake_qi_men_algorithm algorithm;
    const cnetnexus::chart::qi_men_service service{algorithm, cnetnexus::chart::inline_compute()};

    const auto ok = cnetmod::sync_wait(service.pai_pan(base_input()));
    if (!ok || algorithm.calls != 1)
        return 1;
    if (ok->analysis_zh != "奇门报告" || ok->input.kind != "GongMing")
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
    auto empty_kind = base_input();
    empty_kind.kind.clear();
    if (cnetmod::sync_wait(service.pai_pan(empty_kind)).error() != error_code::request_invalid_argument)
        return 6;
    if (algorithm.calls != before)
        return 7;

    // 农历允许负数月份（闰月），公历不允许。
    auto lunar = base_input();
    lunar.calendar = cnetnexus::chart::qi_men::calendar_kind::lunar;
    lunar.month = -6;
    if (!cnetmod::sync_wait(service.pai_pan(lunar)))
        return 8;
    auto solar_negative = base_input();
    solar_negative.month = -6;
    if (cnetmod::sync_wait(service.pai_pan(solar_negative)).error() != error_code::request_invalid_argument)
        return 9;

    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.pai_pan(base_input())).error() != error_code::algorithm_failed)
        return 10;
    return 0;
}
