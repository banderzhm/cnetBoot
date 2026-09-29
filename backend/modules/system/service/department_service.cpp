module cnetnexus.system.service.department_service;

import std;
import cnetnexus.framework.error;
import cnetnexus.system.service.failure;
import cnetmod.coro.task;
import cnetmod.orm;

namespace cnetnexus::system {
namespace {

template <typename T>
[[nodiscard]] auto failure_of(const cnetmod::orm::model_result<T>& result)
    -> std::string
{
    if (!result.error_msg.empty())
        return result.error_msg;
    if (result.framework_error)
        return result.framework_error.message();
    return "database operation failed";
}

[[nodiscard]] auto build_level(std::int64_t parent_id,
    const std::vector<sys_dept>& source, std::set<std::int64_t>& ancestry)
    -> std::vector<department_node>
{
    std::vector<const sys_dept*> children;
    for (const auto& department : source)
    {
        if (department.parent_id == parent_id && department.status == 0)
            children.push_back(&department);
    }
    std::ranges::sort(children, {}, &sys_dept::order_num);
    std::vector<department_node> result;
    result.reserve(children.size());
    for (const auto* department : children)
    {
        if (!ancestry.insert(department->dept_id).second)
            continue;
        department_node node{.value = *department};
        node.children = build_level(department->dept_id, source, ancestry);
        ancestry.erase(department->dept_id);
        result.push_back(std::move(node));
    }
    return result;
}

[[nodiscard]] auto is_descendant(std::int64_t candidate,
    std::int64_t root, const std::vector<sys_dept>& source) -> bool
{
    auto cursor = candidate;
    std::set<std::int64_t> visited;
    while (cursor > 0 && visited.insert(cursor).second)
    {
        if (cursor == root)
            return true;
        const auto parent = std::ranges::find(source, cursor, &sys_dept::dept_id);
        if (parent == source.end())
            return false;
        cursor = parent->parent_id;
    }
    return false;
}

} // namespace

department_service::department_service(
    cnetmod::application::managed_repository<sys_dept> departments,
    cnetmod::application::managed_repository<sys_user> users)
    : departments_(std::move(departments)), users_(std::move(users))
{
}

auto department_service::get(std::int64_t department_id)
    -> cnetmod::task<std::expected<std::optional<sys_dept>, cnetnexus::framework::error::error_code>>
{
    auto result = co_await departments_.get_by_id(
        cnetmod::orm::param_value::from_int(department_id));
    if (!result.ok())
        co_return std::unexpected(storage_failure("department", result));
    co_return result.first();
}

auto department_service::list(std::string_view name)
    -> cnetmod::task<std::expected<std::vector<sys_dept>, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_dept> query;
    if (!name.empty())
        query.like(&sys_dept::dept_name, name);
    query.order_by_asc(&sys_dept::order_num);
    auto result = co_await departments_.list(query);
    if (!result.ok())
        co_return std::unexpected(storage_failure("department", result));
    co_return std::move(result.data);
}

auto department_service::tree(std::string_view name)
    -> cnetmod::task<std::expected<std::vector<department_node>, cnetnexus::framework::error::error_code>>
{
    auto departments = co_await list(name);
    if (!departments)
        co_return std::unexpected(departments.error());
    std::set<std::int64_t> ancestry;
    co_return build_level(0, *departments, ancestry);
}

auto department_service::create(sys_dept department)
    -> cnetmod::task<std::expected<sys_dept, cnetnexus::framework::error::error_code>>
{
    if (department.dept_name.empty() || department.parent_id < 0)
        co_return std::unexpected(error_code::request_invalid_argument);
    if (department.parent_id > 0)
    {
        auto parent = co_await get(department.parent_id);
        if (!parent)
            co_return std::unexpected(parent.error());
        if (!*parent)
            co_return std::unexpected(error_code::department_not_found);
    }
    auto result = co_await departments_.save(department);
    if (!result.ok())
        co_return std::unexpected(storage_failure("department", result));
    co_return department;
}

auto department_service::update(sys_dept department)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    if (department.dept_id <= 0 || department.dept_name.empty() ||
        department.parent_id == department.dept_id)
        co_return std::unexpected(error_code::request_invalid_argument);
    auto all = co_await list();
    if (!all)
        co_return std::unexpected(all.error());
    if (department.parent_id > 0 &&
        is_descendant(department.parent_id, department.dept_id, *all))
        co_return std::unexpected(error_code::department_conflict);
    auto result = co_await departments_.update_by_id(department);
    if (!result.ok())
        co_return std::unexpected(storage_failure("department", result));
    co_return {};
}

auto department_service::remove(std::int64_t department_id)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_dept> child_query;
    child_query.eq(&sys_dept::parent_id, department_id);
    auto children = co_await departments_.exists(child_query);
    if (!children.ok())
        co_return std::unexpected(storage_failure("department", children));
    if (!children.data.empty() && children.data.front())
        co_return std::unexpected(error_code::department_conflict);

    cnetmod::orm::query_wrapper<sys_user> user_query;
    user_query.eq(&sys_user::dept_id, department_id);
    auto users = co_await users_.exists(user_query);
    if (!users.ok())
        co_return std::unexpected(storage_failure("department", users));
    if (!users.data.empty() && users.data.front())
        co_return std::unexpected(error_code::department_conflict);

    auto removed = co_await departments_.remove_by_id(
        cnetmod::orm::param_value::from_int(department_id));
    if (!removed.ok())
        co_return std::unexpected(storage_failure("department", removed));
    co_return {};
}

} // namespace cnetnexus::system
