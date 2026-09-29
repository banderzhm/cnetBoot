export module cnetnexus.chart.mapper.chart_mapper_registry;

import std;
import cnetmod.application.managed_service;
import cnetmod.application.service_lifecycle;
import cnetmod.coro;
import cnetmod.orm;
import cnetmod.orm.xml_mapper_registry;

export namespace cnetnexus::chart {

/**
 * @brief Loads the chart XML mappers and retains an immutable registry.
 *
 * The registry is published only after every configured document parsed
 * successfully, so request handlers never read files and never observe a
 * partially loaded mapper set.
 */
class chart_mapper_registry final : public cnetmod::application::managed_service
{
public:
    explicit chart_mapper_registry(
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

/** @brief 启动期加载 chart XML mapper 目录。
 *  @param registry 目标注册表
 *  @param directory mappers 目录
 *  @return 加载失败返回可直接打印的错误文本 */
[[nodiscard]] auto load_chart_mappers(cnetmod::orm::mapper_registry& registry,
    const std::filesystem::path& directory) -> std::expected<void, std::string>;

} // namespace cnetnexus::chart
