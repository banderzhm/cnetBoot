module cnetboot.framework.security.request_identity;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.audit;
import cnetboot.framework.security.current_user;

namespace cnetboot::security {

auto current_principal(cnetmod::http::request_context& request) noexcept
    -> const current_user*
{
    return request.scope().find<current_user>();
}

auto audit_actor_from(const cnetmod::http::request_context& request,
    const current_user* principal) -> cnetboot::framework::audit_actor
{
    cnetboot::framework::audit_actor actor;
    actor.ip = request.client_address();
    actor.user_agent = std::string{request.get_header("User-Agent")};
    actor.request_id = std::string{request.trace_id()};
    actor.method = std::string{request.method()};
    actor.path = std::string{request.path()};
    if (principal != nullptr)
    {
        actor.user_id = static_cast<std::uint64_t>(principal->id);
        actor.username = principal->username;
    }
    return actor;
}

} // namespace cnetboot::security
