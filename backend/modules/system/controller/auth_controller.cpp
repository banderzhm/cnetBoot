module cnetboot.system.controller.auth_controller;

import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.security.current_user;
import cnetboot.framework.security.jwt_security;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.rate_limit;
import cnetboot.framework.web.response;
import cnetboot.framework.web.route_handler;
import cnetboot.system.dto.auth;
import cnetboot.system.service.auth_service;

namespace cnetboot::system {
namespace {

using error_code = cnetboot::framework::error::error_code;
namespace web = cnetboot::framework::web;

constexpr std::size_t max_login_body{4096};
constexpr std::size_t max_username_length{64};

[[nodiscard]] auto handle_login(cnetmod::http::request_context& request,
    const auth_route_dependencies& dependencies) -> cnetmod::task<void>
{
    if (request.body().size() > max_login_body)
    {
        web::respond_error(request, error_code::request_invalid_argument);
        co_return;
    }
    auto credentials = cnetmod::json::parse<login_request>(request.body());
    if (!credentials || credentials->username.empty() ||
        credentials->username.size() > max_username_length ||
        credentials->password.empty())
    {
        web::respond_error(request, error_code::request_invalid_argument);
        co_return;
    }

    auto result = co_await dependencies.service->login(
        std::move(*credentials), web::client_ip(request, *dependencies.edge));
    if (!result)
    {
        web::respond_error(request, result.error());
        co_return;
    }
    web::respond_document(request, to_document(*result));
}

[[nodiscard]] auto handle_refresh(cnetmod::http::request_context& request,
    const auth_route_dependencies& dependencies) -> cnetmod::task<void>
{
    const auto* user = request.scope().find<cnetboot::security::current_user>();
    const auto* session = request.scope().find<cnetboot::security::jwt_session_id>();
    if (!user || !session)
    {
        web::respond_error(request, error_code::auth_token_missing);
        co_return;
    }
    auto result = co_await dependencies.service->refresh(user->id, user->username,
        user->department_id, session->value);
    if (!result)
    {
        web::respond_error(request, result.error());
        co_return;
    }
    web::respond_document(request, to_document(*result));
}

[[nodiscard]] auto handle_logout(cnetmod::http::request_context& request,
    const auth_route_dependencies& dependencies) -> cnetmod::task<void>
{
    const auto* session = request.scope().find<cnetboot::security::jwt_session_id>();
    if (!session)
    {
        web::respond_error(request, error_code::auth_token_missing);
        co_return;
    }
    if (auto result = co_await dependencies.service->logout(session->value); !result)
    {
        web::respond_error(request, result.error());
        co_return;
    }
    web::respond_document(request, revoked_document());
}

} // namespace

namespace detail {

void declare_auth_handlers(cnetmod::http::router& routes,
    auth_route_dependencies dependencies)
{
    using handler = cnetboot::framework::web::route_handler<auth_route_dependencies>;
    routes.post("/api/auth/login", handler{dependencies, &handle_login},
        web::with_rate_limit(web::anonymous(), web::rate_limit_policy::login));
    routes.post("/api/auth/refresh", handler{dependencies, &handle_refresh});
    routes.post("/api/auth/logout", handler{std::move(dependencies), &handle_logout});
}

} // namespace detail

void declare_auth_routes(cnetmod::http::router& routes,
    auth_route_dependencies dependencies)
{
    if (!dependencies.service || !dependencies.edge)
        throw std::invalid_argument{"auth routes require service and edge options"};
    detail::declare_auth_handlers(routes, std::move(dependencies));
}

} // namespace cnetboot::system
