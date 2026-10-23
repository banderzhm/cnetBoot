
export module cnetboot.chat.mapper.chat_mapper;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.domain.chat_record;
import cnetboot.framework.error;
import cnetboot.framework.pagination;

export namespace cnetboot::chat {

namespace pagination = cnetboot::framework::pagination;
namespace error = cnetboot::framework::error;


class mysql_chat_mapper
{
public:
    mysql_chat_mapper(cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages,
        std::shared_ptr<const cnetmod::orm::mapper_registry> registry);


    [[nodiscard]] auto create_session(const chat_session& value)
        -> cnetmod::task<std::expected<chat_session, error::error_code>>;


    [[nodiscard]] auto find_session(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<chat_session>, error::error_code>>;


    [[nodiscard]] auto update_session(const chat_session& value)
        -> cnetmod::task<std::expected<chat_session, error::error_code>>;


    [[nodiscard]] auto remove_session(std::uint64_t id)
        -> cnetmod::task<std::expected<void, error::error_code>>;


    [[nodiscard]] auto find_session_page(const chat_session_query& value)
        -> cnetmod::task<std::expected<pagination::page_result<chat_session>, error::error_code>>;


    [[nodiscard]] auto append_message(const chat_message& value)
        -> cnetmod::task<std::expected<chat_message, error::error_code>>;


    [[nodiscard]] auto find_message_page(std::uint64_t session_id, const pagination::page_request& page)
        -> cnetmod::task<std::expected<pagination::page_result<chat_message>, error::error_code>>;


    [[nodiscard]] auto find_recent_messages(std::uint64_t session_id, std::size_t limit)
        -> cnetmod::task<std::expected<std::vector<chat_message>, error::error_code>>;

private:

    [[nodiscard]] auto select_session(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<chat_session>, error::error_code>>;

    cnetmod::application::managed_repository<chat_session_record> sessions_;
    cnetmod::application::managed_repository<chat_message_record> messages_;
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
};


[[nodiscard]] auto validate_chat_mapper_registry(const cnetmod::orm::mapper_registry& registry)
    -> std::expected<void, std::string>;

} // namespace cnetboot::chat
