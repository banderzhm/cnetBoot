module cnetboot.system.service.user_service;

import std;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.system.service.failure;

namespace cnetboot::system {
namespace {

    [[nodiscard]] auto to_dto(sys_user user) -> user_dto
    {
        return {.id = user.user_id,
            .department_id = user.dept_id,
            .department_name = {},
            .username = std::move(user.user_name),
            .nickname = std::move(user.nick_name),
            .email = std::move(user.email),
            .enabled = user.status == 0};
    }

    [[nodiscard]] auto to_dto(user_list_row user) -> user_dto
    {
        return {.id = user.user_id,
            .department_id = user.dept_id,
            .department_name = std::move(user.dept_name),
            .username = std::move(user.user_name),
            .nickname = std::move(user.nick_name),
            .email = std::move(user.email),
            .enabled = user.status == 0};
    }

} // namespace

user_service::user_service(
    cnetmod::application::managed_repository<sys_user> users,
    std::shared_ptr<system_mapper_registry> registry)
    : users_(std::move(users)), queries_(users_, std::move(registry))
{
}

user_service_factory::user_service_factory(
    std::shared_ptr<system_mapper_registry> registry) noexcept
    : registry_{std::move(registry)}
{
}

auto user_service_factory::make(
    cnetmod::application::managed_repository<sys_user> users) const
    -> std::unique_ptr<user_service>
{
    return std::make_unique<user_service>(std::move(users), registry_);
}

auto user_service::get(std::int64_t user_id)
    -> cnetmod::task<std::expected<std::optional<user_dto>, cnetboot::framework::error::error_code>>
{
    auto result = co_await users_.get_by_id(
        cnetmod::orm::param_value::from_int(user_id));
    if (!result.ok())
        co_return std::unexpected(storage_failure("user by id", result));
    auto user = result.first();
    if (!user)
        co_return std::optional<user_dto>{};
    co_return std::optional<user_dto>{to_dto(std::move(*user))};
}

auto user_service::page(
    cnetboot::framework::pagination::page_request request,
    std::string_view user_name)
    -> cnetmod::task<std::expected<
        cnetboot::framework::pagination::page_result<user_dto>,
        cnetboot::framework::error::error_code>>
{
    const auto page_request = cnetboot::framework::pagination::normalize(request);
    auto page = co_await queries_.find_page(
        static_cast<std::size_t>(page_request.page),
        static_cast<std::size_t>(page_request.page_size), user_name);
    if (!page)
        co_return std::unexpected(storage_failure("user page", page.error()));
    cnetboot::framework::pagination::page_result<user_dto> response{
        .total = static_cast<std::int64_t>(page->total),
        .page = page_request.page,
        .page_size = page_request.page_size};
    response.items.reserve(page->rows.size());
    for (auto& user : page->rows)
        response.items.push_back(to_dto(std::move(user)));
    co_return response;
}

auto user_service::create(sys_user user)
    -> cnetmod::task<std::expected<user_dto, cnetboot::framework::error::error_code>>
{
    if (user.user_name.empty() || user.nick_name.empty() ||
        user.password_hash.empty())
        co_return std::unexpected(error_code::request_invalid_argument);
    auto result = co_await users_.save(user);
    if (!result.ok())
        co_return std::unexpected(storage_failure("create user", result));
    co_return to_dto(std::move(user));
}

auto user_service::update(sys_user user)
    -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>
{
    if (user.user_id <= 0 || user.user_name.empty() || user.nick_name.empty())
        co_return std::unexpected(error_code::request_invalid_argument);
    auto result = co_await users_.update_by_id(user);
    if (!result.ok())
        co_return std::unexpected(storage_failure("update user", result));
    co_return {};
}

auto user_service::remove(std::int64_t user_id)
    -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>
{
    auto result = co_await users_.remove_by_id(
        cnetmod::orm::param_value::from_int(user_id));
    if (!result.ok())
        co_return std::unexpected(storage_failure("remove user", result));
    co_return {};
}

} // namespace cnetboot::system
