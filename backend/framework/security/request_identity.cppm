/**
 * @brief HTTP request identity helpers shared by authenticated business modules.
 */
export module cnetboot.framework.security.request_identity;

import cnetmod.protocol.http;
import cnetboot.framework.audit;
import cnetboot.framework.security.current_user;

export namespace cnetboot::security {

[[nodiscard]] auto current_principal(
    cnetmod::http::request_context& request) noexcept -> const current_user*;

[[nodiscard]] auto audit_actor_from(const cnetmod::http::request_context& request,
    const current_user* principal) -> cnetboot::framework::audit_actor;

} // namespace cnetboot::security
