export module cnetboot.framework.security.authorization;

import std;
import cnetmod.protocol.http;

export namespace cnetboot::security {

/**
 * @brief Declares the permission a route requires.
 *
 * Shorthand for endpoint metadata read by authorization_middleware():
 * `routes.get(path, handler, requires_permission("user:list"))`.
 */
[[nodiscard]] auto requires_permission(std::string permission)
    -> cnetmod::http::endpoint_metadata;

/**
 * @brief Route-level authorization over cnetmod's authorize() middleware.
 *
 * Maps the request's current_user (bound by jwt_security) to a cnetmod
 * authorization principal, enforces http::required_permissions declared on
 * the matched route, and answers failures with the application envelope.
 * Must be installed after jwt_security.
 */
[[nodiscard]] auto authorization_middleware() -> cnetmod::http::middleware_fn;

/** Returns whether authentication granted unrestricted ORM data visibility. */
[[nodiscard]] auto has_unrestricted_data_access(
    const cnetmod::http::request_context& request) noexcept -> bool;

/**
 * @brief Allows access to the addressed user or to a privileged caller.
 *
 * The named route parameter is parsed as a positive user identifier. A caller
 * accessing another user must hold the supplied permission. This is a domain
 * rule (resource ownership), so it stays a handler decorator.
 */
[[nodiscard]] auto require_self_or_permission(std::string parameter,
                                              std::string permission,
                                              cnetmod::http::handler_fn handler)
    -> cnetmod::http::handler_fn;

} // namespace cnetboot::security
