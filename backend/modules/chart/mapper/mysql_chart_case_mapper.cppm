export module cnetnexus.chart.mapper.mysql_chart_case_mapper;

import std;
import cnetmod.application.data;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.orm.xml_mapper_registry;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.domain.chart_record;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;

export namespace cnetnexus::chart {

// Composition-root names keep storage registration independent of mapper names.
using chart_case_storage_record = chart_case_record;
using chart_tag_storage_record = chart_tag_record;
using chart_archive_storage_record = chart_archive_record;

/** @brief 案例与标签的 MySQL mapper（chart_case / chart_tag /
 * chart_case_tag）。
 *
 *  持久化实体与查询投影由 domain.chart_record 统一声明；mapper 只负责
 *  仓储操作、XML 查询和领域对象映射。
 *
 *  路径分工：
 *  - 单表 CRUD 走 Application 托管仓储与 query_wrapper；
 *  - 动态检索（关键词三列 LIKE、标签 EXISTS、时间窗、JOIN + foreach、GROUP BY
 * 聚合、 保留 created_at 的定制 upsert）走 XML mapper
 *    （mapper/chart_mapper.xml），结果按列别名投影。
 *  所有语句都在框架连接池上以协程 co_await 执行。
 */
class mysql_chart_case_mapper final : public chart_case_mapper {
public:
  mysql_chart_case_mapper(
      cnetmod::application::managed_repository<chart_case_record> cases,
      cnetmod::application::managed_repository<chart_tag_record> tags,
      cnetmod::orm::mapper_registry &registry) noexcept;

  [[nodiscard]] auto create(const chart_case &value)
      -> cnetmod::task<std::expected<
          chart_case, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto find_by_id(std::uint64_t id) -> cnetmod::task<
      std::expected<std::optional<chart_case>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto update(const chart_case &value, bool replace_tags)
      -> cnetmod::task<std::expected<
          chart_case, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto remove(std::uint64_t id) -> cnetmod::task<
      std::expected<void, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto list(const case_query &query) -> cnetmod::task<
      std::expected<cnetnexus::framework::pagination::page_result<chart_case>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto count() -> cnetmod::task<std::expected<
      std::uint64_t, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto set_tags(std::uint64_t case_id,
                              std::span<const std::uint64_t> tag_ids)
      -> cnetmod::task<std::expected<
          void, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto list_tags() -> cnetmod::task<
      std::expected<std::vector<chart_tag>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto find_tag(std::uint64_t id) -> cnetmod::task<
      std::expected<std::optional<chart_tag>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto create_tag(const chart_tag &value)
      -> cnetmod::task<std::expected<
          chart_tag, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto remove_tag(std::uint64_t id) -> cnetmod::task<
      std::expected<void, cnetnexus::framework::error::error_code>> override;

private:
  cnetmod::orm::mapper_registry *registry_;
  cnetmod::application::managed_repository<chart_case_record> cases_;
  cnetmod::application::managed_repository<chart_tag_record> tags_;
  /// @brief Ensures every requested tag is visible in the request scope.
  [[nodiscard]] auto validate_tag_access(std::span<const std::uint64_t> tag_ids)
      -> cnetmod::task<
          std::expected<void, cnetnexus::framework::error::error_code>>;
  /// @brief 装配单个案例的标签（列表与详情共用）。
  [[nodiscard]] auto attach_tags(std::vector<chart_case> &cases)
      -> cnetmod::task<
          std::expected<void, cnetnexus::framework::error::error_code>>;
};

/** @brief 存档索引的 MySQL mapper（chart_archive_index）。
 *  写入用 XML 保留既有 created_at；检索/计数走托管仓储。
 */
class mysql_chart_archive_mapper final : public chart_archive_repository {
public:
  mysql_chart_archive_mapper(
      cnetmod::application::managed_repository<chart_archive_record> archives,
      cnetmod::orm::mapper_registry &registry) noexcept;

