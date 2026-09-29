export module cnetnexus.chart.mapper.chart_case_mapper;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.cases;
import cnetnexus.framework.pagination;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief 案例与标签仓储接口（chart_case / chart_tag / chart_case_tag 三张表）。
 *
 *  案例与标签是同一聚合（标签只服务于案例分类），因此合并为一个 Port；
 *  实现必须保证：
 *  - 标题、algorithm 之外的可写字段逐条落库，未提供的字段保持原值（update 走覆盖语义）；
 *  - 逻辑删除：所有读取一律排除 deleted_at 非空的行；
 *  - 标签唯一性由存储保证（(name, owner_id) 唯一键 → TAG.CONFLICT）。
 *  All operations are coroutine-native. Execution dependencies are injected
 *  into implementations during composition; this public contract never
 *  exposes a raw io_context and callers must not use sync_wait.
 */
class chart_case_mapper
{
public:
    chart_case_mapper() = default;
    chart_case_mapper(const chart_case_mapper&) = delete;
    auto operator=(const chart_case_mapper&) -> chart_case_mapper& = delete;
    chart_case_mapper(chart_case_mapper&&) = delete;
    auto operator=(chart_case_mapper&&) -> chart_case_mapper& = delete;
    virtual ~chart_case_mapper() = default;

    /** @brief 新建案例；id 与时间戳由实现补齐，tag_ids 在同一事务里一并落库。
     *  @return 建档后的案例（已装配 tags）；任一标签不存在返回 TAG.NOT_FOUND（整体回滚）
     */
    [[nodiscard]] virtual auto create(const cnetnexus::chart::chart_case& value)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按主键查询（含 tags）；不存在返回 std::nullopt。 */
    [[nodiscard]] virtual auto find_by_id(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<cnetnexus::chart::chart_case>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 覆盖更新；replace_tags 为真时在同一事务里用 value.tag_ids 覆盖标签集合。
     *  @return 不存在返回 CASE.NOT_FOUND；任一标签不存在返回 TAG.NOT_FOUND（整体回滚）
     */
    [[nodiscard]] virtual auto update(const cnetnexus::chart::chart_case& value,
        bool replace_tags)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 逻辑删除；不存在返回 CASE.NOT_FOUND。 */
    [[nodiscard]] virtual auto remove(std::uint64_t id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 分页检索；items 已装配 tags。 */
    [[nodiscard]] virtual auto list(const cnetnexus::chart::case_query& query)
        -> cnetmod::task<std::expected<cnetnexus::framework::pagination::page_result<cnetnexus::chart::chart_case>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 未删除的案例总数。 */
    [[nodiscard]] virtual auto count()
        -> cnetmod::task<std::expected<std::uint64_t, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 覆盖案例的标签集合；任一标签不存在返回 TAG.NOT_FOUND。 */
    [[nodiscard]] virtual auto set_tags(std::uint64_t case_id,
        std::span<const std::uint64_t> tag_ids) -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 全部标签，按 id 升序。 */
    [[nodiscard]] virtual auto list_tags()
        -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::chart_tag>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 按主键查询标签；不存在返回 std::nullopt。 */
    [[nodiscard]] virtual auto find_tag(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<cnetnexus::chart::chart_tag>,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 新建标签；同名同归属返回 TAG.CONFLICT。 */
    [[nodiscard]] virtual auto create_tag(const cnetnexus::chart::chart_tag& value)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_tag, cnetnexus::framework::error::error_code>> = 0;

    /** @brief 删除标签及其案例关联；不存在返回 TAG.NOT_FOUND。 */
    [[nodiscard]] virtual auto remove_tag(std::uint64_t id)
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>> = 0;
};

} // namespace cnetnexus::chart
