export module cnetnexus.chart.mapper.chart_archive_repository;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.stats;
import cnetnexus.framework.pagination;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief 存档仓储接口（元数据与正文同行持久化）。
 *
 *  异步契约：方法均为 \c cnetmod::task<std::expected<...>> 协程。本端口是**数据面**
 *  端口（MySQL 表 / 进程内索引），不带 io_context 参数：MySQL 实现直接 co_await
 *  框架连接池（池建在宿主事件循环上，await 完自然回到原线程），内存实现是纯计算。
 *  契约与 cnetnexus.chart.mapper.user_count_mapper 一致。
 */
class chart_archive_repository
{
public:
    chart_archive_repository() = default;
    chart_archive_repository(const chart_archive_repository&) = delete;
    auto operator=(const chart_archive_repository&) -> chart_archive_repository& = delete;
    chart_archive_repository(chart_archive_repository&&) = delete;
    auto operator=(chart_archive_repository&&) -> chart_archive_repository& = delete;
    virtual ~chart_archive_repository() = default;

    /** @brief 原子写入或覆盖元数据与正文（同 archive_id 幂等）。 */
    [[nodiscard]] virtual auto save(const cnetnexus::chart::archive_snapshot& value)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按快照 ID 删除索引；不存在视为成功。 */
    [[nodiscard]] virtual auto remove(std::string_view archive_id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按快照 ID 取索引条目（含 owner_id）；不存在返回 std::nullopt。
     *
     *  归属只在索引里（快照正文是文件，不含 owner），因此 fetch / remove 的 P0-1
     *  归属判定都先走这里；读不到索引条目按「无归属信息」处理。
     */
    [[nodiscard]] virtual auto find(std::string_view archive_id)
        -> cnetmod::task<std::expected<
            std::optional<cnetnexus::chart::archive_index_entry>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按 ID 取回完整存档；不存在返回 nullopt。 */
    [[nodiscard]] virtual auto load(std::string_view archive_id)
        -> cnetmod::task<std::expected<
            std::optional<cnetnexus::chart::archive_snapshot>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按算法 / 关键字 / 时间范围分页检索，按创建时间倒序。
     *
     *  query.owner 启用时追加 owner 分区谓词（P0-1 的跨租户读隔离）。
     */
    [[nodiscard]] virtual auto search(const cnetnexus::chart::archive_query& query)
        -> cnetmod::task<std::expected<cnetnexus::framework::pagination::page_result<cnetnexus::chart::archive_index_entry>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 各术数的存档总数（只返回 count > 0 的项，按算法名升序）。 */
    [[nodiscard]] virtual auto counts_by_algorithm()
        -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::algorithm_count>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief [from, to] 闭区间内的新增存档数（UTC 秒；0 表示该端不过滤）。 */
    [[nodiscard]] virtual auto count_since(std::int64_t from, std::int64_t to)
        -> cnetmod::task<std::expected<std::uint64_t, cnetnexus::framework::error::error_code>> = 0;
};

} // namespace cnetnexus::chart
