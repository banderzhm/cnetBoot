module;

#include <cnetmod/orm.hpp>

export module cnetnexus.chart.domain.chart_record;

import std;
import cnetmod.orm;
// 持久化模型按 chart 表定义列出，不生成 DDL。DATETIME 存 UTC 墙钟；领域层
// 使用 Unix 秒，读写边界通过 cnetmod 的 datetime 转换函数处理。
// 自动读取过滤使用按模型配置的可空 DATETIME 逻辑删除策略；选择性更新
// 仍显式维护 deleted_at/updated_at 的业务原子性。

/// @brief chart_case 全列模型（ORM insert / query_wrapper / update_wrapper
/// 用）。
export namespace cnetnexus::chart {
struct chart_case_record {
  std::uint64_t id{};
  std::uint64_t owner_id{};
  std::string title;
  std::string algorithm;
  std::optional<std::string> archive_id;
  std::string calendar;
  std::string gender;
  std::int64_t birth_year{};
  std::int64_t birth_month{};
  std::int64_t birth_day{};
  std::int64_t birth_hour{};
  std::int64_t birth_minute{};
  std::string birth_place;
  std::string client_name;
  std::string question;
  std::optional<std::string> notes;
  std::optional<std::string> reading;
  std::optional<std::string> payload;
  std::uint64_t created_by{};
  std::uint64_t updated_by{};
  cnetmod::orm::calendar_datetime created_at{};
  cnetmod::orm::calendar_datetime updated_at{};
  /// @note 可空 DATETIME 用框架原生的
  /// optional<calendar_datetime>（model_metadata.cpp:231-237 / 321-324）。
  std::optional<cnetmod::orm::calendar_datetime> deleted_at;
};
} // namespace cnetnexus::chart

CNETMOD_MODEL(cnetnexus::chart::chart_case_record, "chart_case",
              CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
              CNETMOD_FIELD(owner_id, "owner_id", bigint, DATA_OWNER),
              CNETMOD_FIELD(title, "title", varchar),
              CNETMOD_FIELD(algorithm, "algorithm", varchar),
              CNETMOD_FIELD(archive_id, "archive_id", char_, NULLABLE),
              CNETMOD_FIELD(calendar, "calendar", varchar),
              CNETMOD_FIELD(gender, "gender", varchar),
              CNETMOD_FIELD(birth_year, "birth_year", smallint),
              CNETMOD_FIELD(birth_month, "birth_month", tinyint),
              CNETMOD_FIELD(birth_day, "birth_day", tinyint),
              CNETMOD_FIELD(birth_hour, "birth_hour", tinyint),
              CNETMOD_FIELD(birth_minute, "birth_minute", tinyint),
              CNETMOD_FIELD(birth_place, "birth_place", varchar),
              CNETMOD_FIELD(client_name, "client_name", varchar),
              CNETMOD_FIELD(question, "question", varchar),
              CNETMOD_FIELD(notes, "notes", text, NULLABLE),
              CNETMOD_FIELD(reading, "reading", text, NULLABLE),
              CNETMOD_FIELD(payload, "payload", text, NULLABLE),
              CNETMOD_FIELD(created_by, "created_by", bigint),
              CNETMOD_FIELD(updated_by, "updated_by", bigint),
              CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT),
              CNETMOD_FIELD(updated_at, "updated_at", datetime,
                            FILL_INSERT_UPDATE),
              CNETMOD_FIELD(deleted_at, "deleted_at", datetime,
                            NULLABLE | LOGIC_DELETE))

/// @brief chart_mapper.xml selectCases 的**列别名投影**：列名与 <sql
/// id="caseColumns"> 的别名逐列一致，
///        供框架的 from_row（按列名映射）解析 XML 结果，取代手写 row[0..19]
///        下标。
export namespace cnetnexus::chart {
struct chart_case_projection {
  std::uint64_t id{};
  std::uint64_t owner_id{};
  std::string title;
  std::string algorithm;
  std::optional<std::string> archive_id;
  std::string calendar;
  std::string gender;
  std::int64_t birth_year{};
  std::int64_t birth_month{};
  std::int64_t birth_day{};
  std::int64_t birth_hour{};
  std::int64_t birth_minute{};
  std::string birth_place;
  std::string client_name;
  std::string question;
  std::optional<std::string> notes;
  std::optional<std::string> reading;
  std::optional<std::string> payload;
  std::int64_t created_unix{};
  std::int64_t updated_unix{};
};
} // namespace cnetnexus::chart

CNETMOD_PROJECTION(cnetnexus::chart::chart_case_projection,
                   CNETMOD_FIELD(id, "id", bigint),
                   CNETMOD_FIELD(owner_id, "owner_id", bigint),
                   CNETMOD_FIELD(title, "title", varchar),
                   CNETMOD_FIELD(algorithm, "algorithm", varchar),
                   CNETMOD_FIELD(archive_id, "archive_id", char_, NULLABLE),
                   CNETMOD_FIELD(calendar, "calendar", varchar),
                   CNETMOD_FIELD(gender, "gender", varchar),
                   CNETMOD_FIELD(birth_year, "birth_year", smallint),
                   CNETMOD_FIELD(birth_month, "birth_month", tinyint),
                   CNETMOD_FIELD(birth_day, "birth_day", tinyint),
                   CNETMOD_FIELD(birth_hour, "birth_hour", tinyint),
                   CNETMOD_FIELD(birth_minute, "birth_minute", tinyint),
                   CNETMOD_FIELD(birth_place, "birth_place", varchar),
                   CNETMOD_FIELD(client_name, "client_name", varchar),
                   CNETMOD_FIELD(question, "question", varchar),
                   CNETMOD_FIELD(notes, "notes", text, NULLABLE),
                   CNETMOD_FIELD(reading, "reading", text, NULLABLE),
                   CNETMOD_FIELD(payload, "payload", text, NULLABLE),
                   CNETMOD_FIELD(created_unix, "created_unix", bigint),
                   CNETMOD_FIELD(updated_unix, "updated_unix", bigint))

