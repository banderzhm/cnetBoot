module cnetnexus.system.mapper.user;

import std;
import cnetmod.coro.task;
import cnetmod.orm;

namespace cnetnexus::system {
namespace {

    constexpr std::string_view count_page_statement =
        "SystemUserMapper.countPage";
    constexpr std::string_view find_page_statement =
        "SystemUserMapper.findPage";

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

    [[nodiscard]] auto parameters_for(std::string_view user_name,
        std::size_t offset, std::size_t limit)
        -> cnetmod::orm::param_context
    {
        cnetmod::orm::param_context parameters;
        parameters.set("userName", std::string{user_name});
        parameters.set("offset", static_cast<std::uint64_t>(offset));
        parameters.set("limit", static_cast<std::uint64_t>(limit));
        return parameters;
    }

} // namespace

user_mapper::user_mapper(
    cnetmod::application::managed_repository<sys_user>& users,
    std::shared_ptr<system_mapper_registry> registry)
    : users_(users), registry_(std::move(registry))
{
    if (!registry_)
        throw std::invalid_argument("mapper registry is required");
}

auto user_mapper::find_page(std::size_t page_number, std::size_t page_size,
    std::string_view user_name)
    -> cnetmod::task<std::expected<user_query_page, std::string>>
{
    const auto registry = registry_->snapshot();
    if (!registry)
        co_return std::unexpected("system XML mapper registry is not ready");
    const auto offset = (page_number - 1) * page_size;
    auto parameters = parameters_for(user_name, offset, page_size);
    auto count = co_await users_.select_xml_as<user_count_row>(
        *registry, count_page_statement, parameters);
    if (!count.ok())
        co_return std::unexpected(failure_of(count));
    auto rows = co_await users_.select_xml_as<user_list_row>(
        *registry, find_page_statement, parameters);
    if (!rows.ok())
        co_return std::unexpected(failure_of(rows));
    co_return user_query_page{.rows = std::move(rows.data),
        .total = count.data.empty()
            ? 0U
            : static_cast<std::size_t>(count.data.front().total)};
}

} // namespace cnetnexus::system
