import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
class fake_da_liu_ren_algorithm final : public cnetnexus::chart::da_liu_ren_algorithm
{
public:
    int calls{0};
    cnetnexus::chart::da_liu_ren::cast_input last{};
    std::optional<error_code> failure{};

    auto cast(const cnetnexus::chart::da_liu_ren::cast_input& input)
        -> std::expected<cnetnexus::chart::da_liu_ren::cast_result, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::da_liu_ren::cast_result result{};
        result.input = input;
        result.analysis_zh = "课体报告";
        result.pan_projection = cnetmod::json::object(
            {{"lessons", cnetmod::json::array()}});
        return result;
    }
};

/// @brief 合法输入；具体字段由各用例覆盖。
auto base_input() -> cnetnexus::chart::da_liu_ren::cast_input
{
    return cnetnexus::chart::da_liu_ren::cast_input{.year = 2024,
        .month = 1,
        .day = 1,
        .hour = 10,
        .minute = 0,
        .kind = "求财",
        .question = "所问交易何时有进展"};
}

} // namespace

/// @brief service 用例：范围校验必须先于算法调用，失败必须原样传播。
auto main() -> int
{
    fake_da_liu_ren_algorithm algorithm;
    const cnetnexus::chart::da_liu_ren_service service{algorithm, cnetnexus::chart::inline_compute()};

    const auto ok = cnetmod::sync_wait(service.cast(base_input()));
    if (!ok || algorithm.calls != 1)
        return 1;
    if (ok->analysis_zh != "课体报告" || !ok->pan_projection.is_object())
        return 2;
    if (ok->input.kind != "求财")
        return 3;

    const int before = algorithm.calls;
    auto bad_month = base_input();
    bad_month.month = 13;
    if (cnetmod::sync_wait(service.cast(bad_month)).error() != error_code::request_invalid_argument)
        return 4;
    auto bad_hour = base_input();
    bad_hour.hour = 24;
    if (cnetmod::sync_wait(service.cast(bad_hour)).error() != error_code::request_invalid_argument)
        return 5;
    auto bad_year = base_input();
    bad_year.year = 1800;
    if (cnetmod::sync_wait(service.cast(bad_year)).error() != error_code::request_invalid_argument)
        return 6;
    auto empty_kind = base_input();
    empty_kind.kind.clear();
    if (cnetmod::sync_wait(service.cast(empty_kind)).error() != error_code::request_invalid_argument)
        return 7;
    auto empty_branch = base_input();
    empty_branch.ben_ming = std::string{};
    if (cnetmod::sync_wait(service.cast(empty_branch)).error() != error_code::request_invalid_argument)
        return 8;
    if (algorithm.calls != before)
        return 9;

    // 本命/行年/性别按上游口径透传，service 不改写取值。
    auto with_markers = base_input();
    with_markers.ben_ming = "申";
    with_markers.xing_nian = "子";
    with_markers.gender = "男";
    const auto marked = cnetmod::sync_wait(service.cast(with_markers));
    if (!marked || algorithm.last.ben_ming != std::optional<std::string>{"申"})
        return 10;
    if (algorithm.last.gender != std::optional<std::string>{"男"})
        return 11;

    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.cast(base_input())).error() != error_code::algorithm_failed)
        return 12;
    return 0;
}
