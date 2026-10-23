module cnetboot.system.controller.user_controller;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.query;
import cnetboot.framework.web.response;
import cnetboot.framework.security.authorization;
import cnetboot.framework.web.route_handler;
import cnetboot.system.dto.user;
import cnetboot.system.service.user_service;

namespace cnetboot::system {


struct user_route_dependencies
{
    user_service_resolver resolve_service;
};


[[nodiscard]] auto handle_list_users(cnetmod::http::request_context& request,
    const user_route_dependencies& dependencies) -> cnetmod::task<void>
{
    const auto query = request.query_string();
    auto page = cnetboot::framework::pagination::page_request{
        .page = cnetboot::framework::web::query::integer(query, "page")
                    .value_or(1),
        .page_size = cnetboot::framework::web::query::integer(query, "pageSize")
                         .value_or(cnetboot::framework::pagination::default_page_size)};
    auto service = dependencies.resolve_service(request);
    if (!service)
    {

        logger::error{"user service unavailable"};
        cnetboot::framework::web::respond_error(request,
            cnetboot::framework::error::error_code::upstream_unavailable);
        co_return;
    }
    auto result = co_await (*service)->page(page,
        cnetmod::http::url_decode(
            cnetmod::http::parse_query_param(query, "userName")));
    if (!result)
    {
        cnetboot::framework::web::respond_error(request, result.error());
        co_return;
    }
    cnetboot::framework::web::respond_document(request, to_document(*result));
    co_return;
}

void declare_user_routes(cnetmod::http::router& routes,
    user_service_resolver resolve_service)
{
    if (!resolve_service)
        throw std::invalid_argument{"user routes require a service resolver"};
    routes.get("/api/system/users",
        cnetboot::framework::web::route_handler<user_route_dependencies>{
                user_route_dependencies{std::move(resolve_service)},
                &handle_list_users},
        cnetboot::security::requires_permission("system:user:list"));
}

} // namespace cnetboot::system
