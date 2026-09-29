import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
class fake_liu_yao_algorithm final : public cnetnexus::chart::liu_yao_algorithm
{
public:
    int calls{0};
    cnetnexus::chart::liu_yao::cast_input last{};
    std::optional<error_code> failure{};

    auto cast(const cnetnexus::chart::liu_yao::cast_input& input)
        -> std::expected<cnetnexus::chart::liu_yao::cast_result, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::liu_yao::cast_result result{};
        result.input = input;
        result.analysis_zh = "断卦结果";
        return result;
    }
};

/// @brief 合法输入；具体字段由各用例覆盖。
auto base_input() -> cnetnexus::chart::liu_yao::cast_input
{
    return cnetnexus::chart::liu_yao::cast_input{.year = 2025,
        .month = 4,
        .day = 7,
        .hour = 17,
        .minute = 3,
        .hexagram_code = "110001",
        .changing_lines = {1, 3},
        .question = "所问回款能否落实"};
}

} // namespace

/// @brief service 用例：范围校验必须先于算法调用，失败必须原样传播。
auto main() -> int
{
    fake_liu_yao_algorithm algorithm;
    const cnetnexus::chart::liu_yao_service service{algorithm, cnetnexus::chart::inline_compute()};

    // 1. 合法输入：委派给算法。
    const auto ok = cnetmod::sync_wait(service.cast(base_input()));
    if (!ok || algorithm.calls != 1)
        return 1;
    if (ok->analysis_zh != "断卦结果" || ok->input.hexagram_code != "110001")
        return 2;

    // 2. 动爻去重并升序后再交给算法。
    auto duplicated = base_input();
    duplicated.changing_lines = {3, 1, 3, 1};
    const auto normalized = cnetmod::sync_wait(service.cast(duplicated));
    if (!normalized)
        return 3;
    if (algorithm.last.changing_lines != std::vector<int>{1, 3})
        return 4;

    // 3. 爻码必须是恰好六位 0/1。
    const int before = algorithm.calls;
    for (const std::string_view code :
        {std::string_view{"11000"}, std::string_view{"1100011"}, std::string_view{"11000x"}, std::string_view{""}})
    {
        auto bad = base_input();
        bad.hexagram_code = std::string{code};
        if (cnetmod::sync_wait(service.cast(bad)).error() != error_code::request_invalid_argument)
            return 5;
    }

    // 4. 动爻爻位必须在 1—6。
    for (const int line : {0, 7, -1})
    {
        auto bad = base_input();
        bad.changing_lines = {line};
        if (cnetmod::sync_wait(service.cast(bad)).error() != error_code::request_invalid_argument)
            return 6;
    }

    // 5. 时间范围。
    auto bad_hour = base_input();
    bad_hour.hour = 24;
    if (cnetmod::sync_wait(service.cast(bad_hour)).error() != error_code::request_invalid_argument)
        return 7;
    auto bad_year = base_input();
    bad_year.year = 1800;
    if (cnetmod::sync_wait(service.cast(bad_year)).error() != error_code::request_invalid_argument)
        return 8;

    if (algorithm.calls != before)
        return 9;

    // 6. 算法失败原样传播。
    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.cast(base_input())).error() != error_code::algorithm_failed)
        return 10;
    return 0;
}