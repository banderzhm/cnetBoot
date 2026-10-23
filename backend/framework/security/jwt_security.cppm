module;

#include <cnetmod/json.hpp>

export module cnetboot.framework.security.jwt_security;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetmod.protocol.redis;
import cnetmod.orm;
import cnetboot.framework.error;
import cnetboot.framework.security.current_user;

export namespace cnetboot::security {

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


struct jwt_session_id
{
    std::string value;
};


using principal_loader = std::function<cnetmod::task<
    std::expected<authenticated_context, cnetboot::framework::error::error_code>>(
    std::int64_t)>;

[[nodiscard]] auto jwt_session_key(std::string_view session_id)
    -> std::string;


class jwt_session_store
{
public:
    jwt_session_store(std::shared_ptr<cnetmod::redis::redis_template> redis,
        std::chrono::seconds idle_timeout);


    [[nodiscard]] auto open(const jwt_session_record& record)
        -> cnetmod::task<std::expected<std::string, cnetboot::framework::error::error_code>>;


    [[nodiscard]] auto touch(std::string_view session_id)
        -> cnetmod::task<std::expected<std::optional<jwt_session_record>,
            cnetboot::framework::error::error_code>>;


    [[nodiscard]] auto rotate(std::string_view session_id, const jwt_session_record& record)
        -> cnetmod::task<std::expected<std::string, cnetboot::framework::error::error_code>>;


    [[nodiscard]] auto revoke(std::string_view session_id)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;

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

} // namespace cnetboot::security

CNETMOD_JSON(cnetboot::security::jwt_session_record,
    CNETMOD_JSON_FIELD(user_id),
    CNETMOD_JSON_FIELD(username),
    CNETMOD_JSON_FIELD(department_id))
