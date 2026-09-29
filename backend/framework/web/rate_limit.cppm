/**
 * @brief 按路由声明的限流策略（端点元数据）+ 客户端地址的可信代理配置。
 *
 * 路由在注册处声明自己属于哪一个策略：
 *   routes.post(path, handler, with_rate_limit(anonymous(), rate_limit_policy::reading));
 * rate_limit_middleware() 在路由匹配之后读取该元数据，把请求交给对应策略的 cnetmod
 * rate_limiter；未声明策略的路由不限流。策略是封闭的枚举：路由不可能引用一个
 * 没有配置的策略名。
 *
 * 客户端地址一律经 cnetmod::http::resolve_client_ip(ctx, trusted_proxies) 解析：
 * 只有来自可信代理的转发头才被采信，客户端无法伪造自己的限流身份。
 */
export module cnetnexus.framework.web.rate_limit;

import std;
import cnetmod.protocol.http;

export namespace cnetnexus::framework::web {

/// @brief 限流策略；每个策略一份独立的全进程令牌桶（按客户端地址分桶）。
enum class rate_limit_policy
{
    login,         ///< POST /api/auth/login：防口令爆破的第一道闸
    reading,       ///< LLM 解读：每次调用都产生上游费用
    archive_write, ///< 写入存档：落盘 / 落库
};

/// @brief 端点元数据：本路由受 policy 限流。
struct rate_limited
{
    rate_limit_policy policy{};
};

/// @brief 一个策略的令牌桶参数。
struct rate_limit_rule
{
    double rate_per_second{1.0}; ///< 稳态速率
    double burst{5.0};           ///< 突发容量
};

/// @brief 全部策略（封闭集合，配置里出现未知策略名即构建失败）。
struct rate_limit_rules
{
    rate_limit_rule login{.rate_per_second = 0.2, .burst = 5.0};
    rate_limit_rule reading{.rate_per_second = 0.05, .burst = 3.0};
    rate_limit_rule archive_write{.rate_per_second = 0.2, .burst = 5.0};
};

/// @brief 顶层 edge 配置小节：入口层的客户端识别与限流。
struct edge_options
{
    /// 可信反向代理（地址或 CIDR）；只有它们的 X-Forwarded-For 被采信。
    std::vector<std::string> trusted_proxies;
    rate_limit_rules rate_limits;
};

/// @brief 顶层配置小节名。
inline constexpr std::string_view edge_section_name{"edge"};

/// @brief 结构校验：速率与突发必须为正。
[[nodiscard]] auto validate_edge_options(const edge_options& options)
    -> std::expected<void, std::string>;

/// @brief 在已有端点元数据上追加限流策略。
[[nodiscard]] auto with_rate_limit(cnetmod::http::endpoint_metadata metadata,
    rate_limit_policy policy) -> cnetmod::http::endpoint_metadata;

/// @brief 本请求的客户端地址（按 trusted_proxies 解析）。
[[nodiscard]] auto client_ip(const cnetmod::http::request_context& request,
    const edge_options& options) -> std::string;

/**
 * @brief 按端点元数据分发到各策略的限流中间件；超限时返回 REQUEST.RATE_LIMITED。
 */
[[nodiscard]] auto rate_limit_middleware(const edge_options& options)
    -> cnetmod::http::middleware_fn;

} // namespace cnetnexus::framework::web
