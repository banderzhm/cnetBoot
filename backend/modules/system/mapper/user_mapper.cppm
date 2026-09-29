export module cnetboot.system.mapper.user;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetboot.system.domain.sys_user;
import cnetboot.system.domain.user_record;
import cnetboot.system.mapper.registry;
import cnetmod.orm;

export namespace cnetboot::system {

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

} // namespace cnetboot::system
