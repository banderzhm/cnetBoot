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

// ===================== 列别名投影 → 领域对象（按列名映射，取代手写下标）
// =====================

auto read_case(const chart_case_projection &row) -> chart::chart_case {
  chart::chart_case value;
  value.id = row.id;
  value.owner_id = row.owner_id;
  value.title = row.title;
  value.algorithm = row.algorithm;
  value.archive_id = row.archive_id;
  value.calendar = row.calendar;
  value.gender = row.gender;
  value.birth_year = static_cast<int>(row.birth_year);
  value.birth_month = static_cast<int>(row.birth_month);
  value.birth_day = static_cast<int>(row.birth_day);
  value.birth_hour = static_cast<int>(row.birth_hour);
  value.birth_minute = static_cast<int>(row.birth_minute);
  value.birth_place = row.birth_place;
  value.client_name = row.client_name;
  value.question = row.question;
  value.notes = row.notes;
  value.reading = row.reading;
  value.payload = row.payload;
  value.created_at = row.created_unix;
  value.updated_at = row.updated_unix;
  return value;
}

/// @brief ORM 模型 → 领域对象（查询走 ORM 时用；时间列按 UTC 墙钟还原）。
auto to_case(const chart_case_record &row) -> chart::chart_case {
  chart::chart_case value;
  value.id = row.id;
  value.owner_id = row.owner_id;
  value.title = row.title;
  value.algorithm = row.algorithm;
  value.archive_id = row.archive_id;
  value.calendar = row.calendar;
  value.gender = row.gender;
  value.birth_year = static_cast<int>(row.birth_year);
  value.birth_month = static_cast<int>(row.birth_month);
  value.birth_day = static_cast<int>(row.birth_day);
  value.birth_hour = static_cast<int>(row.birth_hour);
  value.birth_minute = static_cast<int>(row.birth_minute);
  value.birth_place = row.birth_place;
  value.client_name = row.client_name;
  value.question = row.question;
  value.notes = row.notes;
  value.reading = row.reading;
  value.payload = row.payload;
  value.created_at =
      db_api::unix_seconds_from_datetime(row.created_at).value_or(0);
  value.updated_at =
      db_api::unix_seconds_from_datetime(row.updated_at).value_or(0);
  return value;
}

/// @brief 构造案例检索参数（selectCases / countCases 共用）。
auto case_params(const chart::case_query &query,
                 const cnetnexus::framework::pagination::page_request &page)
    -> orm_api::param_context {
  const auto keyword_pattern = query.keyword.empty()
                                   ? std::string{}
                                   : std::string{"%"} + query.keyword + "%";
  return orm_api::param_context::from_map({
      {"case_id", orm_api::param_value::from_uint(0)},
      {"algorithm", orm_api::param_value::from_string(query.algorithm)},
      {"keyword", orm_api::param_value::from_string(keyword_pattern)},
      {"tag_id", orm_api::param_value::from_uint(query.tag_id)},
      {"limit", orm_api::param_value::from_uint(
                    static_cast<std::uint64_t>(page.page_size))},
      {"offset", orm_api::param_value::from_uint(static_cast<std::uint64_t>(
                     (page.page - 1) * page.page_size))},
  });
}

} // namespace

// ============================== chart_case_mapper
// ==============================

mysql_chart_case_mapper::mysql_chart_case_mapper(
    cnetmod::application::managed_repository<chart_case_record> cases,
    cnetmod::application::managed_repository<chart_tag_record> tags,
    cnetmod::orm::mapper_registry &registry) noexcept
    : registry_{&registry}, cases_{std::move(cases)}, tags_{std::move(tags)} {}

