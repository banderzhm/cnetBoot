/**
 * @brief bazi_service 的 Redis 命盘缓存用例（直连真实 Redis）。
 *
 * 缓存由 Application 托管的 redis_template 提供，本用例用同一类型直连 Redis：
 *  - 命中不触达算法，命盘字段完整往返（Glaze 序列化）；
 *  - 不同输入、不同流派各自成键；
 *  - 失败结果不入缓存；流年不缓存；
 *  - Redis 不可达时按 pool_timeout 失败并降级为直接排盘。
 *
 * 连不上 Redis 时返回 77，由 CTest 的 SKIP_RETURN_CODE 记为 Skipped，
 * 不会被误记成通过。可用 CNETNEXUS_TEST_REDIS_HOST / CNETNEXUS_TEST_REDIS_PORT 覆盖。
 */
import std;
import cnetnexus.chart.service.compute_executor;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.json;
import cnetmod.protocol.redis;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

constexpr int k_skipped = 77;

/// @brief 计数替身：每次返回同一份完整命盘，用于观察缓存是否拦住了调用。
class counting_bazi_algorithm final : public cnetnexus::chart::bazi_algorithm
{
public:
    int calls{0};
    int liu_nian_calls{0};
    std::optional<error_code> failure{};

    auto pai_pan(const cnetnexus::chart::bazi::birth_input& input)
        -> std::expected<cnetnexus::chart::bazi::chart, error_code> override
    {
        ++calls;
        if (failure)
            return std::unexpected{*failure};
        cnetnexus::chart::bazi::chart chart{};
        chart.birth = input;
        chart.pillars = {"庚辰", "壬午", "甲辰", "壬申"};
        chart.xun_kong_1 = "寅";
        chart.xun_kong_2 = "卯";
        chart.chart_zh = "【四柱八字】年柱：庚辰  月柱：壬午  日柱：甲辰  时柱：壬申";
        chart.analysis_zh = "八字分析报告占位";
        chart.chart_projection = cnetmod::json::object({
            {"method", "ziping"},
            {"dayMaster", cnetmod::json::object({{"gan", "甲"}})},
        });
        chart.analysis_projection = cnetmod::json::object({
            {"schema_version", "1.7"}, {"day_master", "甲"}});
        return chart;
    }

    auto liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input)
        -> std::expected<cnetnexus::chart::bazi::liu_nian_result, error_code> override
    {
        ++liu_nian_calls;
        cnetnexus::chart::bazi::liu_nian_result result{};
        result.birth = input.birth;
        result.target_year = input.target_year;
        result.chart_projection = cnetmod::json::object();
        return result;
    }
};

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

auto environment_or(const char* name, std::string fallback) -> std::string
{
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::string{value} : std::move(fallback);
}

auto pool_parameters(std::string host, std::uint16_t port) -> cnetmod::redis::pool_params
{
    return {.host = std::move(host),
        .port = port,
        .initial_size = 1,
        .max_size = 2,
        .connect_timeout = std::chrono::milliseconds{500},
        .pool_timeout = std::chrono::milliseconds{500},
        .retry_interval = std::chrono::milliseconds{100},
        .ping_interval = std::chrono::hours{1}};
}

/// @brief 每次运行独占一个键前缀，互不污染；残留键随 TTL 过期。
auto unique_prefix() -> std::string
{
    return std::format("cnetnexus-test:{}:bazi:",
        std::chrono::steady_clock::now().time_since_epoch().count());
}

/// @brief 断言收集器：失败只打印一行，最后统一给出退出码。
class assertions
{
public:
    void check(bool condition, std::string_view what)
    {
        ++total_;
        if (!condition)
        {
            ++failed_;
            std::println("FAIL: {}", what);
        }
    }

    [[nodiscard]] auto failed() const noexcept -> int
    {
        return failed_;
    }

    [[nodiscard]] auto total() const noexcept -> int
    {
        return total_;
    }

private:
    int total_{};
    int failed_{};
};

