export module cnetnexus.chart.domain.archive;

import std;
import cnetnexus.framework.pagination;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief 排盘存档索引条目（chart_archive 表的元数据投影）。 */
struct archive_index_entry
{
    std::string archive_id;     ///< 16 位十六进制快照 ID
    std::string algorithm;      ///< 术数标识（规范值）
    std::string title;          ///< 记录标题
    std::uint64_t owner_id{};   ///< 归属用户；0 表示匿名写入
    std::uint64_t size_bytes{}; ///< 快照字节数
    std::int64_t created_at{};  ///< 写入时间（UTC 秒）
};

/** @brief 存档聚合：元数据与正文始终位于同一数据库行。 */
struct archive_snapshot
{
    archive_index_entry metadata;
    std::string payload;
};

/** @brief 存档检索的 owner 分区过滤（P0-1 跨租户越权修复）。
 *
 *  存储层按本结构追加 chart_archive_index.owner_id 谓词
 *  （ChartMapper.xml 的 archiveFilters / 内存实现）：
 *  - active == false：不加谓词（全量可见的调用方，或没有请求上下文）；
 *  - include_unowned：匿名分区（owner_id = 0）可见 —— 公开契约的写入记录；
 *  - owner_ids：当前用户 ID；部门 ID 不属于这个命名空间。
 *
 *  active 为真且两个条件都为空时表示「看不到任何行」：谓词退化成恒假，
 *  绝不会因为条件为空而放行全表（文件扫描兜底路径更是直接失败，不静默放宽）。
 */
struct archive_owner_filter
{
    bool active{false};                   ///< 是否启用 owner 谓词
    bool include_unowned{false};          ///< 是否包含匿名分区（owner_id = 0）
    std::vector<std::uint64_t> owner_ids; ///< 额外可见的 owner 分区
};

/** @brief 一次存档请求的归属上下文：谁在调用、能作用于哪些 owner 分区。
 *
 *  HTTP 适配层从请求身份与全量数据权限投影；部门 ID 不能作为用户 owner ID。
 *  service 层只按本结构判定，
 *  不 import 任何 HTTP / 身份实现。
 *
 *  匿名调用方（请求作用域里没有 current_user）owner_id 为 0，且**只能**看到匿名分区；
 *  已发布公开契约 GET /api/v1/archive/:algorithm/:id 保持免令牌，靠这条规则收敛
 *  「匿名可读所有人的记录」为「匿名只可读公共分区」。
 *
 *  enforce 标志说明（重要）：
 *  - enforce == true 是**唯一**会做归属判定 / 过滤的状态，由 archive_access_from()
 *    在 HTTP 入口设置；三个 archive 路由与 archive 写入路径都传参进去。
 *  - enforce == false（默认构造）表示「这次调用没有请求身份上下文」，只允许服务/模块
 *    内部复用（例如 case_service 读取案例自带的快照，其路由已由 case 权限守卫把关）。
 *    任何新增的 HTTP 入口都必须走 archive_access_from()，不要用默认值。
 *  「忘记传参」不会在这里静默放行真实用户数据：所有对外读/写/删入口都在
 *  archive_routes.cpp 里显式投影，并由 testing/chart_archive_ownership.cpp 断言
 *  匿名请求与普通用户请求各自拿到的判定结果。
 */
struct archive_access
{
    /// 匿名调用的归属值；与 archive_index_entry::owner_id 的「0 表示匿名」一致。
    static constexpr std::uint64_t anonymous_owner_id = 0;

    bool enforce{false};                        ///< 是否启用归属判定（请求上下文为 true）
    std::uint64_t owner_id{anonymous_owner_id}; ///< 调用方用户 ID；0 表示匿名
    bool authenticated{false};                  ///< 是否带有效令牌（可选鉴权）
    bool unrestricted{false};                   ///< 全量数据范围（管理员 / all）
    /** @brief 已认证且拥有全量数据范围（可作用于任意归属的记录）。 */
    [[nodiscard]] auto can_access_all() const noexcept -> bool;

    /** @brief 是否做归属判定：请求上下文里绑定了身份/数据范围时为 true。 */
    [[nodiscard]] auto scoped() const noexcept -> bool;

    /** @brief 读取：公共匿名分区、本人，或全量管理员。 */
    [[nodiscard]] auto may_read(std::uint64_t record_owner_id) const noexcept
        -> bool;

    /** @brief 删除：仅本人或全量管理员；公共可读不代表公共可删。 */
    [[nodiscard]] auto may_delete(std::uint64_t record_owner_id) const noexcept
        -> bool;

    /** @brief 越权时的统一错误码：未认证 401（AUTH.TOKEN_MISSING），已认证 403。 */
    [[nodiscard]] auto denial() const noexcept
        -> cnetnexus::framework::error::error_code;

    /** @brief 检索用的 owner 分区过滤；全量范围返回未启用（不做谓词）。 */
    [[nodiscard]] auto filter() const -> cnetnexus::chart::archive_owner_filter;
};

/** @brief 存档检索条件（契约 §5.3）。 */
struct archive_query
{
    std::string algorithm;                        ///< 术数标识（规范值）；空表示全部
    std::string keyword;                          ///< 标题模糊匹配
    std::int64_t from{};                          ///< 起始时间（UTC 秒）；0 表示不过滤
    std::int64_t to{};                            ///< 结束时间（UTC 秒）；0 表示不过滤
    cnetnexus::chart::archive_owner_filter owner; ///< 归属分区过滤（P0-1）
    cnetnexus::framework::pagination::page_request page; ///< 分页参数
};

} // namespace cnetnexus::chart
