module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_role_dept;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Custom department assignment owned by one role.
 */
struct sys_role_dept
{
    std::int64_t role_id{};
    std::int64_t dept_id{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_role_dept, "sys_role_dept",
    CNETMOD_FIELD(role_id, "role_id", bigint),
    CNETMOD_FIELD(dept_id, "dept_id", bigint))
