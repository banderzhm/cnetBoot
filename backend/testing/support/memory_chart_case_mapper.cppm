export module cnetnexus.chart.mapper.memory_chart_case_mapper;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.stats;
import cnetnexus.framework.pagination;
import cnetnexus.framework.error;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;

export namespace cnetnexus::chart {

/** @brief owner 分区可见性判定（内存检索与文件扫描共用，口径同 ChartMapper.xml）。
 *
 *  P0-1：filter.active 为真且 owner_ids / include_unowned 都为空时返回 false ——
 *  「除自己以外都不可见」不能退化成「全表可见」。
 */
[[nodiscard]] auto archive_owner_visible(const chart::archive_owner_filter& filter,
    std::uint64_t owner_id) -> bool;

} // namespace cnetnexus::chart

// mapper 方法已协程化：临界区内不存在挂起点，锁不跨 co_await；方法只是把同步逻辑包成协程。
// 即 co_return 直出原同步返回值，因此继续用 std::scoped_lock 保护进程内容器；对象管理器与内部 helper 保持同步不变。

export namespace cnetnexus::chart {

/** @brief 进程内案例 / 标签 / 存档索引数据（storage.backend=memory）。
 *
 *  与身份数据一样用一个互斥量保护全部容器；成员公开仅供同模块的仓储适配器访问。
 *  语义与 MySQL 实现逐条对齐：唯一性、分页归一化、时间倒序、标签关联覆盖。
 */
struct chart_database
{
    mutable std::mutex mutex_;                                          ///< 保护下列全部容器
    std::uint64_t next_case_id_{1};                                     ///< 案例主键序号
    std::uint64_t next_tag_id_{1};                                      ///< 标签主键序号
    std::vector<cnetnexus::chart::chart_case> cases_;             ///< 案例
    std::vector<cnetnexus::chart::chart_tag> tags_;               ///< 标签
    std::map<std::uint64_t, std::vector<std::uint64_t>> case_tags_;     ///< 案例 -> 标签
    std::vector<cnetnexus::chart::archive_snapshot> archives_; ///< 完整存档
};

/** @brief 案例与标签仓储的内存实现。 */
class memory_chart_case_mapper final : public cnetnexus::chart::chart_case_mapper
{
public:
    explicit memory_chart_case_mapper(chart_database& database) noexcept
        : db_{&database}
    {
    }

    [[nodiscard]] auto create(const cnetnexus::chart::chart_case& value)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto find_by_id(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<cnetnexus::chart::chart_case>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto update(const cnetnexus::chart::chart_case& value, bool replace_tags)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto remove(std::uint64_t id) -> cnetmod::task<std::expected<void,
        cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto list(const cnetnexus::chart::case_query& query)
        -> cnetmod::task<std::expected<cnetnexus::framework::pagination::page_result<cnetnexus::chart::chart_case>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto count() -> cnetmod::task<std::expected<std::uint64_t, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto set_tags(std::uint64_t case_id, std::span<const std::uint64_t> tag_ids)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto list_tags()
        -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::chart_tag>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto find_tag(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<cnetnexus::chart::chart_tag>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto create_tag(const cnetnexus::chart::chart_tag& value)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_tag, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto remove_tag(std::uint64_t id) -> cnetmod::task<std::expected<void,
        cnetnexus::framework::error::error_code>> override;

private:
    chart_database* db_;
};

/** @brief 存档索引的内存实现（进程内数组，语义等同 chart_archive_index 表）。 */
class memory_chart_archive_mapper final : public cnetnexus::chart::chart_archive_repository
{
public:
    explicit memory_chart_archive_mapper(chart_database& database) noexcept
        : db_{&database}
    {
    }

    [[nodiscard]] auto save(const cnetnexus::chart::archive_snapshot& value)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto remove(std::string_view archive_id) -> cnetmod::task<std::expected<void,
        cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto find(std::string_view archive_id)
        -> cnetmod::task<std::expected<
            std::optional<cnetnexus::chart::archive_index_entry>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto load(std::string_view archive_id)
        -> cnetmod::task<std::expected<
            std::optional<cnetnexus::chart::archive_snapshot>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto search(const cnetnexus::chart::archive_query& query)
        -> cnetmod::task<std::expected<
            cnetnexus::framework::pagination::page_result<cnetnexus::chart::archive_index_entry>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto counts_by_algorithm()
        -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::algorithm_count>,
            cnetnexus::framework::error::error_code>> override;
    [[nodiscard]] auto count_since(std::int64_t from, std::int64_t to)
        -> cnetmod::task<std::expected<std::uint64_t, cnetnexus::framework::error::error_code>> override;

private:
    chart_database* db_;
};

} // namespace cnetnexus::chart
