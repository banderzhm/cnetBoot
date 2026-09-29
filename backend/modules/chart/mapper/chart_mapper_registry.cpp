module cnetnexus.chart.mapper.chart_mapper_registry;

import std;
import cnetmod.coro;
import cnetmod.orm;
import cnetmod.orm.xml_mapper_registry;

namespace cnetnexus::chart {
namespace {

constexpr std::string_view registry_key{"chart_mapper_registry"};

/// 必需语句清单：启动期缺一条就拒绝启动，运行期不必再猜 XML 是否写错。
constexpr std::array required_statements{
    std::string_view{"ChartMapper.selectCases"},
    std::string_view{"ChartMapper.countCases"},
    std::string_view{"ChartMapper.selectTagsForCases"},
    std::string_view{"ChartMapper.searchArchives"},
    std::string_view{"ChartMapper.countArchives"},
    std::string_view{"ChartMapper.selectAllTags"},
    std::string_view{"ChartMapper.countsByAlgorithm"},
    std::string_view{"ChartMapper.upsertArchive"}};

} // namespace

chart_mapper_registry::chart_mapper_registry(
    std::filesystem::path mapper_directory)
    : mapper_directory_(std::move(mapper_directory))
{
}

auto chart_mapper_registry::key() const -> cnetmod::application::service_key
{
    return cnetmod::application::service_key{std::string{registry_key}};
}

auto chart_mapper_registry::requirement() const noexcept
    -> cnetmod::application::service_requirement
{
    return cnetmod::application::service_requirement::required;
}

auto chart_mapper_registry::start(cnetmod::application::service_context&)
    -> cnetmod::task<std::expected<void, std::error_code>>
{
    auto registry = std::make_shared<cnetmod::orm::mapper_registry>();
    if (auto loaded = load_chart_mappers(*registry, mapper_directory_); !loaded)
    {
        last_error_ = loaded.error();
        co_return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    }
    registry_ = std::move(registry);
    last_error_.clear();
    co_return std::expected<void, std::error_code>{};
}

auto chart_mapper_registry::stop(cnetmod::application::service_context&)
    -> cnetmod::task<std::expected<void, std::error_code>>
{
    registry_.reset();
    co_return std::expected<void, std::error_code>{};
}

auto chart_mapper_registry::probe(cnetmod::application::service_context&)
    -> cnetmod::task<cnetmod::application::health_report>
{
    co_return cnetmod::application::health_report{
        .status = registry_
            ? cnetmod::application::service_health::up
            : cnetmod::application::service_health::down,
        .message = registry_ ? "chart XML mappers loaded" : last_error_,
        .error = registry_
            ? std::error_code{}
            : std::make_error_code(std::errc::state_not_recoverable),
        .checked_at = std::chrono::system_clock::now(),
    };
}

auto chart_mapper_registry::snapshot() const noexcept
    -> std::shared_ptr<const cnetmod::orm::mapper_registry>
{
    return registry_;
}

auto load_chart_mappers(cnetmod::orm::mapper_registry& registry,
    const std::filesystem::path& directory) -> std::expected<void, std::string>
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error)
        return std::unexpected{
            std::format("mapper directory not found: {}", directory.string())};

    if (auto loaded = registry.load_directory(directory); !loaded)
        return std::unexpected{std::format("mapper load failed ({}): {}",
            directory.string(), loaded.error())};

    // 简单单表 CRUD 已改走 ORM，不再登记；这里只校验动态 SQL 语句。
    for (const auto statement : required_statements)
    {
        if (registry.find_statement(statement) == nullptr)
            return std::unexpected{std::format("mapper is missing statement: {}",
                statement)};
    }
    return {};
}

} // namespace cnetnexus::chart
