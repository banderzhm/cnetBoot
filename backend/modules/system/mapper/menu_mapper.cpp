module cnetnexus.system.mapper.menu;

import std;
import cnetmod.coro.task;
import cnetmod.orm;

namespace cnetnexus::system {
namespace {

constexpr std::string_view find_by_role_ids_statement =
    "SystemMenuMapper.findByRoleIds";

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

menu_mapper::menu_mapper(
    cnetmod::application::managed_repository<sys_menu> menus,
    std::shared_ptr<system_mapper_registry> registry)
    : menus_(std::move(menus)), registry_(std::move(registry))
{
    if (!registry_)
        throw std::invalid_argument("mapper registry is required");
}

auto menu_mapper::find_by_role_ids(std::span<const std::int64_t> role_ids)
    -> cnetmod::task<std::expected<std::vector<sys_menu>, std::string>>
{
    if (role_ids.empty())
        co_return std::vector<sys_menu>{};
    const auto registry = registry_->snapshot();
    if (!registry)
        co_return std::unexpected("system XML mapper registry is not ready");

    cnetmod::orm::param_context parameters;
    std::vector<cnetmod::orm::param_context> items;
    items.reserve(role_ids.size());
    for (const auto role_id : role_ids)
    {
        cnetmod::orm::param_context item;
        item.set("value", role_id);
        items.push_back(std::move(item));
    }
    parameters.add_collection("roleIds", std::move(items));
    auto result = co_await menus_.select_xml(
        *registry, find_by_role_ids_statement, parameters);
    if (!result.ok())
        co_return std::unexpected(failure_of(result));
    co_return std::move(result.data);
}

} // namespace cnetnexus::system
