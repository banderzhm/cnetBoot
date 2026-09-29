export module cnetnexus.framework.security.jwt_security;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetmod.protocol.redis;
import cnetmod.orm;
import cnetnexus.framework.error;
import cnetnexus.framework.security.current_user;

export namespace cnetnexus::security {

struct authenticated_context
{
    current_user user;
    cnetmod::orm::data_permission_scope data_permission;
};

struct jwt_session_record
{
    std::int64_t user_id{};
    std::string username;
    std::int64_t department_id{};
};

/// @brief 本次请求所用会话的 ID（绑定在请求作用域，供 refresh / logout 使用）。
struct jwt_session_id
{
    std::string value;
};

/**
 * @brief 按用户 id 加载请求主体（用户状态、角色、权限、数据范围）。
 * @return 主体不存在或已删除 -> AUTH.TOKEN_INVALID；已停用 -> AUTH.ACCOUNT_DISABLED；
 *         数据源故障 -> DEPENDENCY.UNAVAILABLE
 */
using principal_loader = std::function<cnetmod::task<
    std::expected<authenticated_context, cnetnexus::framework::error::error_code>>(
    std::int64_t)>;

[[nodiscard]] auto jwt_session_key(std::string_view session_id)
    -> std::string;

/**
 * @brief JWT 会话的唯一读写入口（Redis）。
 *
 * 会话 ID 是 128 位随机数；会话记录只有身份，不含授权。每次认证请求使用 Redis
 * GETEX 原子读取并续期；refresh 使用应用层 Lua 状态转换原子替换旧会话。
 */
class jwt_session_store
{
public:
    jwt_session_store(std::shared_ptr<cnetmod::redis::redis_template> redis,
        std::chrono::seconds idle_timeout);

    /// @brief 新建会话，返回会话 ID。
    [[nodiscard]] auto open(const jwt_session_record& record)
        -> cnetmod::task<std::expected<std::string, cnetnexus::framework::error::error_code>>;

    /// @brief 读取并续期；会话不存在（过期或已撤销）返回 nullopt。
    [[nodiscard]] auto touch(std::string_view session_id)
        -> cnetmod::task<std::expected<std::optional<jwt_session_record>,
            cnetnexus::framework::error::error_code>>;

    /// @brief 原子换发；并发 refresh 只有一个能消费旧会话。
    [[nodiscard]] auto rotate(std::string_view session_id, const jwt_session_record& record)
        -> cnetmod::task<std::expected<std::string, cnetnexus::framework::error::error_code>>;

    /// @brief 撤销会话（幂等）。
    [[nodiscard]] auto revoke(std::string_view session_id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;

private:
    std::shared_ptr<cnetmod::redis::redis_template> redis_;
    std::chrono::seconds idle_timeout_;
};

/**
 * @brief Collaborators of the JWT middleware, resolved by the composition root.
 */
struct jwt_security_options
{
    /// Validated security.jwt settings; lives as long as the application host.
    const cnetmod::application::jwt_configuration* settings{};
    /// Host execution context used to verify signatures off the event loop.
    cnetmod::application::execution_context* executor{};
    std::shared_ptr<jwt_session_store> sessions;
    principal_loader load_principal;
    /// 授权快照在进程内的缓存时长。会话撤销始终逐请求校验、立即生效；停用账号或
    /// 调整角色权限最迟在该时长后生效。0 表示不缓存。
    std::chrono::seconds principal_cache_ttl{30};
};

/**
 * @brief Verifies JWTs once and propagates their principal through the request.
 *
 * Whether a route needs credentials is declared on the route through endpoint
 * metadata (allow_anonymous / optional_authentication). On optional routes a
 * presented but invalid token is rejected with 401 instead of silently
 * degrading to anonymous access. Failures answer with registered AUTH.* codes.
 */
[[nodiscard]] auto jwt_security(jwt_security_options options)
    -> cnetmod::http::middleware_fn;

} // namespace cnetnexus::security