/// @brief chart_tag 全列模型。
export namespace cnetnexus::chart {
struct chart_tag_record {
  std::uint64_t id{};
  std::string name;
  std::string color;
  std::uint64_t owner_id{};
  cnetmod::orm::calendar_datetime created_at{};
};
} // namespace cnetnexus::chart

CNETMOD_MODEL(cnetnexus::chart::chart_tag_record, "chart_tag",
              CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
              CNETMOD_FIELD(name, "name", varchar),
              CNETMOD_FIELD(color, "color", varchar),
              CNETMOD_FIELD(owner_id, "owner_id", bigint, DATA_OWNER),
              CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT))

/// @brief chart_mapper.xml selectAllTags 的列别名投影。
export namespace cnetnexus::chart {
struct chart_tag_projection {
  std::uint64_t id{};
  std::string name;
  std::string color;
  std::uint64_t owner_id{};
  std::int64_t created_unix{};
};
} // namespace cnetnexus::chart

CNETMOD_PROJECTION(cnetnexus::chart::chart_tag_projection,
                   CNETMOD_FIELD(id, "id", bigint),
                   CNETMOD_FIELD(name, "name", varchar),
                   CNETMOD_FIELD(color, "color", varchar),
                   CNETMOD_FIELD(owner_id, "owner_id", bigint),
                   CNETMOD_FIELD(created_unix, "created_unix", bigint))

/// @brief chart_mapper.xml selectTagsForCases 的列别名投影（首列 case_id + JOIN
/// 出来的标签列）。
export namespace cnetnexus::chart {
struct chart_case_tag_projection {
  std::uint64_t case_id{};
  std::uint64_t id{};
  std::string name;
  std::string color;
  std::uint64_t owner_id{};
  std::int64_t created_unix{};
};
} // namespace cnetnexus::chart

CNETMOD_PROJECTION(cnetnexus::chart::chart_case_tag_projection,
                   CNETMOD_FIELD(case_id, "case_id", bigint),
                   CNETMOD_FIELD(id, "id", bigint),
                   CNETMOD_FIELD(name, "name", varchar),
                   CNETMOD_FIELD(color, "color", varchar),
                   CNETMOD_FIELD(owner_id, "owner_id", bigint),
                   CNETMOD_FIELD(created_unix, "created_unix", bigint))

/// @brief chart_case_tag 关联行模型（复合主键，ORM insert / query_wrapper
/// 删除用）。
export namespace cnetnexus::chart {
struct chart_case_tag_record {
  std::uint64_t case_id{};
  std::uint64_t tag_id{};
};
} // namespace cnetnexus::chart

CNETMOD_MODEL(cnetnexus::chart::chart_case_tag_record, "chart_case_tag",
              CNETMOD_FIELD(case_id, "case_id", bigint, PK),
              CNETMOD_FIELD(tag_id, "tag_id", bigint, PK))

/// @brief chart_archive_index 全列模型（query_wrapper / count 用）。
export namespace cnetnexus::chart {
struct chart_archive_record {
  std::string archive_id;
  std::string algorithm;
  std::string title;
  std::uint64_t owner_id{};
  std::uint64_t size_bytes{};
  std::string payload;
  cnetmod::orm::calendar_datetime created_at{};
};
} // namespace cnetnexus::chart

CNETMOD_MODEL(cnetnexus::chart::chart_archive_record, "chart_archive_index",
              CNETMOD_FIELD(archive_id, "archive_id", char_, PK),
              CNETMOD_FIELD(algorithm, "algorithm", varchar),
              CNETMOD_FIELD(title, "title", varchar),
              CNETMOD_FIELD(owner_id, "owner_id", bigint),
              CNETMOD_FIELD(size_bytes, "size_bytes", int_),
              CNETMOD_FIELD(payload, "payload", text),
              CNETMOD_FIELD(created_at, "created_at", datetime))

/// @brief chart_mapper.xml searchArchives 的列别名投影（created_unix）。
export namespace cnetnexus::chart {
struct chart_archive_projection {
  std::string archive_id;
  std::string algorithm;
  std::string title;
  std::uint64_t owner_id{};
  std::uint64_t size_bytes{};
  std::int64_t created_unix{};
};
} // namespace cnetnexus::chart

CNETMOD_PROJECTION(cnetnexus::chart::chart_archive_projection,
                   CNETMOD_FIELD(archive_id, "archive_id", char_),
                   CNETMOD_FIELD(algorithm, "algorithm", varchar),
                   CNETMOD_FIELD(title, "title", varchar),
                   CNETMOD_FIELD(owner_id, "owner_id", bigint),
                   CNETMOD_FIELD(size_bytes, "size_bytes", int_),
                   CNETMOD_FIELD(created_unix, "created_unix", bigint))

/// @brief chart_mapper.xml countsByAlgorithm 的列别名投影（algorithm, total）。
export namespace cnetnexus::chart {
struct chart_algorithm_count_projection {
  std::string algorithm;
  std::int64_t total{};
};
} // namespace cnetnexus::chart

CNETMOD_PROJECTION(cnetnexus::chart::chart_algorithm_count_projection,
                   CNETMOD_FIELD(algorithm, "algorithm", varchar),
                   CNETMOD_FIELD(total, "total", bigint))

