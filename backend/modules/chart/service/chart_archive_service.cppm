export module cnetnexus.chart.service.chart_archive_service;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.archive;
import cnetnexus.framework.pagination;
import cnetnexus.framework.error;
import cnetnexus.chart.mapper.chart_archive_repository;

export namespace cnetnexus::chart {

/** @brief 排盘存档用例；正文和元数据由单一仓储原子持久化。 */
class chart_archive_service
{
public:
    explicit chart_archive_service(
        cnetnexus::chart::chart_archive_repository& archives) noexcept
        : archives_{&archives}
    {
    }

    /** @brief 保存快照并登记索引。
     *  @param algorithm 术数标识（规范值）
     *  @param snapshot 快照 JSON 文本
     *  @param title 记录标题（写入索引，供检索展示）
     *  @param owner_id 归属用户；0 表示匿名写入
     *  @return 新生成的 16 位十六进制 ID
     */
    [[nodiscard]] auto archive(std::string_view algorithm, std::string snapshot,
        std::string title = {}, std::uint64_t owner_id = 0)
        -> cnetmod::task<std::expected<std::string, cnetnexus::framework::error::error_code>>;

    /** @brief 按 ID 取回快照；不存在返回 std::nullopt。
     *  @param access 调用方归属上下文；请求路径无索引时拒绝，内部调用可复用文件仓储
     *  @return 命中返回快照文本；不存在返回 nullopt；无权访问返回 AUTH.FORBIDDEN
     */
    [[nodiscard]] auto fetch(std::string_view algorithm, std::string_view id,
        const cnetnexus::chart::archive_access& access =
            cnetnexus::chart::archive_access{})
        -> cnetmod::task<std::expected<std::optional<std::string>, cnetnexus::framework::error::error_code>>;

    /** @brief 检索存档索引（分页）；未装配索引时退回文件扫描并在本地切页。
     *  @param access 调用方可见范围：请求检索只返回公共分区和本人记录
     */
    [[nodiscard]] auto search(const cnetnexus::chart::archive_query& query,
        const cnetnexus::chart::archive_access& access =
            cnetnexus::chart::archive_access{})
        -> cnetmod::task<std::expected<
            cnetnexus::framework::pagination::page_result<cnetnexus::chart::archive_index_entry>,
            cnetnexus::framework::error::error_code>>;

    /** @brief 删除快照与索引；快照不存在返回 ARCHIVE.NOT_FOUND。
     *  @param caller 调用方归属上下文
     *  @return 他人记录返回 AUTH.FORBIDDEN；删除失败原样传播错误码
     */
    [[nodiscard]] auto remove(std::string_view algorithm, std::string_view id,
        const cnetnexus::chart::archive_access& caller =
            cnetnexus::chart::archive_access{})
        -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>;

private:
    cnetnexus::chart::chart_archive_repository* archives_;
};

} // namespace cnetnexus::chart
