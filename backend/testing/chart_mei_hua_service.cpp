import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
class fake_mei_hua_algorithm final : public cnetnexus::chart::mei_hua_algorithm
{
public:
    int calls{0};
    cnetnexus::chart::mei_hua::cast_input last{};
    std::optional<error_code> failure{};

    auto cast(const cnetnexus::chart::mei_hua::cast_input& input)
        -> std::expected<cnetnexus::chart::mei_hua::cast_result, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::mei_hua::cast_result result{};
        result.input = input;
        result.analysis_zh = "占断结果";
        return result;
    }
};

/// @brief 合法输入；具体字段由各用例覆盖。
auto base_input() -> cnetnexus::chart::mei_hua::cast_input
{
    return cnetnexus::chart::mei_hua::cast_input{.year = 2000,
        .month = 6,
        .day = 15,
        .hour = 16,
        .minute = 30,
        .kind = "求财经营",
        .question = "所谋合作能否得财"};
}

} // namespace

/// @brief service 用例：范围校验必须先于算法调用，失败必须原样传播。
auto main() -> int
{
    fake_mei_hua_algorithm algorithm;
    const cnetnexus::chart::mei_hua_service service{algorithm, cnetnexus::chart::inline_compute()};

    const auto ok = cnetmod::sync_wait(service.cast(base_input()));
    if (!ok || algorithm.calls != 1)
        return 1;
    if (ok->analysis_zh != "占断结果" || ok->input.kind != "求财经营")
        return 2;

    const int before = algorithm.calls;
    auto bad_hour = base_input();
    bad_hour.hour = 24;
    if (cnetmod::sync_wait(service.cast(bad_hour)).error() != error_code::request_invalid_argument)
        return 3;
    auto bad_month = base_input();
    bad_month.month = 13;
    if (cnetmod::sync_wait(service.cast(bad_month)).error() != error_code::request_invalid_argument)
        return 4;
    auto bad_year = base_input();
    bad_year.year = 1800;
    if (cnetmod::sync_wait(service.cast(bad_year)).error() != error_code::request_invalid_argument)
        return 5;
    auto bad_day = base_input();
    bad_day.day = 0;
    if (cnetmod::sync_wait(service.cast(bad_day)).error() != error_code::request_invalid_argument)
        return 6;
    if (algorithm.calls != before)
        return 7;

    // 农历允许负数月份（闰月），公历不允许。
    auto lunar = base_input();
    lunar.calendar = cnetnexus::chart::mei_hua::calendar_kind::lunar;
    lunar.month = -6;
    if (!cnetmod::sync_wait(service.cast(lunar)))
        return 8;
    auto solar_negative = base_input();
    solar_negative.month = -6;
    if (cnetmod::sync_wait(service.cast(solar_negative)).error() != error_code::request_invalid_argument)
        return 9;

    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.cast(base_input())).error() != error_code::algorithm_failed)
        return 10;
    return 0;
}