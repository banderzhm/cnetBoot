module cnetnexus.system.dto.authorization;

import std;
import cnetmod.json;

namespace cnetnexus::system {

auto to_document(const role_dto& value) -> cnetmod::json::document
{
    return cnetmod::json::object(
        {{"id", value.id}, {"name", value.name}, {"key", value.key}});
}

auto to_document(const menu_dto& value) -> cnetmod::json::document
{
    auto children = cnetmod::json::array();
    children.get_array().reserve(value.children.size());
    for (const auto& child : value.children)
        children.get_array().push_back(to_document(child));
    return cnetmod::json::object({{"id", value.id},
        {"parentId", value.parent_id}, {"name", value.name},
        {"path", value.path}, {"component", value.component},
        {"permission", value.permission}, {"type", value.type},
        {"icon", value.icon}, {"order", value.order},
        {"children", std::move(children)}});
}

auto to_document(const authorization_dto& value) -> cnetmod::json::document
{
    auto roles = cnetmod::json::array();
    roles.get_array().reserve(value.roles.size());
    for (const auto& role : value.roles)
        roles.get_array().push_back(to_document(role));

    auto permissions = cnetmod::json::array();
    permissions.get_array().reserve(value.permissions.size());
    for (const auto& permission : value.permissions)
        permissions.get_array().emplace_back(permission);

    auto menus = cnetmod::json::array();
    menus.get_array().reserve(value.menus.size());
    for (const auto& menu : value.menus)
        menus.get_array().push_back(to_document(menu));

    return cnetmod::json::object({{"userId", value.user_id},
        {"roles", std::move(roles)},
        {"permissions", std::move(permissions)},
        {"menus", std::move(menus)}});
}

} // namespace cnetnexus::system
