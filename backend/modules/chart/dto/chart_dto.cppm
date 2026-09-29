module;

#include <cnetmod/json_codec.hpp>

/**
 * @brief Typed HTTP projections for the chart bounded context.
 *
 * 字段名与前端 types.ts 逐字对齐：迁移时只改命名空间与序列化实现，
 * 不改任何对外键名与取值口径。
 */
export module cnetnexus.chart.dto.chart;

import std;
import cnetmod.json;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.stats;
import cnetnexus.framework.pagination;

export namespace cnetnexus::chart::dto {

/** @brief 把 DTO 的可选 raw JSON 标量解码为强类型值。 */
template <typename T>
[[nodiscard]] auto scalar_of(const std::optional<cnetmod::json::raw_value>& raw)
    -> std::optional<T>
{
    if (!raw || raw->is_null())
        return std::nullopt;
    auto parsed = cnetmod::json::from_document<T>(*raw);
    if (!parsed)
        return std::nullopt;
    return std::move(*parsed);
}

/** @brief 存档索引条目响应。 */
struct archive_index_response
{
    std::string archive_id;
    std::string algorithm;
    std::string title;
    std::uint64_t owner_id{};
    std::uint64_t size_bytes{};
    std::string created_at;
};

/** @brief 写入存档时落盘的文档：算法 + 保存时刻 + 盘面快照。 */
struct archive_snapshot_document
{
    std::string algorithm;
    std::int64_t savedAt{};
    cnetmod::json::value snapshot;
};

/** @brief 新建存档的响应。 */
struct archive_created_response
{
    std::string id;
    std::string algorithm;
};

/** @brief 通用成功响应体。 */
struct success_response
{
    bool success{true};
};

/** @brief 批量排盘的单条请求；字段一律用 raw_value 接住，由 transport 逐项校验。 */
struct chart_batch_item_request
{
    std::optional<cnetmod::json::raw_value> title;
    std::optional<cnetmod::json::raw_value> calendar;
    std::optional<cnetmod::json::raw_value> gender;
    std::optional<cnetmod::json::raw_value> year;
    std::optional<cnetmod::json::raw_value> month;
    std::optional<cnetmod::json::raw_value> day;
    std::optional<cnetmod::json::raw_value> hour;
    std::optional<cnetmod::json::raw_value> minute;
    std::optional<cnetmod::json::raw_value> question;
    std::optional<cnetmod::json::raw_value> kind;
    std::optional<cnetmod::json::raw_value> code;
    std::optional<cnetmod::json::raw_value> changing;
};

/** @brief 批量排盘请求体。 */
struct chart_batch_request
{
    std::optional<std::string> algorithm;
    std::optional<bool> save_archive;
    std::optional<std::vector<chart_batch_item_request>> items;
};

/** @brief 批量排盘勾选落存档时的文档。 */
struct chart_batch_archive_document
{
    std::string algorithm;
    std::int64_t savedAt{};
    std::string title;
    cnetmod::json::value snapshot;
};

/** @brief 批量条目的稳定错误投影；不依赖 HTTP 响应实现。 */
struct batch_error_response
{
    std::string code;
    std::string status;
    std::string message;
    bool retryable{};
};

/** @brief 批量排盘的单条响应；chart / error 互斥，null 也必须显式出现。 */
struct chart_batch_item_response
{
    std::size_t index{};
    std::string title;
    bool ok{};
    std::string algorithm;
    std::optional<std::string> archive_id;
    std::optional<cnetmod::json::value> chart;
    std::optional<batch_error_response> error;
};

/** @brief 批量排盘响应。 */
struct chart_batch_response
{
    std::vector<chart_batch_item_response> items;
    std::size_t succeeded{};
    std::size_t failed{};
};

/** @brief 案例写入请求体（新建与编辑共用）。 */
struct chart_case_request
{
    std::optional<std::string> title;
    std::optional<std::string> algorithm;
    std::optional<cnetmod::json::raw_value> archive_id;
    std::optional<std::string> calendar;
    std::optional<std::string> gender;
    std::optional<int> birth_year;
    std::optional<int> birth_month;
    std::optional<int> birth_day;
    std::optional<int> birth_hour;
    std::optional<int> birth_minute;
    std::optional<std::string> birth_place;
    std::optional<std::string> client_name;
    std::optional<std::string> question;
    std::optional<cnetmod::json::raw_value> notes;
    std::optional<cnetmod::json::raw_value> reading;
    std::optional<cnetmod::json::raw_value> payload;
    std::optional<cnetmod::json::raw_value> chart;
    std::optional<cnetmod::json::raw_value> tag_ids;
};

/** @brief 标签写入请求体。 */
struct chart_tag_request
{
    std::optional<std::string> name;
    std::optional<std::string> color;
};

/** @brief 标签响应。 */
struct chart_tag_response
{
    std::uint64_t id{};
    std::string name;
    std::string color;
    std::uint64_t owner_id{};
    std::string created_at;
};

/** @brief 案例响应。 */
struct chart_case_response
{
    std::uint64_t id{};
    std::uint64_t owner_id{};
    std::string title;
    std::string algorithm;
    std::optional<std::string> archive_id;
    std::string calendar;
    std::string gender;
    int birth_year{};
    int birth_month{};
    int birth_day{};
    int birth_hour{};
    int birth_minute{};
    std::string birth_place;
    std::string client_name;
    std::string question;
    std::optional<std::string> notes;
    std::optional<std::string> reading;
    std::optional<std::string> payload;
    std::string created_at;
    std::string updated_at;
    std::vector<std::uint64_t> tag_ids;
    std::vector<chart_tag_response> tags;
};

/** @brief 单个术数的计数响应。 */
struct algorithm_count_response
{
    std::string algorithm;
    std::uint64_t count{};
};

/** @brief 各术数存档数。 */
struct archive_counts_response
{
    std::uint64_t ba_zi{};
    std::uint64_t liu_yao{};
    std::uint64_t mei_hua{};
    std::uint64_t zi_wei{};
    std::uint64_t da_liu_ren{};
    std::uint64_t qi_men{};
};

/** @brief 今日统计。 */
struct today_stats_response
{
    std::uint64_t logins{};
    std::uint64_t charts{};
};

/** @brief 概览统计响应。 */
struct stats_overview_response
{
    std::uint64_t users{};
    std::uint64_t cases{};
    archive_counts_response archives;
    today_stats_response today;
    std::vector<algorithm_count_response> top_algorithms;
};

/** @brief 分页响应体（items / total / page / page_size）。 */
template <typename T> struct chart_page_response
{
    std::vector<T> items;
    std::int64_t total{};
    int page{};
    int page_size{};
};

[[nodiscard]] auto project(const cnetnexus::chart::archive_index_entry& value)
    -> archive_index_response;
[[nodiscard]] auto project(const cnetnexus::chart::chart_tag& value)
    -> chart_tag_response;
[[nodiscard]] auto project(const cnetnexus::chart::chart_case& value)
    -> chart_case_response;
[[nodiscard]] auto project(const cnetnexus::chart::stats_overview& value)
    -> stats_overview_response;

/** @brief 把分页结果投影成分页响应；投影函数由调用方提供。 */
template <typename Entity, typename Projection>
[[nodiscard]] auto project_chart_page(
    const cnetnexus::framework::pagination::page_result<Entity>& page,
    Projection&& projection)
    -> chart_page_response<std::invoke_result_t<Projection, const Entity&>>
{
    using result_type = std::invoke_result_t<Projection, const Entity&>;
    chart_page_response<result_type> result;
    result.items.reserve(page.items.size());
    for (const auto& item : page.items)
        result.items.push_back(std::invoke(projection, item));
    result.total = page.total;
    result.page = page.page;
    result.page_size = page.page_size;
    return result;
}

/** @brief 序列化投影结果；字段为 nullopt 时不输出该键（与框架默认写入口径一致）。 */
[[nodiscard]] auto to_document(const archive_index_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const chart_tag_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const chart_case_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const stats_overview_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const archive_created_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const success_response& value)
    -> cnetmod::json::document;

/** @brief 序列化分页投影；元素转换由调用方提供（保持模板实例化在本模块内）。 */
template <typename T, typename ToDocument>
[[nodiscard]] auto page_document(const chart_page_response<T>& value,
    ToDocument&& to_document_element) -> cnetmod::json::document
{
    auto rows = cnetmod::json::array();
    rows.get_array().reserve(value.items.size());
    for (const auto& item : value.items)
        rows.get_array().push_back(std::invoke(to_document_element, item));
    return cnetmod::json::object({{"items", std::move(rows)},
        {"total", value.total}, {"page", value.page},
        {"pageSize", value.page_size}});
}

/**
 * @brief 序列化批量排盘响应。
 *
 * 契约要求 chart / error / archiveId 三个成员**显式出现**（命中为值、未命中为 null），
 * 由本函数手工构造文档，不依赖「varint 默认省略 null」的写入策略。
 */
[[nodiscard]] auto to_document(const chart_batch_response& value)
    -> cnetmod::json::document;

} // namespace cnetnexus::chart::dto
