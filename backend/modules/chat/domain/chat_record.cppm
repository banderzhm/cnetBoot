/**
 * @brief 对话聚合的持久化模型（chat_session / chat_message 两张表的列清单）。
 *
 * 按项目分层约定，ORM 实体归 domain；实体与业务聚合之间的转换只发生在
 * mapper 边界。
 *
 * 时间口径：DATETIME 列存 **UTC 墙钟**，领域层是 UTC 秒；unix 秒 <-> UTC
 * 墙钟只在映射器 边界互换，SQL 里不出现任何按会话时区解释的函数（FROM_UNIXTIME
 * / UNIX_TIMESTAMP / NOW()）。 温度列 DECIMAL(3,2)：MySQL 协议把 DECIMAL
 * 解码成字符串，而框架的 set_member(double&) 只认 double/float
 * 字段值，所以模型用 std::string 承接原文，边界再转 double。
 */
module;

// CNETMOD_MODEL / CNETMOD_FIELD 只在头文件里：模块 TU 用全局模块片段 include，
// 宏在模块 purview 里展开（此时 import cnetmod.orm 的 model_traits
// 等名字已可见）。
#include <cnetmod/orm.hpp>

export module cnetnexus.chat.domain.chat_record;

import std;
import cnetmod.orm;

export namespace cnetnexus::chat {

/// @brief chat_session 全列模型。
struct chat_session_record {
  std::uint64_t id{};
  std::uint64_t owner_id{};
  std::string title;
  std::string algorithm;
  std::optional<std::string> system_prompt;
  std::string model;
  std::string temperature;
  std::int64_t message_count{};
  std::optional<cnetmod::orm::calendar_datetime> last_message_at;
  std::uint64_t created_by{};
  std::uint64_t updated_by{};
  cnetmod::orm::calendar_datetime created_at{};
  cnetmod::orm::calendar_datetime updated_at{};
  std::optional<cnetmod::orm::calendar_datetime> deleted_at;
};

/// @brief chat_message 全列模型。
struct chat_message_record {
  std::uint64_t id{};
  std::uint64_t session_id{};
  std::string role;
  std::string content;
  std::string model;
  std::int64_t tokens_in{};
  std::int64_t tokens_out{};
  cnetmod::orm::calendar_datetime created_at{};
  std::optional<cnetmod::orm::calendar_datetime> deleted_at;
};

} // namespace cnetnexus::chat

CNETMOD_MODEL(
    cnetnexus::chat::chat_session_record, "chat_session",
    CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
    CNETMOD_FIELD(owner_id, "owner_id", bigint, DATA_OWNER),
    CNETMOD_FIELD(title, "title", varchar),
    CNETMOD_FIELD(algorithm, "algorithm", varchar),
    CNETMOD_FIELD(system_prompt, "system_prompt", text, NULLABLE),
    CNETMOD_FIELD(model, "model", varchar),
    CNETMOD_FIELD(temperature, "temperature", decimal),
    CNETMOD_FIELD(message_count, "message_count", int_),
    CNETMOD_FIELD(last_message_at, "last_message_at", datetime, NULLABLE),
    CNETMOD_FIELD(created_by, "created_by", bigint),
    CNETMOD_FIELD(updated_by, "updated_by", bigint),
    CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT),
    CNETMOD_FIELD(updated_at, "updated_at", datetime, FILL_INSERT_UPDATE),
    CNETMOD_FIELD(deleted_at, "deleted_at", datetime, NULLABLE | LOGIC_DELETE))

CNETMOD_MODEL(cnetnexus::chat::chat_message_record, "chat_message",
              CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
              CNETMOD_FIELD(session_id, "session_id", bigint),
              CNETMOD_FIELD(role, "role", varchar),
              CNETMOD_FIELD(content, "content", text),
              CNETMOD_FIELD(model, "model", varchar),
              CNETMOD_FIELD(tokens_in, "tokens_in", int_),
              CNETMOD_FIELD(tokens_out, "tokens_out", int_),
              CNETMOD_FIELD(created_at, "created_at", datetime, FILL_INSERT),
              CNETMOD_FIELD(deleted_at, "deleted_at", datetime,
                            NULLABLE | LOGIC_DELETE))
