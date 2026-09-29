module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_user_role;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Assignment between a system user and a role.
 */
struct sys_user_role
{
    std::int64_t user_id{};
    std::int64_t role_id{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_user_role, "sys_user_role",
    CNETMOD_FIELD(user_id, "user_id", bigint),
    CNETMOD_FIELD(role_id, "role_id", bigint))
