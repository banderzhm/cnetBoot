module cnetnexus.system.service.role_service;

import std;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;
import cnetnexus.system.service.failure;
import cnetmod.coro.task;
import cnetmod.orm;

namespace cnetnexus::system {
namespace {

} // namespace

role_service::role_service(
    cnetmod::application::managed_repository<sys_role> roles,
    cnetmod::application::managed_repository<sys_user_role> user_roles,
    cnetmod::application::managed_repository<sys_role_dept> role_depts)
    : roles_(std::move(roles)), user_roles_(std::move(user_roles)), role_depts_(std::move(role_depts))
{
}

auto role_service::get(std::int64_t role_id)
    -> cnetmod::task<std::expected<std::optional<sys_role>, cnetnexus::framework::error::error_code>>
{
    auto result = co_await roles_.get_by_id(
        cnetmod::orm::param_value::from_int(role_id));
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    co_return result.first();
}

auto role_service::page(
    cnetnexus::framework::pagination::page_request request,
    std::string_view role_name)
    -> cnetmod::task<std::expected<
        cnetnexus::framework::pagination::page_result<sys_role>,
        cnetnexus::framework::error::error_code>>
{
    const auto page = cnetnexus::framework::pagination::normalize(request);
    cnetmod::orm::query_wrapper<sys_role> query;
    if (!role_name.empty())
        query.like(&sys_role::role_name, role_name);
    query.order_by_asc(&sys_role::role_sort);
    auto result = co_await roles_.page(
        static_cast<std::size_t>(page.page),
        static_cast<std::size_t>(page.page_size), query);
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result.records));
    co_return cnetnexus::framework::pagination::page_result<sys_role>{
        .items = std::move(result.records.data),
        .total = static_cast<std::int64_t>(result.total),
        .page = static_cast<int>(result.page),
        .page_size = static_cast<int>(result.page_size)};
}

auto role_service::list()
    -> cnetmod::task<std::expected<std::vector<sys_role>, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_role> query;
    query.order_by_asc(&sys_role::role_sort);
    auto result = co_await roles_.list(query);
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    co_return std::move(result.data);
}

auto role_service::create(sys_role role)
    -> cnetmod::task<std::expected<sys_role, cnetnexus::framework::error::error_code>>
{
    if (role.role_name.empty() || role.role_key.empty() ||
        role.data_scope < static_cast<int>(role_data_scope::all) ||
        role.data_scope > static_cast<int>(role_data_scope::self))
        co_return std::unexpected(error_code::request_invalid_argument);
    auto result = co_await roles_.save(role);
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    co_return role;
}

auto role_service::update(sys_role role)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    if (role.role_id <= 0 || role.role_name.empty() || role.role_key.empty() ||
        role.data_scope < static_cast<int>(role_data_scope::all) ||
        role.data_scope > static_cast<int>(role_data_scope::self))
        co_return std::unexpected(error_code::request_invalid_argument);
    auto result = co_await roles_.update_by_id(role);
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    co_return {};
}

auto role_service::remove(std::int64_t role_id)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_user_role> assigned;
    assigned.eq(&sys_user_role::role_id, role_id);
    auto used = co_await user_roles_.exists(assigned);
    if (!used.ok())
        co_return std::unexpected(storage_failure("role", used));
    if (!used.data.empty() && used.data.front())
        co_return std::unexpected(error_code::role_conflict);
    auto result = co_await roles_.remove_by_id(
        cnetmod::orm::param_value::from_int(role_id));
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    co_return {};
}

auto role_service::roles_for_user(std::int64_t user_id)
    -> cnetmod::task<std::expected<std::vector<sys_role>, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_user_role> query;
    query.eq(&sys_user_role::user_id, user_id);
    auto links = co_await user_roles_.list(query);
    if (!links.ok())
        co_return std::unexpected(storage_failure("role", links));
    std::vector<std::int64_t> ids;
    ids.reserve(links.data.size());
    for (const auto& link : links.data)
        ids.push_back(link.role_id);
    if (ids.empty())
        co_return std::vector<sys_role>{};
    auto roles = co_await roles_.list_by_ids(std::span<const std::int64_t>{ids});
    if (!roles.ok())
        co_return std::unexpected(storage_failure("role", roles));
    co_return std::move(roles.data);
}

auto role_service::assign_to_user(std::int64_t user_id,
    std::span<const std::int64_t> role_ids)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    auto ids = std::vector<std::int64_t>{role_ids.begin(), role_ids.end()};
    auto replaced = co_await roles_.transaction<void>(
        [user_id, ids = std::move(ids)](auto& unit) mutable
            -> cnetmod::task<std::expected<void, std::string>>
        {
            auto links = unit.template mapper<sys_user_role>();
            cnetmod::orm::query_wrapper<sys_user_role> query;
            query.eq(&sys_user_role::user_id, user_id);
            auto removed = co_await links.remove(query);
            if (!removed.ok())
                co_return std::unexpected(removed.error_msg);
            for (const auto role_id : ids)
            {
                sys_user_role link{.user_id = user_id, .role_id = role_id};
                auto inserted = co_await links.insert(link);
                if (!inserted.ok())
                    co_return std::unexpected(inserted.error_msg);
            }
            co_return {};
        });
    if (!replaced)
        co_return std::unexpected(storage_failure("role", replaced.error()));
    co_return {};
}

auto role_service::department_ids(std::int64_t role_id)
    -> cnetmod::task<std::expected<std::vector<std::int64_t>, cnetnexus::framework::error::error_code>>
{
    cnetmod::orm::query_wrapper<sys_role_dept> query;
    query.eq(&sys_role_dept::role_id, role_id);
    auto result = co_await role_depts_.list(query);
    if (!result.ok())
        co_return std::unexpected(storage_failure("role", result));
    std::vector<std::int64_t> ids;
    ids.reserve(result.data.size());
    for (const auto& link : result.data)
        ids.push_back(link.dept_id);
    co_return ids;
}

auto role_service::assign_departments(std::int64_t role_id,
    std::span<const std::int64_t> department_ids)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    auto ids = std::vector<std::int64_t>{
        department_ids.begin(), department_ids.end()};
    auto replaced = co_await roles_.transaction<void>(
        [role_id, ids = std::move(ids)](auto& unit) mutable
            -> cnetmod::task<std::expected<void, std::string>>
        {
            auto links = unit.template mapper<sys_role_dept>();
            cnetmod::orm::query_wrapper<sys_role_dept> query;
            query.eq(&sys_role_dept::role_id, role_id);
            auto removed = co_await links.remove(query);
            if (!removed.ok())
                co_return std::unexpected(removed.error_msg);
            for (const auto department_id : ids)
            {
                sys_role_dept link{
                    .role_id = role_id,
                    .dept_id = department_id};
                auto inserted = co_await links.insert(link);
                if (!inserted.ok())
                    co_return std::unexpected(inserted.error_msg);
            }
            co_return {};
        });
    if (!replaced)
        co_return std::unexpected(storage_failure("role", replaced.error()));
    co_return {};
}

} // namespace cnetnexus::system
