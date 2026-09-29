export module cnetboot.system.mapper.role;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetboot.system.domain.sys_role;
import cnetboot.system.mapper.registry;

export namespace cnetboot::system {

/**
 * @brief Strongly typed role queries backed by the system XML registry.
 */
class role_mapper
{
public:
    role_mapper(cnetmod::application::managed_repository<sys_role> roles,
        std::shared_ptr<system_mapper_registry> registry);

    [[nodiscard]] auto find_by_user_id(std::int64_t user_id)
        -> cnetmod::task<std::expected<std::vector<sys_role>, std::string>>;

private:
    cnetmod::application::managed_repository<sys_role> roles_;
    std::shared_ptr<system_mapper_registry> registry_;
};

} // namespace cnetboot::system
