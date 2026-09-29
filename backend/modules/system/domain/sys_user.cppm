module;

#include <cnetmod/orm.hpp>

export module cnetboot.system.domain.sys_user;

import std;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Persistent system user managed by cnetmod ORM policies.
 */
struct sys_user {
    std::int64_t user_id{};
    std::int64_t dept_id{};
  std::string user_name;
  std::string password_hash;
  std::string nick_name;
  std::string email;
  int status{};
  int deleted{};
  std::int64_t version{};
  cnetmod::orm::calendar_datetime created_at{};
  cnetmod::orm::calendar_datetime updated_at{};
};

} // namespace cnetboot::system

CNETMOD_MODEL(cnetboot::system::sys_user, "sys_user",
    CNETMOD_FIELD(user_id, "user_id", bigint, PK | AUTO_INC | DATA_OWNER),
    CNETMOD_FIELD(dept_id, "dept_id", bigint, DATA_PARTITION),
              CNETMOD_FIELD(user_name, "user_name", varchar),
              CNETMOD_FIELD(password_hash, "password_hash", varchar),
              CNETMOD_FIELD(nick_name, "nick_name", varchar),
              CNETMOD_FIELD(email, "email", varchar),
              CNETMOD_FIELD(status, "status", tinyint),
              CNETMOD_FIELD(deleted, "deleted", tinyint, LOGIC_DELETE),
              CNETMOD_FIELD(version, "version", bigint, VERSION),
              CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT),
              CNETMOD_FIELD(updated_at, "updated_at", datetime,
                            FILL_INSERT_UPDATE))
