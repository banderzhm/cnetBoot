module cnetnexus.system.service.login_guard;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.protocol.redis;
import cnetnexus.framework.error;

namespace cnetnexus::system {
namespace {

    using error_code = cnetnexus::framework::error::error_code;

    constexpr std::string_view key_prefix{"cnetnexus:auth:"};

    /// @brief 账号名进入 Redis 键前做十六进制编码：键里不出现任何用户可控的分隔符。
    [[nodiscard]] auto encode(std::string_view value) -> std::string
    {
        constexpr std::string_view digits{"0123456789abcdef"};
        std::string result;
        result.reserve(value.size() * 2);
        for (const auto character : value)
        {
            const auto byte = static_cast<unsigned char>(character);
            result.push_back(digits[byte >> 4]);
            result.push_back(digits[byte & 0x0f]);
        }
        return result;
    }

    [[nodiscard]] auto account_failures(std::string_view username) -> std::string
    {
        return std::format("{}fail:account:{}", key_prefix, encode(username));
    }

    [[nodiscard]] auto account_lock(std::string_view username) -> std::string
    {
        return std::format("{}lock:account:{}", key_prefix, encode(username));
    }

    [[nodiscard]] auto client_failures(std::string_view client) -> std::string
    {
        return std::format("{}fail:client:{}", key_prefix, encode(client));
    }

    [[nodiscard]] auto client_lock(std::string_view client) -> std::string
    {
        return std::format("{}lock:client:{}", key_prefix, encode(client));
    }

} // namespace

auto validate_login_protection(const login_protection_options& options)
    -> std::expected<void, std::string>
{
    if (options.max_account_failures == 0 || options.max_client_failures == 0)
        return std::unexpected{std::string{"login 失败阈值必须 >= 1"}};
    if (options.window_seconds <= 0 || options.lock_seconds <= 0)
        return std::unexpected{std::string{"login window_seconds / lock_seconds 必须 > 0"}};
    return {};
}

login_guard::login_guard(std::shared_ptr<cnetmod::redis::redis_template> store,
    login_protection_options options)
    : store_{std::move(store)},
      options_{options}
{
    if (!store_)
        throw std::invalid_argument{"login guard requires a redis store"};
    if (auto valid = validate_login_protection(options_); !valid)
        throw std::invalid_argument{valid.error()};
}

auto login_guard::admit(std::string_view username, std::string_view client)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const std::vector<std::string> locks{account_lock(username), client_lock(client)};
    auto held = co_await store_->mget(locks);
    if (!held)
    {
        logger::warn{"login guard unavailable: {}", held.error().message()};
        co_return std::unexpected{error_code::upstream_unavailable};
    }
    if (std::ranges::any_of(*held, [](const auto& value) { return value.has_value(); }))
        co_return std::unexpected{error_code::auth_account_locked};
    co_return std::expected<void, error_code>{};
}

auto login_guard::record_failure(std::string_view username, std::string_view client)
    -> cnetmod::task<void>
{
    co_await count(account_failures(username), options_.max_account_failures,
        account_lock(username));
    co_await count(client_failures(client), options_.max_client_failures,
        client_lock(client));
}

auto login_guard::record_success(std::string_view username) -> cnetmod::task<void>
{
    if (auto cleared = co_await store_->del(account_failures(username)); !cleared)
        logger::warn{"login guard could not clear failures: {}",
            cleared.error().message()};
}

auto login_guard::count(std::string key, std::uint32_t limit, std::string lock)
    -> cnetmod::task<void>
{
    // INCR 与 EXPIRE 同一次往返。每次失败都刷新窗口（滑动窗口：距上次失败
    // window_seconds 内无新失败才清零）；达到阈值时写入独立的锁键。
    auto batch = store_->pipeline();
    batch.incr(key).expire(key, std::chrono::seconds{options_.window_seconds});
    auto replies = co_await store_->execute(batch);
    if (!replies || replies->empty() || !(*replies)[0] || (*replies)[0]->empty())
    {
        logger::warn{"login guard could not record a failure"};
        co_return;
    }
    const auto failures = (*replies)[0]->front().as_integer();
    if (failures < static_cast<std::int64_t>(limit))
        co_return;
    if (auto locked = co_await store_->set(lock, "1",
            std::chrono::seconds{options_.lock_seconds});
        !locked)
        logger::warn{"login guard could not write a lock: {}", locked.error().message()};
}

} // namespace cnetnexus::system
