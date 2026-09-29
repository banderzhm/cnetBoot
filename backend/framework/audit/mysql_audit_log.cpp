module cnetnexus.framework.audit.mysql;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetnexus.framework.audit;
import cnetnexus.framework.audit.record;
import cnetnexus.framework.error;

namespace cnetnexus::framework {

mysql_audit_log_service::mysql_audit_log_service(
    cnetmod::application::managed_repository<audit_log_record> audit) noexcept
    : audit_{std::move(audit)} {}

auto mysql_audit_log_service::record(const audit_actor &actor,
                                     std::string_view action,
                                     std::string_view resource, int status_code,
                                     std::string detail)
    -> cnetmod::task<std::expected<void, error::error_code>> {
  audit_log_record row{.actor_user_id = actor.user_id,
                       .actor_username = actor.username,
                       .source_ip = actor.ip,
                       .user_agent = actor.user_agent,
                       .request_id = actor.request_id,
                       .request_method = actor.method,
                       .request_path = actor.path,
                       .action_name = std::string{action},
                       .resource_name = std::string{resource},
                       .status_code = status_code,
                       .detail = std::move(detail)};
  const auto saved = co_await audit_.save(row);
  if (!saved.ok())
    co_return std::unexpected{error::error_code::system_internal_error};
  co_return {};
}

} // namespace cnetnexus::framework
