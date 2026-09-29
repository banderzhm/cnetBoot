module;

#include <cnetmod/orm.hpp>

export module cnetnexus.framework.audit.record;

import std;
import cnetmod.orm;

export namespace cnetnexus::framework {

struct audit_log_record {
  std::uint64_t id{};
  std::uint64_t actor_user_id{};
  std::string actor_username;
  std::string source_ip;
  std::string user_agent;
  std::string request_id;
  std::string request_method;
  std::string request_path;
  std::string action_name;
  std::string resource_name;
  int status_code{};
  std::string detail;
  cnetmod::orm::calendar_datetime created_at{};
};

} // namespace cnetnexus::framework

CNETMOD_MODEL(cnetnexus::framework::audit_log_record, "audit_log",
              CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
              CNETMOD_FIELD(actor_user_id, "actor_user_id", bigint),
              CNETMOD_FIELD(actor_username, "actor_username", varchar),
              CNETMOD_FIELD(source_ip, "source_ip", varchar),
              CNETMOD_FIELD(user_agent, "user_agent", varchar),
              CNETMOD_FIELD(request_id, "request_id", varchar),
              CNETMOD_FIELD(request_method, "request_method", varchar),
              CNETMOD_FIELD(request_path, "request_path", varchar),
              CNETMOD_FIELD(action_name, "action_name", varchar),
              CNETMOD_FIELD(resource_name, "resource_name", varchar),
              CNETMOD_FIELD(status_code, "status_code", int_),
              CNETMOD_FIELD(detail, "detail", text),
              CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT))
