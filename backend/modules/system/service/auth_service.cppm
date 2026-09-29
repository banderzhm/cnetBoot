export module cnetnexus.system.service.auth_service;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.framework.security.jwt_security;
import cnetnexus.system.domain.sys_user;
import cnetnexus.system.dto.auth;
import cnetnexus.system.service.login_guard;

export namespace cnetnexus::system {

/** Authentication use cases; controllers only adapt HTTP to this service. */
class auth_service
{
public:
    auth_service(cnetmod::application::application_runtime& runtime,
        std::shared_ptr<cnetmod::application::managed_repository<sys_user>> users,
        std::shared_ptr<cnetnexus::security::jwt_session_store> sessions,
        std::shared_ptr<login_guard> guard,
        std::string decoy_password_hash) noexcept;

    [[nodiscard]] auto login(login_request credentials, std::string client)
        -> cnetmod::task<std::expected<token_response,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto refresh(std::int64_t user_id, std::string username,
        std::int64_t department_id, std::string session_id)
        -> cnetmod::task<std::expected<token_response,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto logout(std::string session_id)
        -> cnetmod::task<std::expected<void,
            cnetnexus::framework::error::error_code>>;

private:
    [[nodiscard]] auto issue_token(std::int64_t user_id,
        const std::string& session_id)
        -> cnetmod::task<std::expected<token_response,
            cnetnexus::framework::error::error_code>>;

    cnetmod::application::application_runtime* runtime_;
    std::shared_ptr<cnetmod::application::managed_repository<sys_user>> users_;
    std::shared_ptr<cnetnexus::security::jwt_session_store> sessions_;
    std::shared_ptr<login_guard> guard_;
    std::string decoy_password_hash_;
};

} // namespace cnetnexus::system
