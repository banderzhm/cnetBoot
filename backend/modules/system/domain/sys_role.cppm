module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_role;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

enum class role_data_scope : int
{
    all = 1,
    custom = 2,
    department = 3,
    department_tree = 4,
    self = 5,
};

/**
 * @brief Role assigned to one or more administration users.
 */
struct sys_role
{
    std::int64_t role_id{};
    std::string role_name;
    std::string role_key;
    int data_scope{static_cast<int>(role_data_scope::self)};
    std::int32_t role_sort{};
    int status{};
    int deleted{};
    std::int64_t version{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_role, "sys_role",
    CNETMOD_FIELD(role_id, "role_id", bigint, PK | AUTO_INC),
    CNETMOD_FIELD(role_name, "role_name", varchar),
    CNETMOD_FIELD(role_key, "role_key", varchar),
    CNETMOD_FIELD(data_scope, "data_scope", int_),
    CNETMOD_FIELD(role_sort, "role_sort", int_),
    CNETMOD_FIELD(status, "status", tinyint),
    CNETMOD_FIELD(deleted, "deleted", tinyint, LOGIC_DELETE),
    CNETMOD_FIELD(version, "version", bigint, VERSION))
