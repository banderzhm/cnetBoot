/**
 * @brief identity_module：系统仓储、JWT 认证、端点授权、登录防护与系统路由。
 *
 * 认证与授权都由端点元数据驱动：路由在注册处声明 anonymous() / optional_identity() /
 * requires_permission()，中间件在路由匹配之后读取，不再维护第二份免鉴权白名单。
 * 未匹配任何路由的请求直接落到 404 / 405，不经过认证。
 */
module cnetboot.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetmod.protocol.redis;
import cnetmod.security.jwt;
import cnetmod.security.password;
import cnetboot.framework.error;
import cnetboot.framework.security.authorization;
import cnetboot.framework.security.current_user;
import cnetboot.framework.security.jwt_security;
import cnetboot.framework.web.rate_limit;
import cnetboot.system.controller.auth_controller;
import cnetboot.system.controller.authorization_controller;
import cnetboot.system.controller.user_controller;
import cnetboot.system.domain.sys_dept;
import cnetboot.system.domain.sys_menu;
import cnetboot.system.domain.sys_role;
import cnetboot.system.domain.sys_role_dept;
import cnetboot.system.domain.sys_user;
import cnetboot.system.mapper.menu;
import cnetboot.system.mapper.registry;
import cnetboot.system.mapper.role;
import cnetboot.system.service.authorization_service;
import cnetboot.system.service.auth_service;
import cnetboot.system.service.login_guard;
import cnetboot.system.service.user_service;

namespace cnetboot::bootstrap {
namespace {

    namespace application = cnetmod::application;
    namespace system = cnetboot::system;
    using error_code = cnetboot::framework::error::error_code;

    constexpr std::string_view identity_section{"identity"};

    /// @brief 身份相关 Redis 模板的组件名（会话与登录防护的键各自带前缀，模板不加命名空间）。
    constexpr std::string_view identity_store{"identity"};

    /// @brief 登录时“账号不存在”分支用于等价校验的假口令哈希。
    struct decoy_password
    {
        std::string encoded;
    };

    /**
     * @brief 按用户 id 加载请求主体：用户状态 + 角色/权限 + 数据权限范围。
     *
     * 名字化的函数对象（项目硬要求：禁止匿名内部类），持有两个容器组件的共享所有权。
     */
    class principal_loader_fn
    {
    public:
        principal_loader_fn(
            std::shared_ptr<application::managed_repository<system::sys_user>> users,
            std::shared_ptr<system::authorization_service> authorization) noexcept
            : users_{std::move(users)},
              authorization_{std::move(authorization)}
        {
        }

        [[nodiscard]] auto operator()(std::int64_t id) const
            -> cnetmod::task<std::expected<security::authenticated_context, error_code>>
        {
            return load(users_, authorization_, id);
        }

    private:
        [[nodiscard]] static auto load(
            std::shared_ptr<application::managed_repository<system::sys_user>> users,
            std::shared_ptr<system::authorization_service> authorization,
            std::int64_t id)
            -> cnetmod::task<std::expected<security::authenticated_context, error_code>>
        {
            auto result = co_await users->get_by_id(cnetmod::orm::param_value::from_int(id));
            if (!result.ok())
                co_return std::unexpected(error_code::upstream_unavailable);

            auto user = result.first();
            if (!user || user->deleted != 0)
                co_return std::unexpected(error_code::auth_token_invalid);
            if (user->status != 0)
                co_return std::unexpected(error_code::auth_account_disabled);

            auto principal = co_await authorization->principal(id, user->dept_id);
            if (!principal)
                co_return std::unexpected(principal.error());

            security::current_user current{
                .id = id,
                .department_id = user->dept_id,
                .username = std::move(user->user_name),
                .permissions = std::move(principal->authorization.permissions),
            };
            for (const auto& role : principal->authorization.roles)
                current.roles.push_back(role.key);
            co_return security::authenticated_context{
                .user = std::move(current),
                .data_permission = std::move(principal->data_permission),
            };
        }

        std::shared_ptr<application::managed_repository<system::sys_user>> users_;
        std::shared_ptr<system::authorization_service> authorization_;
    };

    /** Resolves one data-permission-bound user service for a request. */
    class user_service_resolver_fn
    {
    public:
        user_service_resolver_fn(
            application::repository_factory<system::sys_user>& users,
            std::shared_ptr<system::user_service_factory> services) noexcept
            : users_{&users}, services_{std::move(services)}
        {
        }

        [[nodiscard]] auto operator()(
            const cnetmod::http::request_context& request) const
            -> std::expected<std::unique_ptr<system::user_service>, error_code>
        {
            auto users = users_->for_request(request);
            if (!users)
                return std::unexpected{error_code::upstream_unavailable};
            return services_->make(std::move(*users));
        }

    private:
        application::repository_factory<system::sys_user>* users_;
        std::shared_ptr<system::user_service_factory> services_;
    };

