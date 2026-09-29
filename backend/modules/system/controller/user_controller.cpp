module cnetnexus.system.controller.user_controller;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.response;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.web.route_handler;
import cnetnexus.system.dto.user;
import cnetnexus.system.service.user_service;

namespace cnetnexus::system {

/** @brief 系统用户路由只依赖应用服务，不接触仓储或 Mapper。 */
struct user_route_dependencies
{
    user_service_resolver resolve_service;
};

/** @brief GET /api/system/users：分页列出系统用户。 */
[[nodiscard]] auto handle_list_users(cnetmod::http::request_context& request,
    const user_route_dependencies& dependencies) -> cnetmod::task<void>
{
    const auto query = request.query_string();
    auto page = cnetnexus::framework::pagination::page_request{
        .page = cnetnexus::framework::web::query::integer(query, "page")
                    .value_or(1),
        .page_size = cnetnexus::framework::web::query::integer(query, "pageSize")
                         .value_or(cnetnexus::framework::pagination::default_page_size)};
    auto service = dependencies.resolve_service(request);
    if (!service)
    {
        // 严格 SaaS 模式下缺少租户作用域等：只记日志，对外给注册码。
        logger::error{"user service unavailable"};
        cnetnexus::framework::web::respond_error(request,
            cnetnexus::framework::error::error_code::upstream_unavailable);
        co_return;
    }
    auto result = co_await (*service)->page(page,
        cnetmod::http::url_decode(
            cnetmod::http::parse_query_param(query, "userName")));
    if (!result)
    {
        cnetnexus::framework::web::respond_error(request, result.error());
        co_return;
    }
    cnetnexus::framework::web::respond_document(request, to_document(*result));
    co_return;
}

void declare_user_routes(cnetmod::http::router& routes,
    user_service_resolver resolve_service)
{
    if (!resolve_service)
        throw std::invalid_argument{"user routes require a service resolver"};
    routes.get("/api/system/users",
        cnetnexus::framework::web::route_handler<user_route_dependencies>{
                user_route_dependencies{std::move(resolve_service)},
                &handle_list_users},
        cnetnexus::security::requires_permission("system:user:list"));
}

} // namespace cnetnexus::system
