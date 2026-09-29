/**
 * @brief Nexus 的应用模块：组合根按业务边界拆成 cnetmod application_module。
 *
 * 每个模块在 cnetmod 的命名构建阶段里各做各的事：
 *  - configure_options：声明自己拥有的顶层配置小节（typed options）；
 *  - register_components：注册组件（仓储、mapper、服务、模型），只声明不构造；
 *  - compose：解析组件、声明路由与中间件。
 *
 * 组件在 resolution 阶段由 cnetmod 按依赖顺序构造，缺数据源、缺服务实例、循环依赖都会
 * 让 build() 带着组件链失败，而不是在第一个请求时才暴露。组件由容器持有并与 host
 * 同寿命，路由处理器只借用它们。
 *
 * 模块顺序即中间件与路由的追加顺序：platform（入口限流、请求体上限）-> identity（认证、授权、
 * 系统路由）-> reading（LLM 目录）-> chart（术数、存档、案例、统计）-> chat。
 * chart 依赖 reading 注册的解读服务，chat 依赖 reading 注册的补全实现。
 */
export module cnetnexus.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetnexus.system.service.login_guard;

export namespace cnetnexus::bootstrap {

/// @brief 业务数据源（services.primary-database 的 cnetmod 实例名）。
inline constexpr std::string_view primary_data_source{"primary"};

/// @brief 会话存储（services.primary-cache 的 cnetmod 实例名）。
inline constexpr std::string_view session_cache{"primary"};

/// @brief 排盘结果缓存（services.calculation-cache 的 cnetmod 实例名，可选依赖）。
inline constexpr std::string_view calculation_cache{"calculation-cache"};

/// @brief chart/chat/reading 共用的 XML 语句集组件名。
inline constexpr std::string_view business_statements{"business"};

/** @brief 平台组合配置；路径不再硬编码在模块实现中。 */
struct platform_options
{
    std::string mapper_directory{"mapper"};
};

/// @brief 业务数据源上的普通仓储。
[[nodiscard]] inline auto primary_source() -> cnetmod::application::repository_options
{
    return cnetmod::application::repository_options{
        .instance = std::string{primary_data_source}};
}

/**
 * @brief 业务数据源上以可空 deleted_at 做逻辑删除的仓储（chart_case、chat_session、
 *        chat_message 同一口径，只在这里定义一次）。
 */
[[nodiscard]] inline auto soft_deleted_source()
    -> cnetmod::application::repository_options
{
    cnetmod::orm::logical_delete_config deleted_at;
    deleted_at.field_name = "deleted_at";
    deleted_at.mode = cnetmod::orm::logical_delete_mode::nullable_datetime;
    auto options = primary_source();
    options.policies.logical_delete_policy = std::move(deleted_at);
    return options;
}

/**
 * @brief 为「按值持有仓储」的组件取一份新句柄（共享数据源与策略链）。
 * @throws std::system_error 数据源不可用；resolution 阶段转成 build_error
 */
template <class T>
[[nodiscard]] auto owned_repository(cnetmod::application::component_resolver& resolver)
    -> cnetmod::application::managed_repository<T>
{
    auto made =
        resolver.get<cnetmod::application::repository_factory<T>>().make_repository();
    if (!made)
        throw std::system_error(made.error(), "repository handle cannot be created");
    return std::move(*made);
}

/**
 * @brief 平台能力：入口限流、请求体上限、XML 语句集、审计落点。
 */
class platform_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

/**
 * @brief identity 配置小节。
 */
struct identity_options
{
    /// 登录防爆破参数。
    cnetnexus::system::login_protection_options login;
    /// 授权快照进程内缓存秒数；0 关闭。会话撤销不受影响、始终即时生效。
    std::int64_t principal_cache_seconds{30};
};

/**
 * @brief 身份与权限：系统仓储、JWT 认证、端点授权、系统路由。
 */
class identity_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

/**
 * @brief 解读：llm 配置小节、多供应商补全实现、解读服务与缓存、LLM 目录路由。
 */
class reading_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

/**
 * @brief 术数：六术数、存档、批量、案例、统计。
 */
class chart_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

/**
 * @brief 对话：会话持久化、对话服务与路由。
 */
class chat_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

} // namespace cnetnexus::bootstrap
