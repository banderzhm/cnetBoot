export module cnetnexus.system.mapper.user;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetnexus.system.domain.sys_user;
import cnetnexus.system.domain.user_record;
import cnetnexus.system.mapper.registry;
import cnetmod.orm;

export namespace cnetnexus::system {

struct user_query_page
{
    std::vector<user_list_row> rows;
    std::size_t total{};
};

/**
 * @brief Strongly typed user queries backed by the system XML registry.
 */
class user_mapper
{
public:
    user_mapper(cnetmod::application::managed_repository<sys_user>& users,
        std::shared_ptr<system_mapper_registry> registry);

    [[nodiscard]] auto find_page(std::size_t page_number, std::size_t page_size,
        std::string_view user_name)
        -> cnetmod::task<std::expected<user_query_page, std::string>>;

private:
    cnetmod::application::managed_repository<sys_user>& users_;
    std::shared_ptr<system_mapper_registry> registry_;
};

} // namespace cnetnexus::system
