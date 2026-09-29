module cnetboot.system.mapper.role;

import std;
import cnetmod.coro.task;
import cnetmod.orm;

namespace cnetboot::system {
namespace {

constexpr std::string_view find_by_user_id_statement =
    "SystemRoleMapper.findByUserId";

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

} // namespace

role_mapper::role_mapper(
    cnetmod::application::managed_repository<sys_role> roles,
    std::shared_ptr<system_mapper_registry> registry)
    : roles_(std::move(roles)), registry_(std::move(registry))
{
    if (!registry_)
        throw std::invalid_argument("mapper registry is required");
}

auto role_mapper::find_by_user_id(std::int64_t user_id)
    -> cnetmod::task<std::expected<std::vector<sys_role>, std::string>>
{
    const auto registry = registry_->snapshot();
    if (!registry)
        co_return std::unexpected("system XML mapper registry is not ready");
    cnetmod::orm::param_context parameters;
    parameters.set("userId", user_id);
    auto result = co_await roles_.select_xml(
        *registry, find_by_user_id_statement, parameters);
    if (!result.ok())
        co_return std::unexpected(failure_of(result));
    co_return std::move(result.data);
}

} // namespace cnetboot::system
