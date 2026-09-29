module cnetboot.system.mapper.registry;

import std;
import cnetmod.application.async_file_template;
import cnetmod.coro.task;

namespace cnetboot::system {

system_mapper_registry::system_mapper_registry(
    std::filesystem::path mapper_directory)
    : mapper_directory_(std::move(mapper_directory))
{
    if (mapper_directory_.empty())
        throw std::invalid_argument("mapper directory is required");
}

auto system_mapper_registry::key() const
    -> cnetmod::application::service_key
{
    return {.name = "xml-mapper", .instance = "system"};
}

auto system_mapper_registry::requirement() const noexcept
    -> cnetmod::application::service_requirement
{
    return cnetmod::application::service_requirement::required;
}

auto system_mapper_registry::start(
    cnetmod::application::service_context& context)
    -> cnetmod::task<std::expected<void, std::error_code>>
{
    cnetmod::application::async_file_template files{context.io};
    auto next = std::make_shared<cnetmod::orm::mapper_registry>();
    std::vector<std::filesystem::path> mapper_files;
    std::error_code discovery_error;
    for (std::filesystem::directory_iterator entries{
             mapper_directory_, discovery_error};
         !discovery_error && entries != std::filesystem::directory_iterator{};
         entries.increment(discovery_error))
    {
        const auto& entry = *entries;
        if (entry.is_regular_file(discovery_error) &&
            entry.path().extension() == ".xml")
            mapper_files.push_back(entry.path());
    }
    if (discovery_error)
    {
        last_error_ = std::format("failed to enumerate mapper directory {}: {}",
            mapper_directory_.string(), discovery_error.message());
        co_return std::unexpected(discovery_error);
    }
    std::ranges::sort(mapper_files);
    if (mapper_files.empty())
    {
        last_error_ = std::format(
            "mapper directory {} contains no XML files",
            mapper_directory_.string());
        co_return std::unexpected(
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    for (const auto& path : mapper_files)
    {
        auto content = co_await files.read_all(path, context.cancellation);
        if (!content)
        {
            last_error_ = std::format(
                "failed to read mapper {}: {}", path.string(),
                content.error().message());
            co_return std::unexpected(content.error());
        }
        auto loaded = next->load_xml(*content);
        if (!loaded)
        {
            last_error_ = std::format(
                "failed to parse mapper {}: {}", path.string(),
                loaded.error());
            co_return std::unexpected(
                std::make_error_code(std::errc::invalid_argument));
        }
    }
    registry_ = std::move(next);
    last_error_.clear();
    co_return {};
}

auto system_mapper_registry::stop(
    cnetmod::application::service_context&)
    -> cnetmod::task<std::expected<void, std::error_code>>
{
    co_return {};
}

auto system_mapper_registry::probe(
    cnetmod::application::service_context&)
    -> cnetmod::task<cnetmod::application::health_report>
{
    co_return cnetmod::application::health_report{
        .status = registry_
            ? cnetmod::application::service_health::up
            : cnetmod::application::service_health::down,
        .message = registry_ ? "system XML mappers loaded" : last_error_,
        .error = registry_ ? std::error_code{}
                           : std::make_error_code(std::errc::state_not_recoverable),
        .checked_at = std::chrono::system_clock::now(),
    };
}

auto system_mapper_registry::snapshot() const noexcept
    -> std::shared_ptr<const cnetmod::orm::mapper_registry>
{
    return registry_;
}

} // namespace cnetboot::system
