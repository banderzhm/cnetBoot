export module cnetnexus.system.service.department_service;

import std;
import cnetnexus.framework.error;
import cnetmod.application;
import cnetmod.coro.task;
import cnetnexus.system.domain.sys_dept;
import cnetnexus.system.domain.sys_user;

export namespace cnetnexus::system {

struct department_node
{
    sys_dept value;
    std::vector<department_node> children;
};

/**
 * @brief Owns department CRUD, hierarchy validation, and tree projection.
 */
class department_service
{
public:
    department_service(
        cnetmod::application::managed_repository<sys_dept> departments,
        cnetmod::application::managed_repository<sys_user> users);

    [[nodiscard]] auto get(std::int64_t department_id)
        -> cnetmod::task<std::expected<std::optional<sys_dept>, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto list(std::string_view name = {})
        -> cnetmod::task<std::expected<std::vector<sys_dept>, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto tree(std::string_view name = {})
        -> cnetmod::task<std::expected<std::vector<department_node>, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto create(sys_dept department)
        -> cnetmod::task<std::expected<sys_dept, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto update(sys_dept department)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto remove(std::int64_t department_id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;

private:
    cnetmod::application::managed_repository<sys_dept> departments_;
    cnetmod::application::managed_repository<sys_user> users_;
};

} // namespace cnetnexus::system
