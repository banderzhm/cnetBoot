module;

#include <cnetmod/json.hpp>

export module cnetboot.system.dto.authorization;

import std;
import cnetmod.json;

export namespace cnetboot::system {

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

} // namespace cnetboot::system

CNETMOD_JSON(cnetboot::system::role_dto,
    CNETMOD_JSON_FIELD(id),
    CNETMOD_JSON_FIELD(name),
    CNETMOD_JSON_FIELD(key))

CNETMOD_JSON(cnetboot::system::menu_dto,
    CNETMOD_JSON_FIELD(id),
    ::cnetmod::json::field("parentId", &_cnetmod_json_type::parent_id),
    CNETMOD_JSON_FIELD(name),
    CNETMOD_JSON_FIELD(path),
    CNETMOD_JSON_FIELD(component),
    CNETMOD_JSON_FIELD(permission),
    CNETMOD_JSON_FIELD(type),
    CNETMOD_JSON_FIELD(icon),
    CNETMOD_JSON_FIELD(order),
    CNETMOD_JSON_FIELD(children))

CNETMOD_JSON(cnetboot::system::authorization_dto,
    ::cnetmod::json::field("userId", &_cnetmod_json_type::user_id),
    CNETMOD_JSON_FIELD(roles),
    CNETMOD_JSON_FIELD(permissions),
    CNETMOD_JSON_FIELD(menus))
