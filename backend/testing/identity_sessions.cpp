/**
 * @brief 登录防护与 JWT 会话生命周期（直连真实 Redis）。
 *
 *  - login_guard：按账号、按客户端地址分别计数并锁定；成功登录只清账号计数；
 *  - jwt_session_store：open / touch / rotate / revoke，轮换后旧会话立即失效；
 *  - Redis 不可达：两者都返回 DEPENDENCY.UNAVAILABLE（fail closed），不放行。
 *
 * 连不上 Redis 时返回 77，由 CTest 的 SKIP_RETURN_CODE 记为 Skipped。
 * 可用 CNETNEXUS_TEST_REDIS_HOST / CNETNEXUS_TEST_REDIS_PORT 覆盖地址。
 */
import std;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.protocol.redis;
import cnetnexus.framework.error;
import cnetnexus.framework.security.jwt_security;
import cnetnexus.system.service.login_guard;

namespace {

using error_code = cnetnexus::framework::error::error_code;
constexpr int k_skipped = 77;

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

/// @brief 每次运行独占一个键命名空间，互不污染；残留键随 TTL 过期。
auto unique_prefix() -> std::string
{
    return std::format("cnetnexus-test:{}:identity:",
        std::chrono::steady_clock::now().time_since_epoch().count());
}

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

    [[nodiscard]] auto report(std::string_view suite) const -> int
    {
        std::println("{}: {} checks, {} failures", suite, total_, failed_);
        return failed_ == 0 ? 0 : 1;
    }

private:
    int total_{};
    int failed_{};
};

template <typename Body>
void run_with_pool(cnetmod::io_context& io, cnetmod::redis::pool_params params, Body body)
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

auto exercise_guard(std::shared_ptr<cnetmod::redis::redis_template> store,
    assertions& expect) -> cnetmod::task<void>
{
    cnetnexus::system::login_guard guard{store,
        cnetnexus::system::login_protection_options{
            .max_account_failures = 2,
            .max_client_failures = 3,
            .window_seconds = 60,
            .lock_seconds = 60}};
    const std::string client{"203.0.113.10"};

    expect.check((co_await guard.admit("alice", client)).has_value(),
        "a fresh account and client are admitted");
    co_await guard.record_failure("alice", client);
    expect.check((co_await guard.admit("alice", client)).has_value(),
        "one failure stays below the account threshold");
    co_await guard.record_failure("alice", client);
    const auto locked = co_await guard.admit("alice", client);
    expect.check(!locked && locked.error() == error_code::auth_account_locked,
        "the account locks at its failure threshold");
    const auto elsewhere = co_await guard.admit("alice", "198.51.100.20");
    expect.check(!elsewhere && elsewhere.error() == error_code::auth_account_locked,
        "an account lock holds for every client address");

    expect.check((co_await guard.admit("bob", client)).has_value(),
        "another account from the same client is admitted below the client threshold");
    co_await guard.record_failure("bob", client);
    const auto client_locked = co_await guard.admit("carol", client);
    expect.check(!client_locked && client_locked.error() == error_code::auth_account_locked,
        "a client that fails across accounts locks at the client threshold");

    co_await guard.record_failure("dave", "192.0.2.1");
    co_await guard.record_success("dave");
    co_await guard.record_failure("dave", "192.0.2.1");
    expect.check((co_await guard.admit("dave", "192.0.2.1")).has_value(),
        "a successful login resets the account failure count");
}

auto exercise_sessions(std::shared_ptr<cnetmod::redis::redis_template> store,
    assertions& expect) -> cnetmod::task<void>
{
    cnetnexus::security::jwt_session_store sessions{store, std::chrono::seconds{60}};
    const cnetnexus::security::jwt_session_record record{
        .user_id = 42, .username = "alice", .department_id = 7};

    auto opened = co_await sessions.open(record);
    expect.check(opened.has_value() && opened->size() == 32,
        "open returns a 128-bit hex session id");
    if (!opened)
        co_return;
    auto touched = co_await sessions.touch(*opened);
    expect.check(touched && *touched && (*touched)->user_id == 42 &&
            (*touched)->username == "alice" && (*touched)->department_id == 7,
        "touch reads the session record back");

    auto rotated = co_await sessions.rotate(*opened, record);
    expect.check(rotated.has_value() && *rotated != *opened,
        "rotate issues a different session id");
    if (!rotated)
        co_return;
    const auto repeated = co_await sessions.rotate(*opened, record);
    expect.check(!repeated &&
            repeated.error() == error_code::auth_token_invalid,
        "a refresh token can rotate its session only once");
    auto old_session = co_await sessions.touch(*opened);
    expect.check(old_session && !*old_session,
        "the rotated-out session is gone immediately");
    auto new_session = co_await sessions.touch(*rotated);
    expect.check(new_session && *new_session, "the rotated-in session is live");

    expect.check((co_await sessions.revoke(*rotated)).has_value(), "revoke succeeds");
    auto revoked = co_await sessions.touch(*rotated);
    expect.check(revoked && !*revoked, "a revoked session is gone immediately");
    expect.check((co_await sessions.revoke(*rotated)).has_value(), "revoke is idempotent");
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
            auto store = std::make_shared<cnetmod::redis::redis_template>(
                cnetmod::redis::redis_template{pool, {.ns = {.prefix = unique_prefix()}},
                    {}, {}, io.get()});
            co_await exercise_guard(store, expect);
            co_await exercise_sessions(store, expect);
        });
    if (!reachable)
    {
        std::println("identity_sessions: Redis {}:{} unreachable -> skipped", host, port);
        return k_skipped;
    }

    // Redis 不可达（端口 1 无监听）：fail closed。
    run_with_pool(*io, pool_parameters("127.0.0.1", 1),
        [&](cnetmod::redis::connection_pool& pool) -> cnetmod::task<void>
        {
            auto store = std::make_shared<cnetmod::redis::redis_template>(
                cnetmod::redis::redis_template{pool, {.ns = {.prefix = unique_prefix()}},
                    {}, {}, io.get()});
            cnetnexus::system::login_guard guard{store, {}};
            const auto admitted = co_await guard.admit("alice", "203.0.113.10");
            expect.check(!admitted && admitted.error() == error_code::upstream_unavailable,
                "login guard fails closed without Redis");
            cnetnexus::security::jwt_session_store sessions{store, std::chrono::seconds{60}};
            const auto touched = co_await sessions.touch(std::string(32, 'a'));
            expect.check(!touched && touched.error() == error_code::upstream_unavailable,
                "session lookup fails closed without Redis");
        });
    return expect.report("identity_sessions");
}
