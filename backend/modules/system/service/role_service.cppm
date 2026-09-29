export module cnetboot.system.service.role_service;

import std;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetmod.application;
import cnetmod.coro.task;
import cnetboot.system.domain.sys_role;
import cnetboot.system.domain.sys_role_dept;
import cnetboot.system.domain.sys_user_role;

export namespace cnetboot::system {

/**
 * @brief Owns role CRUD and atomic user/department assignments.
 */
class role_service
{
public:
    role_service(cnetmod::application::managed_repository<sys_role> roles,
        cnetmod::application::managed_repository<sys_user_role> user_roles,
        cnetmod::application::managed_repository<sys_role_dept> role_depts);

    [[nodiscard]] auto get(std::int64_t role_id)
        -> cnetmod::task<std::expected<std::optional<sys_role>, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto page(
        cnetboot::framework::pagination::page_request page,
        std::string_view role_name)
        -> cnetmod::task<std::expected<
            cnetboot::framework::pagination::page_result<sys_role>,
            cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto list()
        -> cnetmod::task<std::expected<std::vector<sys_role>, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto create(sys_role role)
        -> cnetmod::task<std::expected<sys_role, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto update(sys_role role)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto remove(std::int64_t role_id)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto roles_for_user(std::int64_t user_id)
        -> cnetmod::task<std::expected<std::vector<sys_role>, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto assign_to_user(std::int64_t user_id,
        std::span<const std::int64_t> role_ids)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto department_ids(std::int64_t role_id)
        -> cnetmod::task<std::expected<std::vector<std::int64_t>, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto assign_departments(std::int64_t role_id,
        std::span<const std::int64_t> department_ids)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;

private:
    cnetmod::application::managed_repository<sys_role> roles_;
    cnetmod::application::managed_repository<sys_user_role> user_roles_;
    cnetmod::application::managed_repository<sys_role_dept> role_depts_;
};

} // namespace cnetboot::system
