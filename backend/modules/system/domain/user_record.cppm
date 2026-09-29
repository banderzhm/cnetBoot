module;

#include <cnetmod/orm.hpp>

export module cnetnexus.system.domain.user_record;

import std;
import cnetmod.orm;

export namespace cnetnexus::system {

/** Read-only projection returned by the joined user list statement. */
struct user_list_row
{
    std::int64_t user_id{};
    std::int64_t dept_id{};
    std::string user_name;
    std::string nick_name;
    std::string email;
    int status{};
    std::string dept_name;
};

/** Read-only projection returned by the user count statement. */
struct user_count_row
{
    std::int64_t total{};
};

} // namespace cnetnexus::system

CNETMOD_PROJECTION(cnetnexus::system::user_list_row,
    CNETMOD_FIELD(user_id, "user_id", bigint),
    CNETMOD_FIELD(dept_id, "dept_id", bigint),
    CNETMOD_FIELD(user_name, "user_name", varchar),
    CNETMOD_FIELD(nick_name, "nick_name", varchar),
    CNETMOD_FIELD(email, "email", varchar),
    CNETMOD_FIELD(status, "status", int_),
    CNETMOD_FIELD(dept_name, "dept_name", varchar))

CNETMOD_PROJECTION(cnetnexus::system::user_count_row,
    CNETMOD_FIELD(total, "total", bigint))
