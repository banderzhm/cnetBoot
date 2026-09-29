module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_dept;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Department node used by organization and row-level data scopes.
 */
struct sys_dept
{
    std::int64_t dept_id{};
    std::int64_t parent_id{};
    std::string dept_name;
    int order_num{};
    std::string leader;
    std::string email;
    std::string phone;
    int status{};
    int deleted{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_dept, "sys_dept",
    CNETMOD_FIELD(dept_id, "dept_id", bigint, PK | AUTO_INC),
    CNETMOD_FIELD(parent_id, "parent_id", bigint),
    CNETMOD_FIELD(dept_name, "dept_name", varchar),
    CNETMOD_FIELD(order_num, "order_num", int_),
    CNETMOD_FIELD(leader, "leader", varchar),
    CNETMOD_FIELD(email, "email", varchar),
    CNETMOD_FIELD(phone, "phone", varchar),
    CNETMOD_FIELD(status, "status", tinyint),
    CNETMOD_FIELD(deleted, "deleted", tinyint, LOGIC_DELETE))
