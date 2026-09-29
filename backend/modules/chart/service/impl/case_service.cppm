export module cnetnexus.chart.service.case_service;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;

export namespace cnetnexus::chart {

/** @brief 案例写入请求（新建与编辑共用）。
 *
 *  has_* 标志区分「未提供」与「显式清空」：编辑时未提供的字段保持原值，
 *  显式提供空串则清空为 NULL（前端把空值统一按空串发送）。
 */
struct case_write_request
{
    cnetnexus::chart::chart_case value; ///< 目标字段；id 在编辑时必填
    std::vector<std::uint64_t> tag_ids; ///< 标签 ID；has_tag_ids 为 false 时不动
    bool has_algorithm{false};          ///< 请求体是否提供了 algorithm（编辑时未提供则保持原值）
    bool has_tag_ids{false};            ///< 请求体是否提供了 tag_ids
    bool has_payload{false};            ///< 请求体是否提供了 payload 或 chart
    bool has_reading{false};            ///< 请求体是否提供了 reading
};

/** @brief Provides asynchronous case CRUD, tag management, and report extraction.
 *
 *  Both the case mapper and the archive service are injected coroutine mappers.
 *  The service neither accepts nor retains an execution context.
 */
class case_service
{
public:
    case_service(chart_case_mapper& cases, chart_archive_service& archives,
        cnetnexus::framework::audit_log_service& audit) noexcept
        : cases_{&cases},
          archives_{&archives},
          audit_{&audit}
    {
    }

    /** @brief 分页检索案例。 */
    [[nodiscard]] auto list(const cnetnexus::chart::case_query& query)
        -> cnetmod::task<std::expected<cnetnexus::framework::pagination::page_result<cnetnexus::chart::chart_case>,
            cnetnexus::framework::error::error_code>>;

    /** @brief 按 ID 查询；不存在返回 CASE.NOT_FOUND。 */
    [[nodiscard]] auto find(std::uint64_t id)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case,
            cnetnexus::framework::error::error_code>>;

    /** @brief 新建案例。 */
    [[nodiscard]] auto create(const case_write_request& request,
        const cnetnexus::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case,
            cnetnexus::framework::error::error_code>>;

    /** @brief 编辑案例；未提供的字段保持原值。 */
    [[nodiscard]] auto update(const case_write_request& request,
        const cnetnexus::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_case,
            cnetnexus::framework::error::error_code>>;

    /** @brief 逻辑删除案例。 */
    [[nodiscard]] auto remove(std::uint64_t id,
        const cnetnexus::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<void,
            cnetnexus::framework::error::error_code>>;

    /** @brief 断语生成后回写 reading 列（失败不影响本次响应）。 */
    [[nodiscard]] auto save_reading(std::uint64_t id, std::string reading)
        -> cnetmod::task<std::expected<void,
            cnetnexus::framework::error::error_code>>;

    /** @brief 取案例里存的盘面中文报告：payload 的 analysisZh/reportZh，缺失时退回存档快照。
     *  @return 命中返回报告文本；都没有返回 std::nullopt；存档读取失败返回错误 */
    [[nodiscard]] auto report_text(const cnetnexus::chart::chart_case& value)
        -> cnetmod::task<std::expected<std::optional<std::string>,
            cnetnexus::framework::error::error_code>>;

    /** @brief 全部标签。 */
    [[nodiscard]] auto list_tags()
        -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::chart_tag>,
            cnetnexus::framework::error::error_code>>;

    /** @brief 新建标签；同名返回 TAG.CONFLICT。 */
    [[nodiscard]] auto create_tag(std::string name, std::string color,
        std::uint64_t owner_id, const cnetnexus::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetnexus::chart::chart_tag,
            cnetnexus::framework::error::error_code>>;

    /** @brief 删除标签；不存在返回 TAG.NOT_FOUND。 */
    [[nodiscard]] auto remove_tag(std::uint64_t id,
        const cnetnexus::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<void,
            cnetnexus::framework::error::error_code>>;

private:
    chart_case_mapper* cases_;
    chart_archive_service* archives_;
    cnetnexus::framework::audit_log_service* audit_;
};

} // namespace cnetnexus::chart
