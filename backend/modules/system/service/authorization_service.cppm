export module cnetnexus.system.service.authorization_service;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetnexus.framework.error;
import cnetnexus.system.dto.authorization;
import cnetnexus.system.domain.sys_dept;
import cnetnexus.system.domain.sys_role;
import cnetnexus.system.domain.sys_role_dept;
import cnetnexus.system.mapper.menu;
import cnetnexus.system.mapper.registry;
import cnetnexus.system.mapper.role;

export namespace cnetnexus::system {

struct principal_authorization
{
    authorization_dto authorization;
    cnetmod::orm::data_permission_scope data_permission;
};

/** Builds current role/menu grants and ORM visibility from database state. */
class authorization_service
{
public:
    authorization_service(std::shared_ptr<role_mapper> roles,
        std::shared_ptr<menu_mapper> menus,
        cnetmod::application::managed_repository<sys_role_dept> role_departments,
        cnetmod::application::managed_repository<sys_dept> departments,
        std::shared_ptr<system_mapper_registry> registry);

    [[nodiscard]] auto authorization(std::int64_t user_id)
        -> cnetmod::task<std::expected<authorization_dto, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto principal(std::int64_t user_id,
        std::int64_t department_id)
        -> cnetmod::task<std::expected<principal_authorization, cnetnexus::framework::error::error_code>>;

private:
    enum class department_selection
    {
        exact,
        subtree,
    };

    struct snapshot
    {
        authorization_dto authorization;
        std::vector<sys_role> roles;
    };

    std::shared_ptr<role_mapper> roles_;
    std::shared_ptr<menu_mapper> menus_;
    cnetmod::application::managed_repository<sys_role_dept> role_departments_;
    cnetmod::application::managed_repository<sys_dept> departments_;
    std::shared_ptr<system_mapper_registry> registry_;

    [[nodiscard]] auto load(std::int64_t user_id)
        -> cnetmod::task<std::expected<snapshot, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto visible_departments(
        const std::vector<std::int64_t>& roots,
        department_selection selection)
        -> cnetmod::task<std::expected<std::vector<std::int64_t>, cnetnexus::framework::error::error_code>>;
};

} // namespace cnetnexus::system
