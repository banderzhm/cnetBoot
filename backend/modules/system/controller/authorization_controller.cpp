module cnetboot.system.controller.authorization_controller;

import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.response;
import cnetboot.framework.security.authorization;
import cnetboot.framework.web.route_handler;
import cnetboot.system.dto.authorization;

namespace cnetboot::system {

/** @brief 授权快照路由的依赖：共享的授权服务。 */
struct authorization_route_dependencies
{
    std::shared_ptr<authorization_service> service;
};

namespace {

[[nodiscard]] auto user_id(cnetmod::http::request_context& request)
    -> std::optional<std::int64_t>
{
    const auto source = request.param("id");
    std::int64_t value{};
    const auto result =
        std::from_chars(source.data(), source.data() + source.size(), value);
    if (result.ec != std::errc{} ||
        result.ptr != source.data() + source.size() || value <= 0)
        return std::nullopt;
    return value;
}

} // namespace

/** @brief GET /api/system/users/:id/authorization：返回用户权限快照。 */
[[nodiscard]] auto handle_authorization(
    cnetmod::http::request_context& request,
    const authorization_route_dependencies& dependencies) -> cnetmod::task<void>
{
    const auto id = user_id(request);
    if (!id)
    {
        cnetboot::framework::web::respond_error(request,
            cnetboot::framework::error::error_code::request_invalid_argument);
        co_return;
    }
    auto result = co_await dependencies.service->authorization(*id);
    if (!result)
    {
        cnetboot::framework::web::respond_error(request, result.error());
        co_return;
    }
    cnetboot::framework::web::respond_document(request, to_document(*result));
    co_return;
}

void declare_authorization_routes(
    cnetmod::http::router& routes,
    std::shared_ptr<authorization_service> service)
{
    routes.get("/api/system/users/:id/authorization",
        security::require_self_or_permission("id", "system:user:query",
            cnetboot::framework::web::route_handler<
                authorization_route_dependencies>{
                authorization_route_dependencies{std::move(service)},
                &handle_authorization}));
}

} // namespace cnetboot::system
