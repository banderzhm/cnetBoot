import std;
import cnetmod.orm;
import cnetnexus.chart.mapper.chart_mapper_registry;
import cnetnexus.chart.mapper.mysql_chart_case_mapper;

namespace {

/// @brief 统计 SQL 文本里 `{}` 顺序占位符的个数。
auto placeholder_count(std::string_view sql) -> std::size_t {
  std::size_t total = 0;
  for (std::size_t i = 0; i + 1 < sql.size(); ++i) {
    if (sql[i] == '{' && sql[i + 1] == '}')
      ++total;
  }
  return total;
}

/// @brief 从 SELECT 语句里按出现顺序取出列名（有 AS 取别名，否则去掉表前缀）。
///  这是与适配层「按列名映射」的列序契约。
auto select_columns(std::string_view sql) -> std::vector<std::string> {
  std::string text;
  bool pending_space = false;
  for (const auto character : sql) {
    if (std::isspace(static_cast<unsigned char>(character))) {
      pending_space = !text.empty();
      continue;
    }
    if (pending_space) {
      text.push_back(' ');
      pending_space = false;
    }
    text.push_back(character);
  }

  const auto select = text.find("SELECT ");
  const auto from = text.find(" FROM ", select);
  if (select == std::string::npos || from == std::string::npos)
    return {};

  const auto body = text.substr(select + 7, from - select - 7);
  std::vector<std::string> columns;
  std::size_t start = 0;
  for (;;) {
    const auto comma = body.find(',', start);
    std::string_view token{body};
    token =
        token.substr(start, comma == std::string::npos ? std::string_view::npos
                                                       : comma - start);
    while (!token.empty() && token.front() == ' ')
      token.remove_prefix(1);
    while (!token.empty() && token.back() == ' ')
      token.remove_suffix(1);
    if (const auto alias = token.find(" AS "); alias != std::string_view::npos)
      token.remove_prefix(alias + 4);
    else if (const auto dot = token.rfind('.'); dot != std::string_view::npos)
      token.remove_prefix(dot + 1);
    if (!token.empty())
      columns.emplace_back(token);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return columns;
}

/// @brief 渲染一条 XML 语句（不连数据库，只跑动态 SQL 处理器）。
auto render(cnetmod::orm::mapper_registry &registry, std::string_view statement,
            const cnetmod::orm::param_context &context)
    -> cnetmod::orm::built_dynamic_sql {
  const auto *node = registry.find_statement(statement);
  const auto *fragments = registry.get_fragments("ChartMapper");
  cnetmod::orm::dynamic_sql_processor processor{cnetmod::orm::format_options{}};
  return processor.process(*node, context, *fragments);
}

/// @brief 渲染 chart_case XML 后再通过与生产仓储相同的逻辑删除拦截链。
auto render_case(cnetmod::orm::mapper_registry &registry,
                 std::string_view statement,
                 const cnetmod::orm::param_context &context)
    -> cnetmod::orm::intercepted_statement {
  auto built = render(registry, statement, context);
  cnetmod::orm::logical_delete_config deleted_at;
  deleted_at.field_name = "deleted_at";
  deleted_at.mode = cnetmod::orm::logical_delete_mode::nullable_datetime;
  cnetmod::orm::automatic_interceptor_options options;
  options.logical_delete_policy = std::move(deleted_at);
  auto chain = cnetmod::orm::make_automatic_interceptor_chain<
      cnetnexus::chart::chart_case_storage_record>(std::move(options));
  if (!chain)
    throw std::runtime_error{chain.error()};
  auto intercepted =
      (*chain)->apply(cnetmod::orm::sql_operation::query,
                      {std::move(built.sql), std::move(built.params)});
  if (!intercepted)
    throw std::runtime_error{intercepted.error()};
  return std::move(*intercepted);
}

/// @brief 组装案例检索参数。
auto case_context(std::string algorithm, std::string keyword,
                  std::uint64_t tag_id) -> cnetmod::orm::param_context {
  return cnetmod::orm::param_context::from_map({
      {"case_id", cnetmod::orm::param_value::from_uint(0)},
      {"algorithm",
       cnetmod::orm::param_value::from_string(std::move(algorithm))},
      {"keyword", cnetmod::orm::param_value::from_string(std::move(keyword))},
      {"tag_id", cnetmod::orm::param_value::from_uint(tag_id)},
      {"limit", cnetmod::orm::param_value::from_uint(20)},
      {"offset", cnetmod::orm::param_value::from_uint(0)},
  });
}

/// @brief unix 秒 → UTC 墙钟 DATETIME 参数（与适配层 w_time 同口径）。
auto wall_clock(std::int64_t seconds) -> cnetmod::orm::param_value {
  const auto point =
      std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
  const auto day = std::chrono::floor<std::chrono::days>(point);
  const std::chrono::year_month_day ymd{day};
  const std::chrono::hh_mm_ss clock{point - day};
  return cnetmod::orm::param_value::from_datetime(
      cnetmod::orm::calendar_datetime{
          .year = static_cast<std::uint16_t>(static_cast<int>(ymd.year())),
          .month =
              static_cast<std::uint8_t>(static_cast<unsigned>(ymd.month())),
          .day = static_cast<std::uint8_t>(static_cast<unsigned>(ymd.day())),
          .hour = static_cast<std::uint8_t>(clock.hours().count()),
          .minute = static_cast<std::uint8_t>(clock.minutes().count()),
          .second = static_cast<std::uint8_t>(clock.seconds().count()),
          .microsecond = 0,
      });
}

/// @brief 组装存档检索参数；0 表示「无边界」，与仓储一样绑 NULL。
auto archive_context(std::int64_t from, std::int64_t to)
    -> cnetmod::orm::param_context {
  return cnetmod::orm::param_context::from_map({
      {"algorithm", cnetmod::orm::param_value::from_string("")},
      {"keyword", cnetmod::orm::param_value::from_string("")},
      {"from_time",
       from > 0 ? wall_clock(from) : cnetmod::orm::param_value::null()},
      {"to_time", to > 0 ? wall_clock(to) : cnetmod::orm::param_value::null()},
      {"limit", cnetmod::orm::param_value::from_uint(20)},
      {"offset", cnetmod::orm::param_value::from_uint(0)},
  });
}

/// @brief 组装带 owner 分区的存档检索参数。
///  与 mysql_chart_archive_mapper 的 archive_context + owner_scope_identifiers
///  同口径： owner_scope = 0 表示不加谓词；谓词启用时 owner_scope_ids
///  必须非空（恒假兜底由 仓储侧保证，见 owner_scope_identifiers）。
auto owner_archive_context(std::string identifiers,
                           const std::vector<std::uint64_t> &entries,
                           std::uint64_t owner_scope)
    -> cnetmod::orm::param_context {
  auto context = cnetmod::orm::param_context::from_map({
      {"algorithm", cnetmod::orm::param_value::from_string("")},
      {"keyword", cnetmod::orm::param_value::from_string("")},
      {"from_time", cnetmod::orm::param_value::null()},
      {"to_time", cnetmod::orm::param_value::null()},
      {"limit", cnetmod::orm::param_value::from_uint(20)},
      {"offset", cnetmod::orm::param_value::from_uint(0)},
      {"owner_scope", cnetmod::orm::param_value::from_uint(owner_scope)},
      {"owner_scope_ids",
       cnetmod::orm::param_value::from_string(std::move(identifiers))},
  });
  std::vector<cnetmod::orm::param_context> items;
  items.reserve(entries.size());
  for (const auto owner : entries)
    items.push_back(cnetmod::orm::param_context::from_map(
        {{"owner_scope_item", cnetmod::orm::param_value::from_uint(owner)}}));
  context.add_collection("owner_scope_id_list", std::move(items));
  return context;
}

} // namespace

// XML mapper 用例：backend/modules/chart/mapper/ChartMapper.xml
// 必须能加载、语句齐备， 且动态条件按参数上下文正确展开（不需要真实
// MySQL；CTest 工作目录为 mappers 目录）。
//
// 迁移自 xuanxue-algorithm
// tests/chart_mapper_test.cpp。断言逐条保留（列序契约、
// 占位符对齐、无会话时区函数），只把装载入口换成 CNetNexus 的
// cnetnexus.chart.mapper.chart_mapper_registry::load_chart_mappers。
// 装载目录：优先取 argv[1]（CMake 注册时传
// ${CMAKE_CURRENT_BINARY_DIR}/mapper），
// 没有参数时退回当前工作目录——两种调用方式都必须能装载成功。
auto main(int argc, char **argv) -> int {
  const auto directory =
      argc > 1 ? std::filesystem::path{argv[1]} : std::filesystem::path{"."};
  cnetmod::orm::mapper_registry registry;
  if (auto loaded = cnetnexus::chart::load_chart_mappers(registry, directory);
      !loaded) {
    std::println("mapper 装载失败：{}", loaded.error());
    return 1;
  }

  for (const auto *statement :
       {"ChartMapper.selectCases", "ChartMapper.countCases",
        "ChartMapper.selectTagsForCases", "ChartMapper.searchArchives",
        "ChartMapper.countArchives"}) {
    if (registry.find_statement(statement) == nullptr)
      return 2;
  }

  // 1. 无过滤条件时不出现任何过滤片段，但仍带逻辑删除、排序与分页。
  const auto plain =
      render_case(registry, "ChartMapper.selectCases", case_context("", "", 0));
  if (plain.sql.find("`deleted_at` IS NULL") == std::string::npos ||
      plain.sql.find("`deleted_at` IS NULL") !=
          plain.sql.rfind("`deleted_at` IS NULL"))
    return 3;
  if (plain.sql.find("ORDER BY c.created_at DESC, c.id DESC") ==
      std::string::npos)
    return 4;
  if (plain.sql.find("LIMIT {} OFFSET {}") == std::string::npos)
    return 5;
  if (plain.sql.find("c.algorithm = ") != std::string::npos ||
      plain.sql.find("LIKE") != std::string::npos ||
      plain.sql.find("EXISTS") != std::string::npos)
    return 6;
  if (plain.parameters.size() != placeholder_count(plain.sql) ||
      plain.parameters.size() != 2)
    return 7;

  cnetmod::orm::data_permission_interceptor<
      cnetnexus::chart::chart_case_storage_record>
      owner_filter{cnetmod::orm::data_permission_scope{
          .include_owner = true,
          .owner_id = 42,
      }};
  auto owner_guarded =
      owner_filter.apply(cnetmod::orm::sql_operation::query,
                         {.sql = plain.sql, .parameters = plain.parameters});
  if (!owner_guarded || !owner_guarded->sql.contains("c.owner_id =") ||
      owner_guarded->parameters.size() != plain.parameters.size() + 1)
    return 30;

  // 2. 全部过滤条件同时生效：术数 / 关键字 / 标签 EXISTS 都进
  // SQL，参数个数对齐。
  const auto filtered = render_case(registry, "ChartMapper.selectCases",
                                    case_context("ba_zi", "%张三%", 7));
  if (filtered.sql.find("c.algorithm = {}") == std::string::npos)
    return 8;
  if (filtered.sql.find("c.title LIKE {}") == std::string::npos)
    return 9;
  if (filtered.sql.find("EXISTS (SELECT 1 FROM chart_case_tag ct") ==
      std::string::npos)
    return 10;
  // 关键字在 title/client_name/question 三处复用，因此占位符与参数都是 7 个。
  if (filtered.parameters.size() != placeholder_count(filtered.sql) ||
      filtered.parameters.size() != 7)
    return 11;

  // 3. countCases 与 selectCases 共享同一段过滤片段，但不带排序与分页。
  const auto counted = render_case(registry, "ChartMapper.countCases",
                                   case_context("ba_zi", "%张三%", 7));
  if (counted.sql.find("COUNT(*)") == std::string::npos ||
      counted.sql.find("LIMIT") != std::string::npos)
    return 12;

  // 4. <foreach> 展开 IN 列表：两个 case_id 生成两个占位符。
  auto context = cnetmod::orm::param_context::from_map({});
  std::vector<cnetmod::orm::param_context> identifiers;
  identifiers.push_back(cnetmod::orm::param_context::from_map(
      {{"case_id", cnetmod::orm::param_value::from_uint(3)}}));
  identifiers.push_back(cnetmod::orm::param_context::from_map(
      {{"case_id", cnetmod::orm::param_value::from_uint(4)}}));
  context.add_collection("case_ids", std::move(identifiers));
  const auto tags = render(registry, "ChartMapper.selectTagsForCases", context);
  // 空白由 SQL 规范化决定，因此只断言 IN 片段与两个占位符，不比对整段文本。
  if (tags.sql.find("ct.case_id IN") == std::string::npos ||
      placeholder_count(tags.sql) != 2)
    return 13;
  if (tags.params.size() != 2)
    return 14;

  // 5. 存档检索：无时间范围时条件片段完全不出现；带范围时是「列与 DATETIME
  // 参数直接比较」，
  //    整条 SQL 里不许出现任何按会话时区解释的时间函数。
  const auto archive_plain =
      render(registry, "ChartMapper.searchArchives", archive_context(0, 0));
  if (archive_plain.sql.find("a.created_at >= ") != std::string::npos ||
      archive_plain.sql.find("a.created_at <= ") != std::string::npos)
    return 15;
  const auto archive_window = render(registry, "ChartMapper.searchArchives",
                                     archive_context(1700000000, 1800000000));
  if (archive_window.sql.find("a.created_at >= {}") == std::string::npos ||
      archive_window.sql.find("a.created_at <= {}") == std::string::npos ||
      archive_window.params.size() != placeholder_count(archive_window.sql) ||
      archive_window.params.size() != 4)
    return 16;

  // 6. 列序契约：渲染出来的列清单必须与适配层的列投影逐列相同
  //    （改造前这些位置是魔法列号 row[0] / row[19]）。
  const auto same = [](const std::vector<std::string> &actual,
                       const auto &expected) {
    if (actual.size() != expected.size())
      return false;
    for (std::size_t index = 0; index < expected.size(); ++index) {
      if (actual[index] != expected[index])
        return false;
    }
    return true;
  };

  constexpr std::array k_case_columns{
      "id",          "owner_id",    "title",        "algorithm",
      "archive_id",  "calendar",    "gender",       "birth_year",
      "birth_month", "birth_day",   "birth_hour",   "birth_minute",
      "birth_place", "client_name", "question",     "notes",
      "reading",     "payload",     "created_unix", "updated_unix"};
  if (!same(select_columns(plain.sql), k_case_columns))
    return 17;

  constexpr std::array k_case_tag_columns{
      "case_id", "id", "name", "color", "owner_id", "created_unix"};
  if (!same(
          select_columns(
              render(registry, "ChartMapper.selectTagsForCases", context).sql),
          k_case_tag_columns))
    return 18;

  constexpr std::array k_archive_columns{"archive_id", "algorithm",
                                         "title",      "owner_id",
                                         "size_bytes", "created_unix"};
  if (!same(select_columns(archive_plain.sql), k_archive_columns))
    return 19;

  constexpr std::array k_tag_columns{"id", "name", "color", "owner_id",
                                     "created_unix"};
  if (!same(select_columns(render(registry, "ChartMapper.selectAllTags",
                                  cnetmod::orm::param_context{})
                               .sql),
            k_tag_columns))
    return 20;

  constexpr std::array k_algorithm_count_columns{"algorithm", "total"};
  if (!same(select_columns(render(registry, "ChartMapper.countsByAlgorithm",
                                  cnetmod::orm::param_context{})
                               .sql),
            k_algorithm_count_columns))
    return 21;

  // 7. owner 分区谓词（P0-1）：owner_scope = 0（全量可见）时不出现任何 owner
  // 谓词片段。
  //    注意 a.owner_id
  //    仍在列清单里（archiveColumns），因此只断言**谓词**不存在。
  if (archive_plain.sql.find("AND a.owner_id") != std::string::npos ||
      archive_plain.sql.find("a.owner_id IN") != std::string::npos ||
      archive_plain.sql.find("1 = 0") != std::string::npos)
    return 22;

  // 8. 谓词启用且标识列表非空：渲染成参数化的 IN 列表（两个标识 →
  // 两个占位符），
  //    不允许任何字符串拼接进 SQL 文本。
  // <where> 会吃掉首个 AND，因此断言 IN 片段本身，不假定 AND 前缀。
  const auto owned = render(registry, "ChartMapper.searchArchives",
                            owner_archive_context("0,42", {0, 42}, 1));
  if (owned.sql.find("a.owner_id IN") == std::string::npos)
    return 23;
  if (owned.sql.find("0,42") != std::string::npos)
    return 24; // 标识不得直接拼进 SQL
  if (owned.params.size() != placeholder_count(owned.sql) ||
      owned.params.size() != 4)
    return 25;

  // 9. 谓词启用但列表为空（仓储不该产生，属于纵深防御）：退化成恒假，
  //    绝不放行全表，参数个数仍与占位符对齐。
  const auto denied = render(registry, "ChartMapper.searchArchives",
                             owner_archive_context("", {}, 1));
  if (denied.sql.find("1 = 0") == std::string::npos)
    return 26;
  // a.owner_id 仍出现在列清单里；这里断言的是「没有 IN 谓词放行任何分区」。
  if (denied.sql.find("a.owner_id IN") != std::string::npos)
    return 27;
  if (denied.params.size() != placeholder_count(denied.sql) ||
      denied.params.size() != 2)
    return 28;

  // 10. countArchives 与 searchArchives 共享同一段过滤片段，因此 owner
  // 谓词同样生效。
  const auto counted_owned = render(registry, "ChartMapper.countArchives",
                                    owner_archive_context("42", {42}, 1));
  if (counted_owned.sql.find("a.owner_id IN") == std::string::npos ||
      counted_owned.sql.find("LIMIT") != std::string::npos)
    return 29;
  if (counted_owned.params.size() != placeholder_count(counted_owned.sql) ||
      counted_owned.params.size() != 1)
    return 30;
  return 0;
}
