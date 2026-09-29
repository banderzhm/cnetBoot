module;

#include <cnetmod/orm.hpp>

export module cnetnexus.system.domain.sys_user_role;

import std;
import cnetmod.orm;

export namespace cnetnexus::system {

/**
 * @brief Assignment between a system user and a role.
 */
struct sys_user_role
{
    std::int64_t user_id{};
    std::int64_t role_id{};
};

} // namespace cnetnexus::system

CNETMOD_MODEL(cnetnexus::system::sys_user_role, "sys_user_role",
    CNETMOD_FIELD(user_id, "user_id", bigint),
    CNETMOD_FIELD(role_id, "role_id", bigint))
