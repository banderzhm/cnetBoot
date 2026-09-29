module cnetnexus.chart.mapper.mysql_chart_case_mapper;

import std;
import cnetmod.core.time;
import cnetmod.coro;
import cnetmod.orm;
import cnetmod.application.data;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;

namespace cnetnexus::chart {
namespace {

namespace chart = cnetnexus::chart;
namespace db_api = cnetmod::database;
namespace orm_api = cnetmod::orm;
using error_code = cnetnexus::framework::error::error_code;


auto to_archive(const chart_archive_projection& row)
    -> chart::archive_index_entry
{
    return chart::archive_index_entry{.archive_id = row.archive_id,
        .algorithm = row.algorithm,
        .title = row.title,
        .owner_id = row.owner_id,
        .size_bytes = row.size_bytes,
        .created_at = row.created_unix};
}

/// @brief 构造 owner 分区参数（searchArchives / countArchives 共用）。
///
/// 口径与 ChartMapper.xml 的 <foreach> 一一对应：owner 谓词启用时列表必须**非空**，
/// 否则 <foreach> 会渲染出半截 SQL。因此这里在仓储边界归一：
///  - include_unowned 为真时先塞匿名分区 0；
///  - 结果为空时塞哨兵 k_unmatchable_owner（BIGINT 的最大值，自增主键永远取不到），
///    谓词退化成恒假而不会放行全表；
///  - 不作为谓词使用（owner_scope = 0）时列表保持为空，XML 会整体跳过该分支。
auto owner_scope_identifiers(const chart::archive_owner_filter& filter)
    -> std::string
{
    constexpr std::uint64_t unmatched_owner = std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> ids;
    if (filter.include_unowned)
        ids.push_back(chart::archive_access::anonymous_owner_id);
    ids.insert(ids.end(), filter.owner_ids.begin(), filter.owner_ids.end());
    std::ranges::sort(ids);
    const auto unique = std::ranges::unique(ids);
    ids.erase(unique.begin(), unique.end());
    if (filter.active && ids.empty())
        ids.push_back(unmatched_owner);

    std::string joined;
    for (const auto id : ids)
    {
        if (!joined.empty())
            joined.push_back(',');
        joined += std::to_string(id);
    }
    return joined;
}

/// @brief 构造存档检索参数（searchArchives / countArchives 共用）。
auto archive_params(const chart::archive_query& query,
    const cnetnexus::framework::pagination::page_request& page,
    const std::optional<db_api::calendar_datetime>& from_time,
    const std::optional<db_api::calendar_datetime>& to_time)
    -> orm_api::param_context
{
    const auto keyword_pattern = query.keyword.empty()
        ? std::string{}
        : std::string{"%"} + query.keyword + "%";
    return orm_api::param_context::from_map({
        {"algorithm", orm_api::param_value::from_string(query.algorithm)},
        {"keyword", orm_api::param_value::from_string(keyword_pattern)},
        // 时间窗入参在仓储边界换算成 UTC 墙钟 DATETIME，XML
        // 直接与列比较（无时区函数）。
        {"from_time", from_time ? orm_api::to_query_parameter(*from_time)
                                  : orm_api::param_value::null()},
        {"to_time", to_time ? orm_api::to_query_parameter(*to_time)
                              : orm_api::param_value::null()},
        {"limit", orm_api::param_value::from_uint(static_cast<std::uint64_t>(page.page_size))},
        {"offset", orm_api::param_value::from_uint(static_cast<std::uint64_t>((page.page - 1) * page.page_size))},
        // P0-1：owner 分区谓词。XML 侧用 owner_scope（0 = 不启用）/
        // owner_scope_include_unowned / <foreach> 的 owner_ids 表达，
        // 与 archive_owner_visible() 的内存语义逐条对齐。
        {"owner_scope", orm_api::param_value::from_uint(query.owner.active ? 1u : 0u)},
        {"owner_scope_ids", orm_api::param_value::from_string(
             owner_scope_identifiers(query.owner))},
    });
}

/// @brief searchArchives / countArchives 共用的参数上下文（含 <foreach> 集合）。
auto archive_context(const chart::archive_query& query,
    const cnetnexus::framework::pagination::page_request& page,
    const std::optional<db_api::calendar_datetime>& from_time,
    const std::optional<db_api::calendar_datetime>& to_time)
    -> orm_api::param_context
{
    auto context = archive_params(query, page, from_time, to_time);
    std::vector<orm_api::param_context> identifiers;
    if (query.owner.active)
    {
        // 与 owner_scope_identifiers() 同源顺序：仓储已经归一（去重、含匿名分区、
        // 空时哨兵），这里只做投影，保证 IN 列表与 owner_scope_ids 恒一致。
        constexpr std::uint64_t unmatched_owner =
            std::numeric_limits<std::uint64_t>::max();
        std::vector<std::uint64_t> ids;
        if (query.owner.include_unowned)
            ids.push_back(chart::archive_access::anonymous_owner_id);
        ids.insert(ids.end(), query.owner.owner_ids.begin(),
            query.owner.owner_ids.end());
        std::ranges::sort(ids);
        const auto unique = std::ranges::unique(ids);
        ids.erase(unique.begin(), unique.end());
        if (ids.empty())
            ids.push_back(unmatched_owner);
        identifiers.reserve(ids.size());
        for (const auto id : ids)
            identifiers.push_back(orm_api::param_context::from_map(
                {{"owner_scope_item", orm_api::param_value::from_uint(id)}}));
    }
    context.add_collection("owner_scope_id_list", std::move(identifiers));
    return context;
}


} // namespace

// ============================== chart_archive_index_mapper
// ==============================

mysql_chart_archive_mapper::mysql_chart_archive_mapper(
    cnetmod::application::managed_repository<chart_archive_record> archives,
    cnetmod::orm::mapper_registry& registry) noexcept
    : registry_{&registry}, archives_{std::move(archives)} {}

auto mysql_chart_archive_mapper::save(const chart::archive_snapshot& value)
    -> cnetmod::task<std::expected<void, error_code>>
{
    if (value.metadata.archive_id.empty() || value.payload.empty())
        co_return std::unexpected{error_code::request_invalid_argument};

    // INSERT ... ON DUPLICATE KEY UPDATE 是构建器表达不了的（insert
    // 撞主键、update 不建行）， 保留在 XML mapper；时间列以 UTC 墙钟 DATETIME
    // 参数绑定。
    const auto created_at = db_api::datetime_from_unix_seconds(
        value.metadata.created_at == 0 ? cnetmod::unix_time_seconds()
                                       : value.metadata.created_at);
    if (!created_at)
        co_return std::unexpected{error_code::request_invalid_argument};
    const auto inserted = co_await archives_.execute_xml(
        *registry_, "ChartMapper.upsertArchive",
        orm_api::param_context::from_map(
            {{"archive_id", orm_api::to_query_parameter(value.metadata.archive_id)},
                {"algorithm", orm_api::to_query_parameter(value.metadata.algorithm)},
                {"title", orm_api::to_query_parameter(value.metadata.title)},
                {"owner_id", orm_api::to_query_parameter(value.metadata.owner_id)},
                {"size_bytes", orm_api::to_query_parameter(value.metadata.size_bytes)},
                {"payload", orm_api::to_query_parameter(value.payload)},
                {"created_at", orm_api::to_query_parameter(*created_at)}}));
    if (inserted.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    co_return {};
}

auto mysql_chart_archive_mapper::load(std::string_view archive_id)
    -> cnetmod::task<std::expected<std::optional<chart::archive_snapshot>,
        error_code>>
{
    auto query = orm_api::query_wrapper<chart_archive_record>{};
    query.eq(&chart_archive_record::archive_id, std::string{archive_id});
    const auto found = co_await archives_.get_one(query);
    if (found.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    const auto row = found.first();
    if (!row)
        co_return std::optional<chart::archive_snapshot>{};

    chart::archive_snapshot value;
    value.metadata = chart::archive_index_entry{
        .archive_id = row->archive_id,
        .algorithm = row->algorithm,
        .title = row->title,
        .owner_id = row->owner_id,
        .size_bytes = row->size_bytes,
        .created_at = db_api::unix_seconds_from_datetime(row->created_at).value_or(0)};
    value.payload = row->payload;
    co_return std::optional<chart::archive_snapshot>{std::move(value)};
}

auto mysql_chart_archive_mapper::remove(std::string_view archive_id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const auto removed = co_await archives_.remove_by_id(
        orm_api::param_value::from_string(std::string{archive_id}));
    if (removed.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    co_return {};
}

auto mysql_chart_archive_mapper::find(std::string_view archive_id)
    -> cnetmod::task<std::expected<
        std::optional<chart::archive_index_entry>, error_code>>
{
    // 单表按主键读取：P0-1 的归属判定需要 owner_id，用 ORM 构建器表达，
    // 不新增 XML 语句（chart_mapper_registry 的必需清单因此保持不变）。
    auto query = orm_api::query_wrapper<chart_archive_record>{};
    query.eq(&chart_archive_record::archive_id, std::string{archive_id});
    const auto found = co_await archives_.get_one(query);
    if (found.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    const auto row = found.first();
    if (!row.has_value())
        co_return std::optional<chart::archive_index_entry>{};
    co_return std::optional<chart::archive_index_entry>{
        chart::archive_index_entry{.archive_id = row->archive_id,
            .algorithm = row->algorithm,
            .title = row->title,
            .owner_id = row->owner_id,
            .size_bytes = row->size_bytes,
            .created_at =
                db_api::unix_seconds_from_datetime(row->created_at)
                    .value_or(0)}};
}

auto mysql_chart_archive_mapper::search(const chart::archive_query& query_value)
    -> cnetmod::task<std::expected<
        cnetnexus::framework::pagination::page_result<chart::archive_index_entry>,
        error_code>>
{
    const auto from_time = query_value.from > 0
        ? db_api::datetime_from_unix_seconds(query_value.from)
        : std::nullopt;
    const auto to_time = query_value.to > 0
        ? db_api::datetime_from_unix_seconds(query_value.to)
        : std::nullopt;
    if ((query_value.from > 0 && !from_time) ||
        (query_value.to > 0 && !to_time))
        co_return std::unexpected{error_code::request_invalid_argument};
    const auto page = cnetnexus::framework::pagination::normalize(query_value.page);
    const auto parameters = archive_context(query_value, page, from_time, to_time);

    cnetnexus::framework::pagination::page_result<chart::archive_index_entry> result;
    result.page = page.page;
    result.page_size = page.page_size;

    // 术数 / 标题 / 时间窗 / owner 分区是动态 SQL，留在 XML mapper。
    const auto counted = co_await archives_.select_xml_as<
        chart_algorithm_count_projection>(*registry_, "ChartMapper.countArchives",
        parameters);
    if (counted.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    if (const auto total = counted.first())
        result.total = total->total;

    const auto rows = co_await archives_.select_xml_as<chart_archive_projection>(
        *registry_, "ChartMapper.searchArchives", parameters);
    if (rows.is_err())
        co_return std::unexpected{error_code::system_internal_error};
    result.items.reserve(rows.data.size());
    for (const auto& row : rows.data)
        result.items.push_back(to_archive(row));
    co_return result;
}

auto mysql_chart_archive_mapper::counts_by_algorithm() -> cnetmod::task<
    std::expected<std::vector<chart::algorithm_count>, error_code>>
{
    // GROUP BY 聚合留在 XML mapper（结果不是单表模型，用列别名投影解析）。
    const auto rows = co_await archives_.select_xml_as<
        chart_algorithm_count_projection>(*registry_,
        "ChartMapper.countsByAlgorithm", orm_api::param_context{});
    if (rows.is_err())
        co_return std::unexpected{error_code::system_internal_error};

    std::vector<chart::algorithm_count> result;
    result.reserve(rows.data.size());
    for (const auto& row : rows.data)
    {
        result.push_back(
            chart::algorithm_count{.algorithm = row.algorithm,
                .count = static_cast<std::uint64_t>(row.total)});
    }
    co_return result;
}

auto mysql_chart_archive_mapper::count_since(std::int64_t from, std::int64_t to)
    -> cnetmod::task<std::expected<std::uint64_t, error_code>>
{
    // 时间窗计数走 query_wrapper：created_at 比较的是 UTC 墙钟 DATETIME
    // 参数（与会话时区无关）。
    auto query = orm_api::query_wrapper<chart_archive_record>{};
    if (from != 0)
    {
        const auto start = db_api::datetime_from_unix_seconds(from);
        if (!start)
            co_return std::unexpected{error_code::request_invalid_argument};
        query.ge(&chart_archive_record::created_at, *start);
    }
    if (to != 0)
    {
        const auto end = db_api::datetime_from_unix_seconds(to);
        if (!end)
            co_return std::unexpected{error_code::request_invalid_argument};
        query.le(&chart_archive_record::created_at, *end);
    }
    const auto counted = co_await archives_.count(query);
    const auto total = counted.first();
    if (counted.is_err() || !total.has_value())
        co_return std::unexpected{error_code::system_internal_error};
    co_return static_cast<std::uint64_t>(*total);
}

// ============================== 聚合与 mapper 装载
// ==============================

mysql_chart_mappers::mysql_chart_mappers(
    cnetmod::application::managed_repository<chart_case_record> case_repository,
    cnetmod::application::managed_repository<chart_tag_record> tag_repository,
    cnetmod::application::managed_repository<chart_archive_record> archive_repository,
    cnetmod::orm::mapper_registry& registry) noexcept
    : cases{std::move(case_repository), std::move(tag_repository), registry},
      archives{std::move(archive_repository), registry} {}

} // namespace cnetnexus::chart
