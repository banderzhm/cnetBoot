export module cnetboot.system.service.user_service;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.system.domain.sys_user;
import cnetboot.system.domain.user_record;
import cnetboot.system.dto.user;
import cnetboot.system.mapper.user;
import cnetboot.system.mapper.registry;

export namespace cnetboot::system {

/**
 * @brief Executes system-user use cases without exposing SQL or sessions.
 */
class user_service
{
public:
    user_service(cnetmod::application::managed_repository<sys_user> users,
        std::shared_ptr<system_mapper_registry> registry);
    user_service(const user_service&) = delete;
    user_service(user_service&&) = delete;

    [[nodiscard]] auto get(std::int64_t user_id)
        -> cnetmod::task<std::expected<std::optional<user_dto>, cnetboot::framework::error::error_code>>;

    [[nodiscard]] auto page(
        cnetboot::framework::pagination::page_request page,
        std::string_view user_name)
        -> cnetmod::task<std::expected<
            cnetboot::framework::pagination::page_result<user_dto>,
            cnetboot::framework::error::error_code>>;

    [[nodiscard]] auto create(sys_user user)
        -> cnetmod::task<std::expected<user_dto, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto update(sys_user user)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;
    [[nodiscard]] auto remove(std::int64_t user_id)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;

private:
    cnetmod::application::managed_repository<sys_user> users_;
    user_mapper queries_;
};

/** Provides one data-permission-bound user service per HTTP request. */
class user_service_factory
{
public:
    explicit user_service_factory(
        std::shared_ptr<system_mapper_registry> registry) noexcept;

    [[nodiscard]] auto make(
        cnetmod::application::managed_repository<sys_user> users) const
        -> std::unique_ptr<user_service>;

private:
    std::shared_ptr<system_mapper_registry> registry_;
};

} // namespace cnetboot::system