  [[nodiscard]] auto save(const archive_snapshot &value) -> cnetmod::task<
      std::expected<void, cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto remove(std::string_view archive_id) -> cnetmod::task<
      std::expected<void, cnetnexus::framework::error::error_code>> override;
  /// P0-1：归属判定读的索引条目（含 owner_id）；实现见
  /// mysql_chart_archive_mapper.cpp。
  [[nodiscard]] auto find(std::string_view archive_id) -> cnetmod::task<
      std::expected<std::optional<archive_index_entry>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto load(std::string_view archive_id) -> cnetmod::task<
      std::expected<std::optional<archive_snapshot>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto search(const archive_query &query) -> cnetmod::task<
      std::expected<cnetnexus::framework::pagination::page_result<archive_index_entry>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto counts_by_algorithm() -> cnetmod::task<
      std::expected<std::vector<algorithm_count>,
                    cnetnexus::framework::error::error_code>> override;
  [[nodiscard]] auto count_since(std::int64_t from, std::int64_t to)
      -> cnetmod::task<std::expected<
          std::uint64_t, cnetnexus::framework::error::error_code>> override;

private:
  cnetmod::orm::mapper_registry *registry_;
  cnetmod::application::managed_repository<chart_archive_record> archives_;
};

/** @brief 两个 MySQL mapper 的聚合，共享 Application 管理的数据源。 */
struct mysql_chart_mappers {
  mysql_chart_mappers(
      cnetmod::application::managed_repository<chart_case_record> cases,
      cnetmod::application::managed_repository<chart_tag_record> tags,
      cnetmod::application::managed_repository<chart_archive_record> archives,
      cnetmod::orm::mapper_registry &registry) noexcept;

  mysql_chart_case_mapper cases;
  mysql_chart_archive_mapper archives;
};

} // namespace cnetnexus::chart

namespace cnetnexus::chart::detail {

/**
 * @brief
 * 在一个事务单元内覆盖案例的标签关联：校验案例与标签存在、去重、清空后重建。
 *
 * create / update / set_tags 共用，保证“写案例 +
 * 写标签”要么一起生效要么一起回滚。
 * @param failure 业务失败时写入的注册码（CASE.NOT_FOUND / TAG.NOT_FOUND）；
 *        其余失败保持调用方给定的默认值
 */
template <typename Unit>
auto replace_case_tags(Unit &unit, std::uint64_t case_id,
                       std::span<const std::uint64_t> tag_ids,
                       cnetnexus::framework::error::error_code &failure)
    -> cnetmod::task<std::expected<void, std::string>> {
  auto case_query = cnetmod::orm::query_wrapper<chart_case_record>{};
  case_query.eq(&chart_case_record::id, case_id).limit(1);
  const auto existing =
      co_await unit.template mapper<chart_case_record>().select_list(
          case_query);
  if (existing.is_err())
    co_return std::unexpected{std::string{"case lookup failed"}};
  if (existing.data.empty()) {
    failure = cnetnexus::framework::error::error_code::case_not_found;
    co_return std::unexpected{std::string{"case not found"}};
  }

  std::vector<std::uint64_t> unique;
  for (const auto tag_id : tag_ids) {
    if (std::ranges::find(unique, tag_id) != unique.end())
      continue;
    const auto tag =
        co_await unit.template mapper<chart_tag_record>().select_by_id(
            cnetmod::orm::param_value::from_uint(tag_id));
    if (tag.is_err())
      co_return std::unexpected{std::string{"tag lookup failed"}};
    if (tag.data.empty()) {
      failure = cnetnexus::framework::error::error_code::tag_not_found;
      co_return std::unexpected{std::string{"tag not found"}};
    }
    unique.push_back(tag_id);
  }

  auto links = cnetmod::orm::query_wrapper<chart_case_tag_record>{};
  links.eq(&chart_case_tag_record::case_id, case_id);
  const auto cleared =
      co_await unit.template mapper<chart_case_tag_record>().remove(links);
  if (cleared.is_err())
    co_return std::unexpected{std::string{"tag links removal failed"}};
  for (const auto tag_id : unique) {
    chart_case_tag_record link{.case_id = case_id, .tag_id = tag_id};
    const auto linked =
        co_await unit.template mapper<chart_case_tag_record>().insert(link);
    if (linked.is_err())
      co_return std::unexpected{std::string{"tag link insert failed"}};
  }
  co_return std::expected<void, std::string>{};
}

} // namespace cnetnexus::chart::detail
