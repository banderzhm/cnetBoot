export module cnetnexus.system.dto.authorization;

import std;
import cnetmod.json;

export namespace cnetnexus::system {

/**
 * @brief Role representation exposed to authenticated clients.
 */
struct role_dto
{
    std::int64_t id{};
    std::string name;
    std::string key;
};

/**
 * @brief Menu node returned as a hierarchical route tree.
 */
struct menu_dto
{
    std::int64_t id{};
    std::int64_t parent_id{};
    std::string name;
    std::string path;
    std::string component;
    std::string permission;
    std::string type;
    std::string icon;
    std::int32_t order{};
    std::vector<menu_dto> children;
};

/**
 * @brief Complete authorization snapshot for one user.
 */
struct authorization_dto
{
    std::int64_t user_id{};
    std::vector<role_dto> roles;
    std::vector<std::string> permissions;
    std::vector<menu_dto> menus;
};

[[nodiscard]] auto to_document(const role_dto& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const menu_dto& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const authorization_dto& value)
    -> cnetmod::json::document;

} // namespace cnetnexus::system