    [[nodiscard]] auto validate_identity_options(const identity_options& options)
        -> std::expected<void, std::string>
    {
        if (options.principal_cache_seconds < 0)
            return std::unexpected{std::string{"principal_cache_seconds 不能为负"}};
        return system::validate_login_protection(options.login);
    }

} // namespace

auto identity_module::name() const -> std::string_view
{
    return "identity";
}

void identity_module::configure_options(application::options_registry& options)
{
    options.section<identity_options>(std::string{identity_section})
        .validate(validate_identity_options);
}

auto identity_module::register_components(application::registration_context& context)
    -> std::expected<void, std::string>
{
    const auto options = context.options.current<identity_options>(identity_section);
    const auto& jwt = context.configuration.security.jwt;

    // 系统 XML 语句集是托管服务：run() 期间发布快照，健康检查可见。
    auto statements = std::make_shared<system::system_mapper_registry>();
    context.manage(statements);
    context.components.instance(std::move(statements));

    application::add_repository<system::sys_user>(context.components, primary_source());
    application::add_repository<system::sys_role>(context.components, primary_source());
    application::add_repository<system::sys_menu>(context.components, primary_source());
    application::add_repository<system::sys_role_dept>(context.components,
        primary_source());
    application::add_repository<system::sys_dept>(context.components, primary_source());

    context.components.singleton<system::role_mapper>(
        [](application::component_resolver& resolver)
        {
            return std::make_shared<system::role_mapper>(
                owned_repository<system::sys_role>(resolver),
                resolver.shared<system::system_mapper_registry>());
        });
    context.components.singleton<system::menu_mapper>(
        [](application::component_resolver& resolver)
        {
            return std::make_shared<system::menu_mapper>(
                owned_repository<system::sys_menu>(resolver),
                resolver.shared<system::system_mapper_registry>());
        });
    context.components.singleton<system::authorization_service>(
        [](application::component_resolver& resolver)
        {
            return std::make_shared<system::authorization_service>(
                resolver.shared<system::role_mapper>(),
                resolver.shared<system::menu_mapper>(),
                owned_repository<system::sys_role_dept>(resolver),
                owned_repository<system::sys_dept>(resolver),
                resolver.shared<system::system_mapper_registry>());
        });
    context.components.singleton<system::user_service_factory>(
        [](application::component_resolver& resolver)
        {
            return std::make_shared<system::user_service_factory>(
                resolver.shared<system::system_mapper_registry>());
        });

    context.components.singleton<cnetmod::redis::redis_template>(
        std::string{identity_store},
        [](application::component_resolver& resolver)
        {
            return std::make_shared<cnetmod::redis::redis_template>(
                resolver.get<application::redis_service>(session_cache).make_template());
        });
    context.components.singleton<security::jwt_session_store>(
        [idle = std::chrono::seconds{jwt.session_idle_seconds}](
            application::component_resolver& resolver)
        {
            return std::make_shared<security::jwt_session_store>(
                resolver.shared<cnetmod::redis::redis_template>(identity_store), idle);
        });
    context.components.singleton<system::login_guard>(
        [login = options->login](application::component_resolver& resolver)
        {
            return std::make_shared<system::login_guard>(
                resolver.shared<cnetmod::redis::redis_template>(identity_store), login);
        });
    context.components.singleton<decoy_password>(
        [](application::component_resolver&)
        {
            // 随机口令的真实哈希：与真实账号走完全相同的算法与迭代次数。
            auto encoded = cnetmod::security::hash_password(
                cnetmod::security::generate_secure_token(32));
            if (!encoded)
                throw std::system_error{encoded.error(), "decoy password hash"};
            return decoy_password{.encoded = std::move(*encoded)};
        });
    return {};
}

auto identity_module::compose(application::composition_context& context)
    -> std::expected<void, std::string>
{
    const auto& components = context.components;
    const auto options = components
                             .get<application::options_monitor<identity_options>>(
                                 identity_section)
                             .current();
    const auto edge = components
                          .get<application::options_monitor<
                              cnetboot::framework::web::edge_options>>(
                              cnetboot::framework::web::edge_section_name)
                          .current();
    auto users = components.shared<application::managed_repository<system::sys_user>>();
    auto authorization = components.shared<system::authorization_service>();
    auto sessions = components.shared<security::jwt_session_store>();

    context.middleware.push_back(security::jwt_security(security::jwt_security_options{
        .settings = &context.configuration.security.jwt,
        .executor = &context.runtime.executor(),
        .sessions = sessions,
        .load_principal = principal_loader_fn{users, authorization},
        .principal_cache_ttl = std::chrono::seconds{options->principal_cache_seconds},
    }));
    context.middleware.push_back(security::authorization_middleware());

    auto authentication = std::make_shared<system::auth_service>(context.runtime,
        users, sessions, components.shared<system::login_guard>(),
        components.get<decoy_password>().encoded);
    system::declare_auth_routes(context.routes, system::auth_route_dependencies{
        .service = std::move(authentication),
        .edge = edge,
    });
    system::declare_user_routes(context.routes,
        user_service_resolver_fn{
            components.get<application::repository_factory<system::sys_user>>(),
            components.shared<system::user_service_factory>()});
    system::declare_authorization_routes(context.routes, std::move(authorization));
    return {};
}

} // namespace cnetboot::bootstrap
