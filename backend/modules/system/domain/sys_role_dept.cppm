module;

#include <cnetmod/orm.hpp>

export module cnetnexus.system.domain.sys_role_dept;

import std;
import cnetmod.orm;

export namespace cnetnexus::system {

/**
 * @brief Custom department assignment owned by one role.
 */
struct sys_role_dept
{
    std::int64_t role_id{};
    std::int64_t dept_id{};
};

} // namespace cnetnexus::system

CNETMOD_MODEL(cnetnexus::system::sys_role_dept, "sys_role_dept",
    CNETMOD_FIELD(role_id, "role_id", bigint),
    CNETMOD_FIELD(dept_id, "dept_id", bigint))
