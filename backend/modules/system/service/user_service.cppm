export module cnetnexus.system.service.user_service;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;
import cnetnexus.system.domain.sys_user;
import cnetnexus.system.domain.user_record;
import cnetnexus.system.dto.user;
import cnetnexus.system.mapper.user;
import cnetnexus.system.mapper.registry;

export namespace cnetnexus::system {

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
        -> cnetmod::task<std::expected<std::optional<user_dto>, cnetnexus::framework::error::error_code>>;

    [[nodiscard]] auto page(
        cnetnexus::framework::pagination::page_request page,
        std::string_view user_name)
        -> cnetmod::task<std::expected<
            cnetnexus::framework::pagination::page_result<user_dto>,
            cnetnexus::framework::error::error_code>>;

    [[nodiscard]] auto create(sys_user user)
        -> cnetmod::task<std::expected<user_dto, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto update(sys_user user)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto remove(std::int64_t user_id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;

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

} // namespace cnetnexus::system
