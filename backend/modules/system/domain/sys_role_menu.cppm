module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_role_menu;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Assignment between a role and an accessible menu or permission.
 */
struct sys_role_menu
{
    std::int64_t role_id{};
    std::int64_t menu_id{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_role_menu, "sys_role_menu",
    CNETMOD_FIELD(role_id, "role_id", bigint),
    CNETMOD_FIELD(menu_id, "menu_id", bigint))
