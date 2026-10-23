module cnetboot.framework.security.authorization;

import std;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetmod.protocol.http.middleware.authorization;
import cnetboot.framework.security.current_user;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;

namespace cnetboot::security {
namespace {

    [[nodiscard]] auto permits(const current_user& user,
        std::string_view required) noexcept -> bool
    {
        return std::ranges::any_of(
            user.permissions, [required](const std::string& granted)
            {
                return cnetmod::http::permission_matches(granted, required);
            });
    }

    [[nodiscard]] auto route_user_id(const cnetmod::http::request_context& request,
        std::string_view parameter) noexcept
        -> std::optional<std::int64_t>
    {
        const auto source = request.param(parameter);
        std::int64_t value{};
        const auto parsed =
            std::from_chars(source.data(), source.data() + source.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != source.data() + source.size() ||
            value <= 0)
            return std::nullopt;
        return value;
    }

    using error_code = cnetboot::framework::error::error_code;


    [[nodiscard]] auto code_for(std::string_view failure) noexcept -> error_code
    {
        if (failure == "FORBIDDEN")
            return error_code::auth_forbidden;
        if (failure == "UNAUTHENTICATED")
            return error_code::auth_token_missing;
        return error_code::upstream_unavailable;
    }

} // namespace

auto requires_permission(std::string permission)
    -> cnetmod::http::endpoint_metadata
{
    if (permission.empty())
        throw std::invalid_argument("permission must not be empty");
    return cnetmod::http::endpoint_metadata{cnetmod::http::required_permissions{
        .all_of = {std::move(permission)}}};
}

auto authorization_middleware() -> cnetmod::http::middleware_fn
{
    return cnetmod::http::authorize({
        .authenticate = [](cnetmod::http::request_context& request)
            -> std::expected<cnetmod::http::authorization_principal,
                cnetmod::http::authorization_error>
        {
            const auto* current = request.scope().find<current_user>();
            if (!current)
                return std::unexpected(cnetmod::http::authorization_error{
                    .code = cnetmod::http::authorization_error_code::unauthenticated,
                    .message = "unauthenticated"});
            return cnetmod::http::authorization_principal{
                .subject = std::to_string(current->id),
                .permissions = current->permissions,
            };
        },
        .on_failure = [](cnetmod::http::request_context& request, int,
                          std::string_view code)
        {
            cnetboot::framework::web::respond_error(request, code_for(code));
        },
    });
}

auto has_unrestricted_data_access(
    const cnetmod::http::request_context& request) noexcept -> bool
{
    const auto* scope =
        request.scope().find<cnetmod::orm::data_permission_scope>();
    return scope != nullptr && scope->unrestricted;
}

auto require_self_or_permission(std::string parameter, std::string permission,
    cnetmod::http::handler_fn handler)
    -> cnetmod::http::handler_fn
{
    if (parameter.empty() || permission.empty() || !handler)
        throw std::invalid_argument(
            "route parameter, permission, and handler must not be empty");
    return [parameter = std::move(parameter), permission = std::move(permission),
               handler = std::move(handler)](
               cnetmod::http::request_context& request) -> cnetmod::task<void>
    {
        const auto* current = request.scope().find<current_user>();
        if (!current)
        {
            cnetboot::framework::web::respond_error(request,
                error_code::auth_token_missing);
            co_return;
        }
        const auto target = route_user_id(request, parameter);
        if ((!target || *target != current->id) && !permits(*current, permission))
        {
            cnetboot::framework::web::respond_error(request,
                error_code::auth_forbidden);
            co_return;
        }
        co_await handler(request);
    };
}

} // namespace cnetboot::security
