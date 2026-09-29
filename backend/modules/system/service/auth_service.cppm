export module cnetboot.system.service.auth_service;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetboot.framework.error;
import cnetboot.framework.security.jwt_security;
import cnetboot.system.domain.sys_user;
import cnetboot.system.dto.auth;
import cnetboot.system.service.login_guard;

export namespace cnetboot::system {

/** Authentication use cases; controllers only adapt HTTP to this service. */
class auth_service
{
public:
    auth_service(cnetmod::application::application_runtime& runtime,
        std::shared_ptr<cnetmod::application::managed_repository<sys_user>> users,
        std::shared_ptr<cnetboot::security::jwt_session_store> sessions,
        std::shared_ptr<login_guard> guard,
        std::string decoy_password_hash) noexcept;

    [[nodiscard]] auto login(login_request credentials, std::string client)
        -> cnetmod::task<std::expected<token_response,
            cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto refresh(std::int64_t user_id, std::string username,
        std::int64_t department_id, std::string session_id)
        -> cnetmod::task<std::expected<token_response,
            cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto logout(std::string session_id)
        -> cnetmod::task<std::expected<void,
            cnetboot::framework::error::error_code>>;

private:
    [[nodiscard]] auto issue_token(std::int64_t user_id,
        const std::string& session_id)
        -> cnetmod::task<std::expected<token_response,
            cnetboot::framework::error::error_code>>;

    cnetmod::application::application_runtime* runtime_;
    std::shared_ptr<cnetmod::application::managed_repository<sys_user>> users_;
    std::shared_ptr<cnetboot::security::jwt_session_store> sessions_;
    std::shared_ptr<login_guard> guard_;
    std::string decoy_password_hash_;
};

} // namespace cnetboot::system
