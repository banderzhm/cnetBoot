import std;
import cnetnexus.chart.service.compute_executor;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.json;
import cnetnexus.chart.domain.bazi;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.bazi_service;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录调用参数的测试替身；只在测试内使用，不代表生产契约。
///  玄学的 bazi_port 在 CNetNexus 里是 chart 模块内的 bazi_algorithm 接口。
class fake_bazi_algorithm final : public cnetnexus::chart::bazi_algorithm
{
public:
    int calls{0};
    int liu_nian_calls{0};
    cnetnexus::chart::bazi::birth_input last{};
    cnetnexus::chart::bazi::liu_nian_input last_liu_nian{};
    std::optional<error_code> failure{};

    auto pai_pan(const cnetnexus::chart::bazi::birth_input& input)
        -> std::expected<cnetnexus::chart::bazi::chart, error_code> override
    {
        ++calls;
        last = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::bazi::chart chart{};
        chart.birth = input;
        chart.pillars = {"甲子", "乙丑", "丙寅", "丁卯"};
        chart.chart_projection = cnetmod::json::object(
            {{"dayMaster", cnetmod::json::object({{"gan", "丙"}})}});
        chart.analysis_projection =
            cnetmod::json::object({{"schemaVersion", "1.0"}});
        return chart;
    }

    auto liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input)
        -> std::expected<cnetnexus::chart::bazi::liu_nian_result, error_code> override
    {
        ++liu_nian_calls;
        last_liu_nian = input;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::bazi::liu_nian_result result{};
        result.birth = input.birth;
        result.target_year = input.target_year;
        result.chart_projection = cnetmod::json::object();
        return result;
    }
};

/// @brief 构造一个合法输入，具体字段由各用例覆盖。
auto base_input() -> cnetnexus::chart::bazi::birth_input
{
    return cnetnexus::chart::bazi::birth_input{.calendar = cnetnexus::chart::bazi::calendar_kind::solar,
        .year = 2000,
        .month = 6,
        .day = 15,
        .hour = 16,
        .minute = 30,
        .is_male = true};
}

/// @brief 在测试自持的事件循环上跑完一个协程并取回结果。
template <typename T> auto drive(cnetmod::io_context& io, cnetmod::task<T> work) -> T
{
    std::optional<T> result;
    auto runner = [&io, &result, work = std::move(work)]() mutable -> cnetmod::task<void>
    {
        result.emplace(co_await std::move(work));
        io.stop();
    };
    cnetmod::spawn(io, runner());
    io.run();
    io.restart();
    return std::move(*result);
}

} // namespace

// service 用例：请求范围校验必须先于算法调用，算法失败必须原样传播。
auto main() -> int
{
    fake_bazi_algorithm algorithm;
    cnetnexus::chart::bazi_service service{algorithm, cnetnexus::chart::inline_compute()};
    auto io = cnetmod::make_io_context();
    if (!io)
        return 21;
    // 未注入缓存：pai_pan 只做校验与委派，协程在本地事件循环上同步跑完。
    const auto pai_pan = [&](cnetnexus::chart::bazi::birth_input input)
    {
        return drive(*io, service.pai_pan(std::move(input)));
    };

    // 1. 合法输入：委派给算法接口且回显输入。
    const auto ok = pai_pan(base_input());
    if (!ok || algorithm.calls != 1)
        return 1;
    if (ok->pillars.year != "甲子" || ok->birth.hour != 16)
        return 2;
    if (algorithm.last.minute != 30)
        return 3;
    const auto encoded = cnetmod::json::write(*ok);
    const auto decoded = encoded
        ? cnetmod::json::parse<cnetnexus::chart::bazi::chart>(*encoded)
        : std::expected<cnetnexus::chart::bazi::chart, std::error_code>{
              std::unexpected{encoded.error()}};
    if (!decoded ||
        !cnetmod::json::equivalent(
            decoded->chart_projection, ok->chart_projection) ||
        !cnetmod::json::equivalent(
            decoded->analysis_projection, ok->analysis_projection))
        return 22;

    // 2. 越界输入：不得触达算法。
    const int before = algorithm.calls;
    auto bad_hour = base_input();
    bad_hour.hour = 24;
    if (pai_pan(bad_hour).error() != error_code::request_invalid_argument)
        return 4;
    auto bad_month = base_input();
    bad_month.month = 13;
    if (pai_pan(bad_month).error() != error_code::request_invalid_argument)
        return 5;
    auto bad_year = base_input();
    bad_year.year = 1800;
    if (pai_pan(bad_year).error() != error_code::request_invalid_argument)
        return 6;
    auto bad_day = base_input();
    bad_day.day = 0;
    if (pai_pan(bad_day).error() != error_code::request_invalid_argument)
        return 7;
    if (algorithm.calls != before)
        return 8;

    // 3. 农历闰月用负数月份表示，必须放行；公历负数月份必须拒绝。
    auto lunar = base_input();
    lunar.calendar = cnetnexus::chart::bazi::calendar_kind::lunar;
    lunar.month = -6;
    if (!pai_pan(lunar))
        return 9;
    auto solar_negative = base_input();
    solar_negative.month = -6;
    if (pai_pan(solar_negative).error() != error_code::request_invalid_argument)
        return 10;

    // 4. 算法失败原样传播。
    algorithm.failure = error_code::bazi_chart_unavailable;
    if (pai_pan(base_input()).error() != error_code::bazi_chart_unavailable)
        return 11;
    algorithm.failure.reset();

    // 5. 流年：出生输入 + 目标年一起校验，范围内才委派给算法。
    cnetnexus::chart::bazi::liu_nian_input liu_nian{};
    liu_nian.birth = base_input();
    liu_nian.target_year = 2026;
    const auto transit = cnetmod::sync_wait(service.liu_nian(liu_nian));
    if (!transit || algorithm.liu_nian_calls != 1)
        return 12;
    if (transit->target_year != 2026 || algorithm.last_liu_nian.target_year != 2026)
        return 13;

    // 6. 目标年与出生信息越界都不得触达算法。
    const int before_liu_nian = algorithm.liu_nian_calls;
    auto bad_target = liu_nian;
    bad_target.target_year = 1800;
    if (cnetmod::sync_wait(service.liu_nian(bad_target)).error() != error_code::request_invalid_argument)
        return 14;
    auto bad_birth = liu_nian;
    bad_birth.birth.hour = 24;
    if (cnetmod::sync_wait(service.liu_nian(bad_birth)).error() != error_code::request_invalid_argument)
        return 15;
    // 大运序号只接受 0-9。
    auto bad_index_high = liu_nian;
    bad_index_high.da_yun_index = 10;
    if (cnetmod::sync_wait(service.liu_nian(bad_index_high)).error() != error_code::request_invalid_argument)
        return 16;
    auto bad_index_low = liu_nian;
    bad_index_low.da_yun_index = -1;
    if (cnetmod::sync_wait(service.liu_nian(bad_index_low)).error() != error_code::request_invalid_argument)
        return 17;
    if (algorithm.liu_nian_calls != before_liu_nian)
        return 18;

    // 7. 指定大运序号时原样透传。
    auto indexed = liu_nian;
    indexed.da_yun_index = 3;
    if (!cnetmod::sync_wait(service.liu_nian(indexed)) || !algorithm.last_liu_nian.da_yun_index ||
        *algorithm.last_liu_nian.da_yun_index != 3)
        return 19;

    // 8. 流年算法失败原样传播。
    algorithm.failure = error_code::algorithm_failed;
    if (cnetmod::sync_wait(service.liu_nian(liu_nian)).error() != error_code::algorithm_failed)
        return 20;
    return 0;
}
