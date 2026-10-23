module cnetboot.framework.error;

import std;

namespace {

/// Single source of truth. Keep contracts/error_codes.yaml synchronized.
constexpr std::array k_registry{
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::ok, "SYSTEM.OK", 200, "Success", "platform", "system", false,
        true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_invalid_argument, "REQUEST.INVALID_ARGUMENT", 400,
        "Invalid request arguments", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::category_invalid, "CHAT.CATEGORY_INVALID", 400,
        "Invalid chat category", "chat", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::ai_request_failed, "AI.REQUEST_FAILED", 422,
        "AI could not process the request", "ai", "dependency", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::upstream_unavailable, "DEPENDENCY.UNAVAILABLE", 503,
        "An upstream dependency is temporarily unavailable", "platform", "dependency", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_invalid_credentials, "AUTH.INVALID_CREDENTIALS", 401,
        "Invalid username or password", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_missing, "AUTH.TOKEN_MISSING", 401,
        "Access token is required", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_invalid, "AUTH.TOKEN_INVALID", 401,
        "Access token is invalid", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_expired, "AUTH.TOKEN_EXPIRED", 401,
        "Access token has expired", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_forbidden, "AUTH.FORBIDDEN", 403,
        "Permission denied", "identity", "authorization", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_account_locked, "AUTH.ACCOUNT_LOCKED", 423,
        "Account is temporarily locked", "identity", "authentication", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_account_disabled, "AUTH.ACCOUNT_DISABLED", 403,
        "Account is disabled", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::user_not_found, "USER.NOT_FOUND", 404, "User not found",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::user_conflict, "USER.CONFLICT", 409, "User already exists or has a conflicting state",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::role_not_found, "ROLE.NOT_FOUND", 404, "Role not found",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::role_conflict, "ROLE.CONFLICT", 409,
        "Role code already exists or has a conflicting state", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::permission_not_found, "PERMISSION.NOT_FOUND", 404,
        "Permission not found", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::setting_not_found, "SETTING.NOT_FOUND", 404,
        "Setting not found", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_method_not_allowed,
        "REQUEST.METHOD_NOT_ALLOWED", 405, "Method not allowed for this resource", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::chat_session_not_found, "CHAT.SESSION_NOT_FOUND", 404,
        "Chat session not found", "chat", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::department_not_found, "DEPARTMENT.NOT_FOUND", 404,
        "Department not found", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::department_conflict, "DEPARTMENT.CONFLICT", 409,
        "Department has children or members, or its hierarchy conflicts", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_rate_limited, "REQUEST.RATE_LIMITED", 429,
        "Too many requests", "platform", "request", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_not_found, "REQUEST.NOT_FOUND", 404,
        "Resource not found", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::system_internal_error, "SYSTEM.INTERNAL_ERROR", 500,
        "Internal server error", "platform", "system", false, false, false},
};

constexpr std::array k_prefixes{std::string_view{"SYSTEM"}, std::string_view{"REQUEST"},
    std::string_view{"AI"}, std::string_view{"DEPENDENCY"}, std::string_view{"AUTH"},
    std::string_view{"USER"}, std::string_view{"ROLE"}, std::string_view{"PERMISSION"}, std::string_view{"SETTING"},
    std::string_view{"CHAT"}, std::string_view{"DEPARTMENT"}};

constexpr std::array k_http_statuses{200, 400, 401, 403, 404, 405, 409, 422, 423, 429, 500, 502, 503, 504};

/// Registration names use <MODULE>.<MEANING> with uppercase letters, digits, or underscores.
auto is_valid_name(std::string_view name) noexcept -> bool
{
    const auto separator = name.find('.');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 >= name.size())
        return false;
    if (name.find('.', separator + 1) != std::string_view::npos)
        return false;
    const auto is_symbol = [](char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    };
    if (!std::ranges::all_of(name.substr(0, separator), is_symbol))
        return false;
    return std::ranges::all_of(name.substr(separator + 1), is_symbol);
}

} // namespace

namespace cnetboot::framework::error {

auto registry() noexcept -> std::span<const error_entry>
{
    return k_registry;
}

auto find(std::string_view name) noexcept -> std::optional<error_entry>
{
    const auto it = std::ranges::find_if(k_registry, [name](const error_entry& entry)
        {
            return entry.name == name;
        });
    return it == k_registry.end() ? std::nullopt : std::optional{*it};
}

auto find(error_code code) noexcept -> std::optional<error_entry>
{
    const auto it = std::ranges::find_if(k_registry, [code](const error_entry& entry)
        {
            return entry.code == code;
        });
    return it == k_registry.end() ? std::nullopt : std::optional{*it};
}

auto validate() noexcept -> std::expected<void, std::error_code>
{
    // Registration names and enum values are unique and structurally valid.
    for (std::size_t i = 0; i < k_registry.size(); ++i)
    {
        const auto& entry = k_registry[i];
        if (!is_valid_name(entry.name) || entry.owner.empty() || entry.category.empty())
            return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
        if (std::ranges::find(k_http_statuses, entry.http_status) == k_http_statuses.end())
            return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
        const auto prefix = entry.name.substr(0, entry.name.find('.'));
        if (std::ranges::find(k_prefixes, prefix) == k_prefixes.end())
            return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
        for (std::size_t j = i + 1; j < k_registry.size(); ++j)
        {
            if (k_registry[j].name == entry.name || k_registry[j].code == entry.code)
                return std::unexpected{std::make_error_code(std::errc::file_exists)};
        }
    }

    // Every enum value must have exactly one public registry entry.
    constexpr auto last = static_cast<std::uint16_t>(error_code::system_internal_error);
    for (std::uint16_t raw = 0; raw <= last; ++raw)
    {
        const auto code = static_cast<error_code>(raw);
        if (std::ranges::find_if(k_registry, [code](const error_entry& entry)
                {
                    return entry.code == code;
                }) == k_registry.end())
            return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    }
    return {};
}

} // namespace cnetboot::framework::error
