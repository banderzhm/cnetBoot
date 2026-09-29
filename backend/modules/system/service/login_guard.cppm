/**
 * @brief 登录防爆破：按账号与按客户端地址分别计失败次数，超过阈值即临时锁定。
 *
 * 两个维度互补：按账号挡住“多 IP 撞一个账号”，按客户端挡住“一个 IP 撞多个账号”。
 * 计数与锁都在 Redis 里（多实例共享、随 TTL 自动过期）。成功登录清掉该账号的
 * 失败计数；客户端计数不清，避免攻击者用自己的账号穿插成功登录来重置。
 *
 * Redis 不可用时 admit() 返回 DEPENDENCY.UNAVAILABLE（fail closed）：登录本身也要
 * 写 Redis 会话，放行只会让防护在故障期间失效。
 */
export module cnetboot.system.service.login_guard;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.redis;
import cnetboot.framework.error;

export namespace cnetboot::system {

/// @brief 登录防护参数（identity.login 配置小节）。
struct login_protection_options
{
    std::uint32_t max_account_failures{5};  ///< 同一账号在窗口内允许的失败次数
    std::uint32_t max_client_failures{20};  ///< 同一客户端地址在窗口内允许的失败次数
    std::int64_t window_seconds{900};       ///< 失败计数窗口
    std::int64_t lock_seconds{900};         ///< 达到阈值后的锁定时长
};

/// @brief 结构校验：阈值与时长必须为正。
[[nodiscard]] auto validate_login_protection(const login_protection_options& options)
    -> std::expected<void, std::string>;

class login_guard
{
public:
    login_guard(std::shared_ptr<cnetmod::redis::redis_template> store,
        login_protection_options options);

    /**
     * @brief 本次登录尝试是否允许继续校验口令。
     * @return 账号或客户端处于锁定期 -> AUTH.ACCOUNT_LOCKED；
     *         Redis 不可用 -> DEPENDENCY.UNAVAILABLE
     */
    [[nodiscard]] auto admit(std::string_view username, std::string_view client)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;

    /// @brief 记一次失败；达到阈值时写入锁。计数失败只记日志，不改变登录结果。
    [[nodiscard]] auto record_failure(std::string_view username, std::string_view client)
        -> cnetmod::task<void>;

    /// @brief 登录成功：清掉该账号的失败计数。
    [[nodiscard]] auto record_success(std::string_view username) -> cnetmod::task<void>;

private:
    [[nodiscard]] auto count(std::string key, std::uint32_t limit, std::string lock)
        -> cnetmod::task<void>;

    std::shared_ptr<cnetmod::redis::redis_template> store_;
    login_protection_options options_;
};

} // namespace cnetboot::system
