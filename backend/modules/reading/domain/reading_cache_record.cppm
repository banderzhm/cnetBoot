/**
 * @brief 断语缓存的持久化模型（reading_cache 表的列清单）。
 *
 * 按项目分层约定，ORM 实体归 domain；缓存读写和时间转换留在 mapper 边界。
 *
 * 时间口径：DATETIME 列存 **UTC 墙钟**，领域层是 UTC 秒；unix 秒 <-> calendar_datetime 的
 * 互转在映射器边界完成，SQL 里不出现 FROM_UNIXTIME / UNIX_TIMESTAMP / NOW()。
 * 可空 DATETIME（last_hit_at / question）用框架原生的 optional<...>。
 */
module;

#include <cnetmod/orm.hpp>

export module cnetnexus.reading.domain.reading_cache_record;

import std;
import cnetmod.orm;

export namespace cnetnexus::reading {

/// @brief reading_cache 全列模型。
struct reading_cache_record
{
    std::uint64_t id{};
    std::uint64_t user_id{};
    std::string algorithm;
    std::string chart_fingerprint;
    std::string question_hash;
    std::optional<std::string> question;
    std::string content;
    std::string model;
    std::int64_t tokens_in{};
    std::int64_t tokens_out{};
    std::string engine_version;
    std::int64_t hit_count{};
    cnetmod::orm::calendar_datetime created_at{};
    cnetmod::orm::calendar_datetime updated_at{};
    std::optional<cnetmod::orm::calendar_datetime> last_hit_at;
};

} // namespace cnetnexus::reading

CNETMOD_MODEL(cnetnexus::reading::reading_cache_record, "reading_cache",
    CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
    CNETMOD_FIELD(user_id, "user_id", bigint),
    CNETMOD_FIELD(algorithm, "algorithm", varchar),
    CNETMOD_FIELD(chart_fingerprint, "chart_fingerprint", char_),
    CNETMOD_FIELD(question_hash, "question_hash", char_),
    CNETMOD_FIELD(question, "question", text, NULLABLE),
    CNETMOD_FIELD(content, "content", text),
    CNETMOD_FIELD(model, "model", varchar),
    CNETMOD_FIELD(tokens_in, "tokens_in", int_),
    CNETMOD_FIELD(tokens_out, "tokens_out", int_),
    CNETMOD_FIELD(engine_version, "engine_version", varchar),
    CNETMOD_FIELD(hit_count, "hit_count", int_),
    CNETMOD_FIELD(created_at, "created_at", datetime),
    CNETMOD_FIELD(updated_at, "updated_at", datetime),
    CNETMOD_FIELD(last_hit_at, "last_hit_at", datetime, NULLABLE))