/// @brief 命中、成键、失败不缓存、流年不缓存。
auto exercise_cache(cnetnexus::chart::bazi_service& service,
    counting_bazi_algorithm& inner, assertions& expect) -> cnetmod::task<void>
{
    const auto first = co_await service.pai_pan(base_input());
    expect.check(first && inner.calls == 1, "first call computes");

    const auto second = co_await service.pai_pan(base_input());
    expect.check(second && inner.calls == 1, "same input hits the cache");
    if (first && second)
    {
        expect.check(second->pillars.year == "庚辰" && second->pillars.hour == "壬申",
            "pillars round-trip");
        expect.check(second->xun_kong_1 == "寅" && second->xun_kong_2 == "卯",
            "xun kong round-trips");
        expect.check(second->chart_zh == first->chart_zh &&
                second->analysis_zh == first->analysis_zh,
            "chinese reports round-trip");
        expect.check(cnetmod::json::equivalent(second->chart_projection,
                         first->chart_projection) &&
                cnetmod::json::equivalent(second->analysis_projection,
                    first->analysis_projection),
            "raw json payloads round-trip");
        expect.check(second->birth.hour == 16 && second->birth.is_male &&
                second->birth.minute == 30,
            "birth input round-trips");
    }

    auto other_hour = base_input();
    other_hour.hour = 17;
    const auto third = co_await service.pai_pan(other_hour);
    expect.check(third && inner.calls == 2 && third->birth.hour == 17,
        "a different input is a different key");

    auto mangpai = base_input();
    mangpai.method = cnetnexus::chart::bazi::analysis_method::mangpai;
    (void)co_await service.pai_pan(mangpai);
    expect.check(inner.calls == 3, "the analysis method is part of the key");

    auto failing = base_input();
    failing.hour = 18;
    inner.failure = error_code::algorithm_failed;
    const auto failed_once = co_await service.pai_pan(failing);
    const int after_failure = inner.calls;
    const auto failed_twice = co_await service.pai_pan(failing);
    expect.check(!failed_once && failed_once.error() == error_code::algorithm_failed,
        "algorithm failure propagates");
    expect.check(!failed_twice && inner.calls == after_failure + 1,
        "failures are not cached");
    inner.failure.reset();

    cnetnexus::chart::bazi::liu_nian_input transit{};
    transit.birth = base_input();
    transit.target_year = 2026;
    (void)service.liu_nian(transit);
    (void)service.liu_nian(transit);
    expect.check(inner.liu_nian_calls == 2, "liu nian is never cached");
}

/// @brief 跑一个 Redis 场景：启动连接池，执行 body，收尾取消连接池。
template <typename Body>
auto run_with_pool(cnetmod::io_context& io, cnetmod::redis::pool_params params,
    Body body) -> void
{
    cnetmod::redis::connection_pool pool{io, std::move(params)};
    bool run_finished = false;
    auto runner = [&]() -> cnetmod::task<void>
    {
        co_await pool.async_run();
        run_finished = true;
    };
    auto driver = [&]() -> cnetmod::task<void>
    {
        co_await body(pool);
        co_await pool.cancel();
        while (!run_finished)
            co_await cnetmod::async_sleep(io, std::chrono::milliseconds{1});
        io.stop();
    };
    cnetmod::spawn(io, runner());
    cnetmod::spawn(io, driver());
    io.run();
    io.restart();
}

} // namespace

auto main() -> int
{
    const auto host = environment_or("CNETNEXUS_TEST_REDIS_HOST", "127.0.0.1");
    const auto port = static_cast<std::uint16_t>(
        std::stoi(environment_or("CNETNEXUS_TEST_REDIS_PORT", "6379")));

    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;

    assertions expect;
    bool reachable = false;
    run_with_pool(*io, pool_parameters(host, port),
        [&](cnetmod::redis::connection_pool& pool) -> cnetmod::task<void>
        {
            cnetmod::redis::redis_template probe{pool, {}, {}, {}, io.get()};
            reachable = (co_await probe.get("cnetnexus-test:probe")).has_value();
            if (!reachable)
                co_return;

            counting_bazi_algorithm inner;
            cnetnexus::chart::bazi_service service{inner, cnetnexus::chart::inline_compute(),
                cnetmod::redis::redis_template{pool, {.ns = {.prefix = unique_prefix()}},
                    {}, {}, io.get()}};
            co_await exercise_cache(service, inner, expect);
        });
    if (!reachable)
    {
        std::println("chart_bazi_cache: Redis {}:{} unreachable -> skipped", host, port);
        return k_skipped;
    }

    // Redis 不可达（端口 1 无监听）：缓存读写都失败，但排盘照常完成且不报错。
    if (environment_or("CNETNEXUS_TEST_REDIS_DEGRADED", "1") == "1")
    {
        run_with_pool(*io, pool_parameters("127.0.0.1", 1),
            [&](cnetmod::redis::connection_pool& pool) -> cnetmod::task<void>
            {
                counting_bazi_algorithm inner;
                cnetnexus::chart::bazi_service service{inner, cnetnexus::chart::inline_compute(),
                    cnetmod::redis::redis_template{pool, {.ns = {.prefix = unique_prefix()}},
                        {}, {}, io.get()}};
                const auto first = co_await service.pai_pan(base_input());
                const auto second = co_await service.pai_pan(base_input());
                expect.check(first && second && inner.calls == 2,
                    "an unreachable cache degrades to computing every time");
            });
    }

    std::println("chart_bazi_cache: {} assertions, {} failures", expect.total(), expect.failed());
    return expect.failed() == 0 ? 0 : 1;
}
