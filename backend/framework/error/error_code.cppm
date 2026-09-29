/**
 * @brief Registered error codes shared by every cnetBoot module.
 */
export module cnetboot.framework.error;

import std;

export namespace cnetboot::framework::error {

/** @brief Registered error code; every value owns exactly one registry entry. */
enum class error_code : std::uint16_t
{
    ok = 0,                     ///< SYSTEM.OK
    request_invalid_argument,   ///< REQUEST.INVALID_ARGUMENT
    category_invalid,           ///< CHAT.CATEGORY_INVALID
    ai_request_failed,          ///< AI.REQUEST_FAILED
    upstream_unavailable,       ///< DEPENDENCY.UNAVAILABLE
    auth_invalid_credentials,   ///< AUTH.INVALID_CREDENTIALS
    auth_token_missing,         ///< AUTH.TOKEN_MISSING
    auth_token_invalid,         ///< AUTH.TOKEN_INVALID
    auth_token_expired,         ///< AUTH.TOKEN_EXPIRED
    auth_forbidden,             ///< AUTH.FORBIDDEN
    auth_account_locked,        ///< AUTH.ACCOUNT_LOCKED
    auth_account_disabled,      ///< AUTH.ACCOUNT_DISABLED
    user_not_found,             ///< USER.NOT_FOUND
    user_conflict,              ///< USER.CONFLICT
    role_not_found,             ///< ROLE.NOT_FOUND
    role_conflict,              ///< ROLE.CONFLICT
    permission_not_found,       ///< PERMISSION.NOT_FOUND
    setting_not_found,          ///< SETTING.NOT_FOUND
    request_method_not_allowed, ///< REQUEST.METHOD_NOT_ALLOWED
    chat_session_not_found,     ///< CHAT.SESSION_NOT_FOUND
    department_not_found,       ///< DEPARTMENT.NOT_FOUND
    department_conflict,        ///< DEPARTMENT.CONFLICT
    request_rate_limited,       ///< REQUEST.RATE_LIMITED
    request_not_found,          ///< REQUEST.NOT_FOUND
    system_internal_error,      ///< SYSTEM.INTERNAL_ERROR
};

/** @brief Registry metadata for one error code. */
struct error_entry
{
    error_code code;                  ///< enum value
    std::string_view name;            ///< registered <MODULE>.<MEANING> code
    int http_status;                  ///< default HTTP status
    std::string_view default_message; ///< safe default message
    std::string_view owner;           ///< owning module
    std::string_view category;        ///< request / authentication / dependency / system
    bool retryable;                   ///< caller may retry
    bool exposable;                   ///< default_message may be shown verbatim
    bool deprecated;                  ///< no longer issued
};

/** @brief Read-only view over the registry covering every error_code value. */
[[nodiscard]] auto registry() noexcept -> std::span<const error_entry>;

/** @brief Looks up a registry entry by its registered name. */
[[nodiscard]] auto find(std::string_view name) noexcept -> std::optional<error_entry>;

/** @brief Looks up a registry entry by its enum value. */
[[nodiscard]] auto find(error_code code) noexcept -> std::optional<error_entry>;

/** @brief Validates enum coverage, name uniqueness/format, registered prefixes,
 *  HTTP status membership and mandatory fields. */
[[nodiscard]] auto validate() noexcept -> std::expected<void, std::error_code>;

} // namespace cnetboot::framework::error
