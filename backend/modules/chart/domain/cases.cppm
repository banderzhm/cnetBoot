export module cnetnexus.chart.domain.cases;

import std;
import cnetnexus.framework.pagination;

export namespace cnetnexus::chart {

/** @brief 案例标签（chart_tag 表）。字段名与前端 types.ts 的 ChartTag 对齐。 */
struct chart_tag
{
    std::uint64_t id{};           ///< 主键
    std::string name;             ///< 标签名，(name, owner_id) 唯一
    std::string color{"#1677ff"}; ///< 展示色
    std::uint64_t owner_id{};     ///< 归属用户；0 表示共享
    std::int64_t created_at{};    ///< 创建时间（UTC 秒）
};

/** @brief 案例（chart_case 表）。字段名与前端 types.ts 的 ChartCase 对齐。 */
struct chart_case
{
    std::uint64_t id{};                    ///< 主键
    std::uint64_t owner_id{};              ///< 建档用户
    std::string title;                     ///< 标题（必填）
    std::string algorithm{"ba_zi"};        ///< 术数标识（规范值）
    std::optional<std::string> archive_id; ///< 关联存档 ID；无关联为 nullopt
    std::string calendar{"solar"};         ///< solar | lunar
    std::string gender;                    ///< 性别（口径随术数）
    int birth_year{};
    int birth_month{};
    int birth_day{};
    int birth_hour{};
    int birth_minute{};
    std::string birth_place;
    std::string client_name;
    std::string question;
    std::optional<std::string> notes;   ///< 笔记；空串由服务层归一成 nullopt
    std::optional<std::string> reading; ///< 断语（大模型解读）
    std::optional<std::string> payload; ///< 排盘 JSON 快照（字符串）
    std::int64_t created_at{};
    std::int64_t updated_at{};
    std::vector<std::uint64_t> tag_ids; ///< 关联标签 ID
    std::vector<chart_tag> tags;        ///< 关联标签详情（查询时装配）
};

/** @brief 案例列表过滤条件；空字段表示不过滤（契约 §5.4）。 */
struct case_query
{
    std::string keyword;                          ///< 标题 / 客户 / 问题模糊匹配
    std::string algorithm;                        ///< 术数标识（规范值）
    std::uint64_t tag_id{};                       ///< 按标签过滤；0 表示不过滤
    cnetnexus::framework::pagination::page_request page; ///< 分页参数
};

/** @brief 空串归一：空串返回 std::nullopt，否则原样（服务层用）。
 *  @note 定义在头内以便调用方内联，不引入额外链接依赖。
 */
[[nodiscard]] inline auto optional_text(std::string value) -> std::optional<std::string>
{
    if (value.empty())
        return std::nullopt;
    return value;
}

} // namespace cnetnexus::chart
