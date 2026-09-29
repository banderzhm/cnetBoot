module cnetboot.framework.error;

import std;

namespace {

/// 唯一注册表：新增错误码必须同时补登记表项并更新 contracts/error_codes.yaml。
constexpr std::array k_registry{
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::ok, "SYSTEM.OK", 200, "成功", "platform", "system", false,
        true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_invalid_argument, "REQUEST.INVALID_ARGUMENT", 400,
        "请求参数不合法", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::category_invalid, "CHAT.CATEGORY_INVALID", 400,
        "会话分类不合法", "chat", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::ai_request_failed, "AI.REQUEST_FAILED", 422,
        "AI 无法处理当前请求", "ai", "dependency", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::upstream_unavailable, "DEPENDENCY.UNAVAILABLE", 503,
        "上游依赖暂不可用，请稍后重试", "platform", "dependency", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_invalid_credentials, "AUTH.INVALID_CREDENTIALS", 401,
        "账号或密码错误", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_missing, "AUTH.TOKEN_MISSING", 401,
        "缺少访问令牌", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_invalid, "AUTH.TOKEN_INVALID", 401,
        "访问令牌无效", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_token_expired, "AUTH.TOKEN_EXPIRED", 401,
        "访问令牌已过期，请重新登录", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_forbidden, "AUTH.FORBIDDEN", 403,
        "没有执行该操作的权限", "identity", "authorization", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_account_locked, "AUTH.ACCOUNT_LOCKED", 423,
        "账号已被临时锁定，请稍后重试", "identity", "authentication", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::auth_account_disabled, "AUTH.ACCOUNT_DISABLED", 403,
        "账号已被停用", "identity", "authentication", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::user_not_found, "USER.NOT_FOUND", 404, "用户不存在",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::user_conflict, "USER.CONFLICT", 409, "用户已存在或状态冲突",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::role_not_found, "ROLE.NOT_FOUND", 404, "角色不存在",
        "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::role_conflict, "ROLE.CONFLICT", 409,
        "角色编码已存在或状态冲突", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::permission_not_found, "PERMISSION.NOT_FOUND", 404,
        "权限不存在", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::setting_not_found, "SETTING.NOT_FOUND", 404,
        "系统设置不存在", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_method_not_allowed,
        "REQUEST.METHOD_NOT_ALLOWED", 405, "该资源不支持此请求方法", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::chat_session_not_found, "CHAT.SESSION_NOT_FOUND", 404,
        "会话不存在", "chat", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::department_not_found, "DEPARTMENT.NOT_FOUND", 404,
        "部门不存在", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::department_conflict, "DEPARTMENT.CONFLICT", 409,
        "部门仍有下级或成员，或层级关系冲突", "identity", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_rate_limited, "REQUEST.RATE_LIMITED", 429,
        "请求过于频繁，请稍后重试", "platform", "request", true, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::request_not_found, "REQUEST.NOT_FOUND", 404,
        "请求的资源路径不存在", "platform", "request", false, true, false},
    cnetboot::framework::error::error_entry{cnetboot::framework::error::error_code::system_internal_error, "SYSTEM.INTERNAL_ERROR", 500,
        "系统内部错误", "platform", "system", false, false, false},
};

constexpr std::array k_prefixes{std::string_view{"SYSTEM"}, std::string_view{"REQUEST"},
    std::string_view{"AI"}, std::string_view{"DEPENDENCY"}, std::string_view{"AUTH"},
    std::string_view{"USER"}, std::string_view{"ROLE"}, std::string_view{"PERMISSION"}, std::string_view{"SETTING"},
    std::string_view{"CHAT"}, std::string_view{"DEPARTMENT"}};

constexpr std::array k_http_statuses{200, 400, 401, 403, 404, 405, 409, 422, 423, 429, 500, 502, 503, 504};

/// @brief 注册码必须形如 <MODULE>.<MEANING>，两段均为大写字母、数字或下划线。
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
    // 1. 注册码唯一、格式合法、前缀已登记、HTTP 状态与必填字段合法。
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

    // 2. 每个枚举值必须恰有一条登记：未登记的错误码禁止对外使用。
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