auto mysql_chart_case_mapper::create(const chart::chart_case &value)
    -> cnetmod::task<std::expected<chart::chart_case, error_code>> {
  if (value.title.empty())
    co_return std::unexpected{error_code::request_invalid_argument};

  // 单表插入走
  // ORM：列清单来自模型元数据，值按成员类型编码为参数（无下标、无手写 SQL）。
  chart_case_record record;
  record.owner_id = value.owner_id;
  record.title = value.title;
  record.algorithm = value.algorithm;
  record.archive_id = value.archive_id;
  record.calendar = value.calendar;
  record.gender = value.gender;
  record.birth_year = value.birth_year;
  record.birth_month = value.birth_month;
  record.birth_day = value.birth_day;
  record.birth_hour = value.birth_hour;
  record.birth_minute = value.birth_minute;
  record.birth_place = value.birth_place;
  record.client_name = value.client_name;
  record.question = value.question;
  record.notes = value.notes;
  record.reading = value.reading;
  record.payload = value.payload;
  record.created_by = value.owner_id;
  record.updated_by = value.owner_id;
  const auto accessible_tags = co_await validate_tag_access(value.tag_ids);
  if (!accessible_tags)
    co_return std::unexpected{accessible_tags.error()};
  // 案例与标签关联在同一笔事务里落库：标签校验失败时案例一并回滚，
  // 客户端重试不会产生重复案例。
  error_code failure = error_code::system_internal_error;
  const auto inserted = co_await cases_.transaction<std::uint64_t>(
      [&record, tag_ids = value.tag_ids, &failure](auto &unit)
          -> cnetmod::task<std::expected<std::uint64_t, std::string>> {
        const auto saved =
            co_await unit.template mapper<chart_case_record>().insert(record);
        if (saved.is_err())
          co_return std::unexpected{std::string{"case insert failed"}};
        if (!tag_ids.empty()) {
          auto linked = co_await detail::replace_case_tags(unit, record.id,
                                                           tag_ids, failure);
          if (!linked)
            co_return std::unexpected{std::move(linked.error())};
        }
        co_return record.id;
      });
  if (!inserted)
    co_return std::unexpected{failure};

  const auto created = co_await find_by_id(*inserted);
  if (!created)
    co_return std::unexpected{created.error()};
  if (!created->has_value())
    co_return std::unexpected{error_code::system_internal_error};
  co_return **created;
}

