export module cnetboot.framework.security.current_user;

import std;

export namespace cnetboot::security {

/**
 * @brief Immutable authenticated identity propagated through one request.
 */
struct current_user
{
    std::int64_t id{};
    std::int64_t department_id{};
    std::string username;
    std::vector<std::string> roles;
    std::vector<std::string> permissions;
};

} // namespace cnetboot::security
