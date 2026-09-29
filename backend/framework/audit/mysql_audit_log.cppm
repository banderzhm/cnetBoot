/** @brief MySQL-backed append-only audit sink. */
export module cnetnexus.framework.audit.mysql;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetnexus.framework.audit;
import cnetnexus.framework.audit.record;
import cnetnexus.framework.error;

export namespace cnetnexus::framework {

class mysql_audit_log_service final : public audit_log_service {
public:
  explicit mysql_audit_log_service(
      cnetmod::application::managed_repository<audit_log_record>
          audit) noexcept;

  [[nodiscard]] auto record(const audit_actor &actor, std::string_view action,
                            std::string_view resource, int status_code,
                            std::string detail)
      -> cnetmod::task<std::expected<void, error::error_code>> override;

private:
  cnetmod::application::managed_repository<audit_log_record> audit_;
};

} // namespace cnetnexus::framework