auto mysql_chart_case_mapper::find_by_id(std::uint64_t id) -> cnetmod::task<
    std::expected<std::optional<chart::chart_case>, error_code>> {
  // 主键 + 逻辑删除条件 + LIMIT 1：流式查询用 query_wrapper（成员指针 →
  // 列名）。
  auto query = orm_api::query_wrapper<chart_case_record>{};
  query.eq(&chart_case_record::id, id).limit(1);
  const auto found = co_await cases_.list(query);
  if (found.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  if (found.data.empty())
    co_return std::optional<chart::chart_case>{};

  std::vector<chart::chart_case> cases;
  cases.push_back(to_case(found.data.front()));
  const auto attached = co_await attach_tags(cases);
  if (!attached)
    co_return std::unexpected{attached.error()};
  co_return std::optional<chart::chart_case>{std::move(cases.front())};
}

auto mysql_chart_case_mapper::update(const chart::chart_case &value,
                                     bool replace_tags)
    -> cnetmod::task<std::expected<chart::chart_case, error_code>> {
  if (replace_tags) {
    const auto accessible_tags = co_await validate_tag_access(value.tag_ids);
    if (!accessible_tags)
      co_return std::unexpected{accessible_tags.error()};
  }
  // 选择性更新走 update_wrapper：只覆盖调用方声明的列，条件带逻辑删除守卫。
  orm_api::update_wrapper<chart_case_record> update;
  update.set(&chart_case_record::title, value.title)
      .set(&chart_case_record::archive_id, value.archive_id)
      .set(&chart_case_record::calendar, value.calendar)
      .set(&chart_case_record::gender, value.gender)
      .set(&chart_case_record::birth_year,
           static_cast<std::int64_t>(value.birth_year))
      .set(&chart_case_record::birth_month,
           static_cast<std::int64_t>(value.birth_month))
      .set(&chart_case_record::birth_day,
           static_cast<std::int64_t>(value.birth_day))
      .set(&chart_case_record::birth_hour,
           static_cast<std::int64_t>(value.birth_hour))
      .set(&chart_case_record::birth_minute,
           static_cast<std::int64_t>(value.birth_minute))
      .set(&chart_case_record::birth_place, value.birth_place)
      .set(&chart_case_record::client_name, value.client_name)
      .set(&chart_case_record::question, value.question)
      .set(&chart_case_record::notes, value.notes)
      .set(&chart_case_record::reading, value.reading)
      .set(&chart_case_record::payload, value.payload)
      .set(&chart_case_record::updated_by, value.owner_id)
      .eq(&chart_case_record::id, value.id)
      .is_null(&chart_case_record::deleted_at);
  // 主表更新与标签覆盖同一笔事务：任一步失败整体回滚。
  error_code failure = error_code::system_internal_error;
  const auto updated = co_await cases_.transaction<void>(
      [&update, &value, replace_tags, &failure](
          auto &unit) -> cnetmod::task<std::expected<void, std::string>> {
        const auto written =
            co_await unit.template mapper<chart_case_record>().update(update);
        if (written.is_err())
          co_return std::unexpected{std::string{"case update failed"}};
        if (!replace_tags)
          co_return std::expected<void, std::string>{};
        co_return co_await detail::replace_case_tags(unit, value.id,
                                                     value.tag_ids, failure);
      });
  if (!updated)
    co_return std::unexpected{failure};

  // affected_rows == 0 可能只是「值没变」，因此用一次存在性查询区分。
  const auto current = co_await find_by_id(value.id);
  if (!current)
    co_return std::unexpected{current.error()};
  if (!current->has_value())
    co_return std::unexpected{error_code::case_not_found};
  co_return **current;
}

auto mysql_chart_case_mapper::remove(std::uint64_t id)
    -> cnetmod::task<std::expected<void, error_code>> {
  const auto now = cnetmod::unix_time_seconds();
  const auto deleted_at = db_api::datetime_from_unix_seconds(now);
  if (!deleted_at)
    co_return std::unexpected{error_code::request_invalid_argument};

  // 逻辑删除：软删标记是可空 DATETIME，用 update_wrapper 置
  // deleted_at/updated_at。
  orm_api::update_wrapper<chart_case_record> soft_delete;
  soft_delete.set(&chart_case_record::deleted_at, *deleted_at)
      .set(&chart_case_record::updated_at, *deleted_at)
      .eq(&chart_case_record::id, id)
      .is_null(&chart_case_record::deleted_at);
  const auto removed = co_await cases_.update(soft_delete);
  if (removed.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  if (removed.affected_rows == 0)
    co_return std::unexpected{error_code::case_not_found};
  co_return {};
}

auto mysql_chart_case_mapper::list(const chart::case_query &query_value)
    -> cnetmod::task<
        std::expected<cnetnexus::framework::pagination::page_result<chart::chart_case>,
                      error_code>> {
  const auto page = cnetnexus::framework::pagination::normalize(query_value.page);

  cnetnexus::framework::pagination::page_result<chart::chart_case> result;
  result.page = page.page;
  result.page_size = page.page_size;

  // 关键词 / 术数 / 标签 EXISTS + 共享过滤片段的动态检索留在 XML
  // mapper（构建器表达不了
  // 子查询与共享片段），但结果按**列名**映射回投影模型，不再手写下标。
  const auto counted =
      co_await cases_.select_xml_as<chart_algorithm_count_projection>(
          *registry_, "ChartMapper.countCases", case_params(query_value, page));
  if (counted.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  if (const auto total = counted.first())
    result.total = total->total;

  const auto rows = co_await cases_.select_xml_as<chart_case_projection>(
      *registry_, "ChartMapper.selectCases", case_params(query_value, page));
  if (rows.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  result.items.reserve(rows.data.size());
  for (const auto &row : rows.data)
    result.items.push_back(read_case(row));

  const auto attached = co_await attach_tags(result.items);
  if (!attached)
    co_return std::unexpected{attached.error()};
  co_return result;
}

auto mysql_chart_case_mapper::count()
    -> cnetmod::task<std::expected<std::uint64_t, error_code>> {
  // COUNT 走 query_wrapper（列名由模型解析，条件带逻辑删除）。
  auto query = orm_api::query_wrapper<chart_case_record>{};
  const auto counted = co_await cases_.count(query);
  const auto total = counted.first();
  if (counted.is_err() || !total.has_value())
    co_return std::unexpected{error_code::system_internal_error};
  co_return static_cast<std::uint64_t>(*total);
}

} // namespace cnetnexus::chart
