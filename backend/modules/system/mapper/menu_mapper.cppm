export module cnetboot.system.mapper.menu;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetboot.system.domain.sys_menu;
import cnetboot.system.mapper.registry;

export namespace cnetboot::system {

/**
 * @brief Strongly typed menu queries backed by the system XML registry.
 */
class menu_mapper
{
public:
    menu_mapper(cnetmod::application::managed_repository<sys_menu> menus,
        std::shared_ptr<system_mapper_registry> registry);

    [[nodiscard]] auto find_by_role_ids(
        std::span<const std::int64_t> role_ids)
        -> cnetmod::task<std::expected<std::vector<sys_menu>, std::string>>;

private:
    cnetmod::application::managed_repository<sys_menu> menus_;
    std::shared_ptr<system_mapper_registry> registry_;
};

} // namespace cnetboot::system
