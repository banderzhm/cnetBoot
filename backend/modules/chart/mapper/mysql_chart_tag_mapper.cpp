module cnetnexus.chart.mapper.mysql_chart_case_mapper;

import std;
import cnetmod.coro;
import cnetmod.orm;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.framework.error;

namespace cnetnexus::chart {
namespace {

namespace chart = cnetnexus::chart;
namespace db_api = cnetmod::database;
namespace orm_api = cnetmod::orm;
using error_code = cnetnexus::framework::error::error_code;

/// @brief MySQL 唯一键冲突错误号。
constexpr std::uint16_t duplicate_entry_errno = 1062;

/// @brief count_since 的 to 传 0 时按「不限上界」处理。
constexpr std::int64_t no_upper_bound = 4102444800; // 2100-01-01T00:00:00Z

/// @brief ORM 模型 → 领域对象。
auto to_tag(const chart_tag_record &row) -> chart::chart_tag {
  return chart::chart_tag{
      .id = row.id,
      .name = row.name,
      .color = row.color,
      .owner_id = row.owner_id,
      .created_at =
          db_api::unix_seconds_from_datetime(row.created_at).value_or(0)};
}

/// @brief XML 列别名投影 → 领域对象。
auto to_tag(const chart_tag_projection &row) -> chart::chart_tag {
  return chart::chart_tag{.id = row.id,
                          .name = row.name,
                          .color = row.color,
                          .owner_id = row.owner_id,
                          .created_at = row.created_unix};
}

} // namespace

auto mysql_chart_case_mapper::validate_tag_access(
    std::span<const std::uint64_t> tag_ids)
    -> cnetmod::task<std::expected<void, error_code>> {
  std::vector<std::uint64_t> unique;
  unique.reserve(tag_ids.size());
  for (const auto id : tag_ids)
    if (std::ranges::find(unique, id) == unique.end())
      unique.push_back(id);
  if (unique.empty())
    co_return {};

  orm_api::query_wrapper<chart_tag_record> query;
  query.in(&chart_tag_record::id, unique);
  const auto visible = co_await tags_.list(query);
  if (visible.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  if (visible.data.size() != unique.size())
    co_return std::unexpected{error_code::tag_not_found};
  co_return {};
}

/// @brief 该 mapper 的标签与关联实现（列表 / 详情 / 标签增删）。
auto mysql_chart_case_mapper::attach_tags(std::vector<chart::chart_case> &cases)
    -> cnetmod::task<std::expected<void, error_code>> {
  if (cases.empty())
    co_return {};

  // 一次 JOIN 查询取回本页全部关联标签（IN 列表由 XML mapper 的 <foreach>
  // 渲染）。
  auto context = orm_api::param_context::from_map({});
  std::vector<orm_api::param_context> identifiers;
  identifiers.reserve(cases.size());
  for (const auto &item : cases)
    identifiers.push_back(orm_api::param_context::from_map(
        {{"case_id", orm_api::param_value::from_uint(item.id)}}));
  context.add_collection("case_ids", std::move(identifiers));

  const auto rows = co_await tags_.select_xml_as<chart_case_tag_projection>(
      *registry_, "ChartMapper.selectTagsForCases", context);
  if (rows.is_err())
    co_return std::unexpected{error_code::system_internal_error};

  for (const auto &row : rows.data) {
    const auto found = std::ranges::find_if(
        cases, [case_id = row.case_id](const chart::chart_case &item) {
          return item.id == case_id;
        });
    if (found == cases.end())
      continue;
    auto tag = chart::chart_tag{.id = row.id,
                                .name = row.name,
                                .color = row.color,
                                .owner_id = row.owner_id,
                                .created_at = row.created_unix};
    found->tag_ids.push_back(tag.id);
    found->tags.push_back(std::move(tag));
  }
  co_return {};
}

/// @brief 覆盖式替换一个案例的标签关联（去重；case 或任一 tag 不存在即拒绝）。
///  迁移自 xuanxue-chart/src/main/cpp/mapper/mysql/mysql_chart_store.cpp:648，
///  先校验案例和标签，再在一笔 Application 托管的事务中覆盖关联。
auto mysql_chart_case_mapper::set_tags(std::uint64_t case_id,
                                       std::span<const std::uint64_t> tag_ids)
    -> cnetmod::task<std::expected<void, error_code>> {
  const auto accessible_tags = co_await validate_tag_access(tag_ids);
  if (!accessible_tags)
    co_return std::unexpected{accessible_tags.error()};
  error_code failure = error_code::system_internal_error;
  const auto result = co_await cases_.transaction<void>(
      [case_id, tag_ids, &failure](
          auto &unit) -> cnetmod::task<std::expected<void, std::string>> {
        co_return co_await detail::replace_case_tags(unit, case_id, tag_ids,
                                                     failure);
      });
  if (!result)
    co_return std::unexpected{failure};
  co_return {};
}

auto mysql_chart_case_mapper::list_tags()
    -> cnetmod::task<std::expected<std::vector<chart::chart_tag>, error_code>> {
  const auto rows = co_await tags_.select_xml_as<chart_tag_projection>(
      *registry_, "ChartMapper.selectAllTags", orm_api::param_context{});
  if (rows.is_err())
    co_return std::unexpected{error_code::system_internal_error};

  std::vector<chart::chart_tag> tags;
  tags.reserve(rows.data.size());
  for (const auto &row : rows.data)
    tags.push_back(to_tag(row));
  co_return tags;
}

auto mysql_chart_case_mapper::find_tag(std::uint64_t id) -> cnetmod::task<
    std::expected<std::optional<chart::chart_tag>, error_code>> {
  const auto found =
      co_await tags_.get_by_id(orm_api::param_value::from_uint(id));
  if (found.is_err())
    co_return std::unexpected{error_code::system_internal_error};
  if (found.data.empty())
    co_return std::optional<chart::chart_tag>{};
  co_return std::optional<chart::chart_tag>{to_tag(found.data.front())};
}

auto mysql_chart_case_mapper::create_tag(const chart::chart_tag &value)
    -> cnetmod::task<std::expected<chart::chart_tag, error_code>> {
  if (value.name.empty())
    co_return std::unexpected{error_code::request_invalid_argument};

  chart_tag_record record{
      .name = value.name, .color = value.color, .owner_id = value.owner_id};
  const auto inserted = co_await tags_.save(record);
  if (inserted.is_err()) {
    if (inserted.error_code == duplicate_entry_errno)
      co_return std::unexpected{error_code::tag_conflict};
    co_return std::unexpected{error_code::system_internal_error};
  }

  chart::chart_tag created = value;
  created.id = record.id;
  const auto created_at = db_api::unix_seconds_from_datetime(record.created_at);
  if (!created_at)
    co_return std::unexpected{error_code::system_internal_error};
  created.created_at = *created_at;
  co_return created;
}

auto mysql_chart_case_mapper::remove_tag(std::uint64_t id)
    -> cnetmod::task<std::expected<void, error_code>> {
  error_code failure = error_code::system_internal_error;
  const auto result = co_await tags_.transaction<void>(
      [id, &failure](
          auto &unit) -> cnetmod::task<std::expected<void, std::string>> {
        const auto removed =
            co_await unit.template mapper<chart_tag_record>().remove_by_id(
                orm_api::param_value::from_uint(id));
        if (removed.is_err())
          co_return std::unexpected{"tag removal failed"};
        if (removed.affected_rows == 0) {
          failure = error_code::tag_not_found;
          co_return std::unexpected{"tag not found"};
        }

        auto links = orm_api::query_wrapper<chart_case_tag_record>{};
        links.eq(&chart_case_tag_record::tag_id, id);
        const auto cleared =
            co_await unit.template mapper<chart_case_tag_record>().remove(
                links);
        if (cleared.is_err())
          co_return std::unexpected{"tag links removal failed"};
        co_return std::expected<void, std::string>{};
      });
  if (!result)
    co_return std::unexpected{failure};
  co_return {};
}

} // namespace cnetnexus::chart
