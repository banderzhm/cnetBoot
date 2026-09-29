module cnetboot.system.service.auth_service;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetmod.security.jwt;
import cnetmod.security.password;
import cnetboot.framework.error;
import cnetboot.framework.security.jwt_security;
import cnetboot.system.domain.sys_user;
import cnetboot.system.dto.auth;
import cnetboot.system.service.login_guard;

namespace cnetboot::system {

using error_code = cnetboot::framework::error::error_code;

auth_service::auth_service(cnetmod::application::application_runtime& runtime,
    std::shared_ptr<cnetmod::application::managed_repository<sys_user>> users,
    std::shared_ptr<cnetboot::security::jwt_session_store> sessions,
    std::shared_ptr<login_guard> guard,
    std::string decoy_password_hash) noexcept
    : runtime_{&runtime}, users_{std::move(users)}, sessions_{std::move(sessions)},
      guard_{std::move(guard)}, decoy_password_hash_{std::move(decoy_password_hash)}
{
}

auto auth_service::issue_token(std::int64_t user_id,
    const std::string& session_id)
    -> cnetmod::task<std::expected<token_response, error_code>>
{
    auto& executor = runtime_->executor();
    const auto& jwt = runtime_->configuration().security.jwt;
    const auto lifetime = std::chrono::seconds{jwt.expires_in_seconds};
    auto token = co_await cnetmod::security::sign_jwt(executor.cpu_pool(),
        executor.event_loop(),
        cnetmod::security::jwt_sign_options{
            .issuer = jwt.issuer,
            .subject = std::to_string(user_id),
            .lifetime = lifetime,
            .custom_claims = {{"sid", session_id}},
        },
        jwt.secret);
    if (!token)
    {
        (void)co_await sessions_->revoke(session_id);
        co_return std::unexpected{error_code::system_internal_error};
    }
    co_return token_response{
        .access_token = std::move(*token),
        .expires_in = lifetime.count(),
    };
}

auto auth_service::login(login_request credentials, std::string client)
    -> cnetmod::task<std::expected<token_response, error_code>>
{
    if (auto admitted = co_await guard_->admit(credentials.username, client);
        !admitted)
        co_return std::unexpected{admitted.error()};

    cnetmod::orm::query_wrapper<sys_user> query;
    query.eq(&sys_user::user_name, credentials.username);
    auto found = co_await users_->get_one(query);
    if (!found.ok())
        co_return std::unexpected{error_code::upstream_unavailable};
    auto user = found.first();

    auto& executor = runtime_->executor();
    const bool known = user && user->deleted == 0;
    const bool valid = co_await cnetmod::security::verify_password(
        executor.cpu_pool(), executor.event_loop(), std::move(credentials.password),
        known ? user->password_hash : decoy_password_hash_);
    if (!known || !valid)
    {
        co_await guard_->record_failure(credentials.username, client);
        co_return std::unexpected{error_code::auth_invalid_credentials};
    }
    if (user->status != 0)
        co_return std::unexpected{error_code::auth_account_disabled};
    co_await guard_->record_success(credentials.username);

    auto session = co_await sessions_->open(cnetboot::security::jwt_session_record{
        .user_id = user->user_id,
        .username = user->user_name,
        .department_id = user->dept_id,
    });
    if (!session)
        co_return std::unexpected{session.error()};
    co_return co_await issue_token(user->user_id, *session);
}

auto auth_service::refresh(std::int64_t user_id, std::string username,
    std::int64_t department_id, std::string session_id)
    -> cnetmod::task<std::expected<token_response, error_code>>
{
    auto rotated = co_await sessions_->rotate(session_id,
        cnetboot::security::jwt_session_record{
            .user_id = user_id,
            .username = std::move(username),
            .department_id = department_id,
        });
    if (!rotated)
        co_return std::unexpected{rotated.error()};
    co_return co_await issue_token(user_id, *rotated);
}

auto auth_service::logout(std::string session_id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    auto revoked = co_await sessions_->revoke(session_id);
    if (!revoked)
        co_return std::unexpected{revoked.error()};
    co_return std::expected<void, error_code>{};
}

} // namespace cnetboot::system
