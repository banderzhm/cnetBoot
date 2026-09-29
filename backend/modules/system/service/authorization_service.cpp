module cnetboot.system.service.authorization_service;

import std;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.framework.error;
import cnetboot.system.domain.sys_menu;
import cnetboot.system.service.failure;

namespace cnetboot::system {
namespace {

    [[nodiscard]] auto build_menu_level(std::int64_t parent_id,
        const std::vector<sys_menu>& source, std::set<std::int64_t>& ancestry)
        -> std::vector<menu_dto>
    {
        std::vector<const sys_menu*> children;
        for (const auto& item : source)
        {
            if (item.parent_id == parent_id && item.menu_type != "F" &&
                item.visible == 0 && item.status == 0)
                children.push_back(&item);
        }
        std::ranges::sort(children, {}, &sys_menu::order_num);

        std::vector<menu_dto> result;
        result.reserve(children.size());
        for (const auto* item : children)
        {
            if (!ancestry.insert(item->menu_id).second)
                continue;
            menu_dto node{.id = item->menu_id,
                .parent_id = item->parent_id,
                .name = item->menu_name,
                .path = item->path,
                .component = item->component,
                .permission = item->permission,
                .type = item->menu_type,
                .icon = item->icon,
                .order = item->order_num};
            node.children = build_menu_level(item->menu_id, source, ancestry);
            ancestry.erase(item->menu_id);
            result.push_back(std::move(node));
        }
        return result;
    }

} // namespace

authorization_service::authorization_service(
    std::shared_ptr<role_mapper> roles, std::shared_ptr<menu_mapper> menus,
    cnetmod::application::managed_repository<sys_role_dept> role_departments,
    cnetmod::application::managed_repository<sys_dept> departments,
    std::shared_ptr<system_mapper_registry> registry)
    : roles_(std::move(roles)), menus_(std::move(menus)), role_departments_(std::move(role_departments)), departments_(std::move(departments)), registry_(std::move(registry))
{
    if (!roles_ || !menus_ || !registry_)
        throw std::invalid_argument("authorization dependencies are required");
}

auto authorization_service::authorization(std::int64_t user_id)
    -> cnetmod::task<std::expected<authorization_dto, cnetboot::framework::error::error_code>>
{
    auto result = co_await load(user_id);
    if (!result)
        co_return std::unexpected(result.error());
    co_return std::move(result->authorization);
}

auto authorization_service::load(std::int64_t user_id)
    -> cnetmod::task<std::expected<snapshot, cnetboot::framework::error::error_code>>
{
    snapshot response{.authorization = {.user_id = user_id}};
    auto role_rows = co_await roles_->find_by_user_id(user_id);
    if (!role_rows)
        co_return std::unexpected(storage_failure("roles by user", role_rows.error()));
    response.roles = std::move(*role_rows);

    std::vector<std::int64_t> role_ids;
    for (const auto& role : response.roles)
    {
        if (role.status != 0)
            continue;
        role_ids.push_back(role.role_id);
        response.authorization.roles.push_back(
            {.id = role.role_id, .name = role.role_name, .key = role.role_key});
    }
    if (role_ids.empty())
    {
        co_return response;
    }

    auto menu_rows = co_await menus_->find_by_role_ids(role_ids);
    if (!menu_rows)
        co_return std::unexpected(storage_failure("menus by roles", menu_rows.error()));

    std::set<std::string, std::less<>> permissions;
    for (const auto& menu : *menu_rows)
    {
        if (menu.status == 0 && !menu.permission.empty())
            permissions.insert(menu.permission);
    }
    response.authorization.permissions.assign(
        permissions.begin(), permissions.end());
    std::set<std::int64_t> ancestry;
    response.authorization.menus = build_menu_level(0, *menu_rows, ancestry);
    co_return response;
}

auto authorization_service::visible_departments(
    const std::vector<std::int64_t>& roots,
    department_selection selection)
    -> cnetmod::task<std::expected<std::vector<std::int64_t>, cnetboot::framework::error::error_code>>
{
    if (roots.empty())
        co_return std::vector<std::int64_t>{};

    const auto registry = registry_->snapshot();
    if (!registry)
        co_return std::unexpected(storage_failure("visible departments",
            "system XML mapper registry is not ready"));

    cnetmod::orm::param_context parameters;
    if (selection == department_selection::subtree)
    {
        if (roots.size() != 1)
            co_return std::unexpected(integrity_failure("visible departments",
                "department tree requires one root"));
        parameters.set("departmentId", roots.front());
    }
    else
    {
        std::vector<cnetmod::orm::param_context> departments;
        departments.reserve(roots.size());
        for (const auto id : roots)
        {
            cnetmod::orm::param_context department;
            department.set("value", id);
            departments.push_back(std::move(department));
        }
        parameters.add_collection("departmentIds", std::move(departments));
    }
    auto result = co_await departments_.select_xml(*registry,
        selection == department_selection::subtree
            ? "SystemDepartmentMapper.findSubtree"
            : "SystemDepartmentMapper.findVisible",
        parameters);
    if (!result.ok())
        co_return std::unexpected(storage_failure("visible departments", result));

    std::vector<std::int64_t> ids;
    ids.reserve(result.data.size());
    for (const auto& department : result.data)
        ids.push_back(department.dept_id);
    co_return ids;
}

auto authorization_service::principal(
    std::int64_t user_id, std::int64_t department_id)
    -> cnetmod::task<std::expected<principal_authorization, cnetboot::framework::error::error_code>>
{
    auto result = co_await load(user_id);
    if (!result)
        co_return std::unexpected(result.error());

    cnetmod::orm::data_permission_scope scope{
        .include_owner = true,
        .owner_id = user_id,
    };
    std::vector<std::int64_t> custom_role_ids;
    bool own_department = false;
    bool department_tree = false;
    for (const auto& role : result->roles)
    {
        if (role.status != 0 || role.deleted != 0)
            continue;
        if (role.data_scope < static_cast<int>(role_data_scope::all) ||
            role.data_scope > static_cast<int>(role_data_scope::self))
            co_return std::unexpected(integrity_failure("principal", "invalid role data scope"));
        if (role.role_key == "admin" ||
            role.data_scope == static_cast<int>(role_data_scope::all))
        {
            scope.unrestricted = true;
            continue;
        }
        switch (static_cast<role_data_scope>(role.data_scope))
        {
        case role_data_scope::custom:
            custom_role_ids.push_back(role.role_id);
            break;
        case role_data_scope::department:
            own_department = true;
            break;
        case role_data_scope::department_tree:
            department_tree = true;
            break;
        case role_data_scope::self:
            break;
        default:
            co_return std::unexpected(integrity_failure("principal", "invalid role data scope"));
        }
    }

    if (!scope.unrestricted)
    {
        std::vector<std::int64_t> roots;
        if (!custom_role_ids.empty())
        {
            cnetmod::orm::query_wrapper<sys_role_dept> query;
            query.in(&sys_role_dept::role_id, custom_role_ids);
            auto links = co_await role_departments_.list(query);
            if (!links.ok())
                co_return std::unexpected(storage_failure("role departments", links));
            for (const auto& link : links.data)
                if (link.dept_id > 0)
                    roots.push_back(link.dept_id);
        }
        if (own_department && department_id > 0)
            roots.push_back(department_id);
        if (!roots.empty())
        {
            auto visible = co_await visible_departments(
                roots, department_selection::exact);
            if (!visible)
                co_return std::unexpected(visible.error());
            scope.partition_ids = std::move(*visible);
        }
        if (department_tree && department_id > 0)
        {
            auto visible = co_await visible_departments(
                {department_id}, department_selection::subtree);
            if (!visible)
                co_return std::unexpected(visible.error());
            scope.partition_ids.insert(scope.partition_ids.end(),
                visible->begin(), visible->end());
        }
        std::ranges::sort(scope.partition_ids);
        const auto unique = std::ranges::unique(scope.partition_ids);
        scope.partition_ids.erase(unique.begin(), unique.end());
    }
    co_return principal_authorization{
        .authorization = std::move(result->authorization),
        .data_permission = std::move(scope),
    };
}

} // namespace cnetboot::system
