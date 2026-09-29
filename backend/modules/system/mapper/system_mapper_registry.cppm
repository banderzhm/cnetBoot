export module cnetboot.system.mapper.registry;

import std;
import cnetmod.application;
import cnetmod.application.managed_service;
import cnetmod.coro.task;
import cnetmod.orm;

export namespace cnetboot::system {

/**
 * @brief Loads system XML mappers asynchronously and retains an immutable registry.
 *
 * The service publishes a snapshot only after every configured document has
 * been read and parsed successfully. Request handlers therefore never perform
 * file I/O and cannot observe a partially loaded mapper set.
 */
class system_mapper_registry final
    : public cnetmod::application::managed_service
{
public:
    explicit system_mapper_registry(
        std::filesystem::path mapper_directory = "mapper");

    [[nodiscard]] auto key() const
        -> cnetmod::application::service_key override;
    [[nodiscard]] auto requirement() const noexcept
        -> cnetmod::application::service_requirement override;
    auto start(cnetmod::application::service_context& context)
        -> cnetmod::task<std::expected<void, std::error_code>> override;
    auto stop(cnetmod::application::service_context& context)
        -> cnetmod::task<std::expected<void, std::error_code>> override;
    auto probe(cnetmod::application::service_context& context)
        -> cnetmod::task<cnetmod::application::health_report> override;

    [[nodiscard]] auto snapshot() const noexcept
        -> std::shared_ptr<const cnetmod::orm::mapper_registry>;

private:
    std::filesystem::path mapper_directory_;
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
    std::string last_error_;
};

} // namespace cnetboot::system
