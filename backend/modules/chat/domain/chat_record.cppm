
module;




#include <cnetmod/orm.hpp>

export module cnetboot.chat.domain.chat_record;

import std;
import cnetmod.orm;

export namespace cnetboot::chat {


struct chat_session_record {
  std::uint64_t id{};
  std::uint64_t owner_id{};
  std::string title;
  std::string category;
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

} // namespace cnetboot::chat

CNETMOD_MODEL(
    cnetboot::chat::chat_session_record, "chat_session",
    CNETMOD_FIELD(id, "id", bigint, PK | AUTO_INC),
    CNETMOD_FIELD(owner_id, "owner_id", bigint, DATA_OWNER),
    CNETMOD_FIELD(title, "title", varchar),
    CNETMOD_FIELD(category, "category", varchar),
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

CNETMOD_MODEL(cnetboot::chat::chat_message_record, "chat_message",
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
