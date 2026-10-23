module;

#include <cnetmod/json_codec.hpp>

/**
 * @brief Typed transport projections for the chat bounded context.
 */
export module cnetboot.chat.dto.chat;

import std;
import cnetmod.json;
import cnetboot.chat.domain.chat_domain;
import cnetboot.framework.pagination;

export namespace cnetboot::chat::dto {


struct chat_session_request
{
    std::optional<std::string> category;
    std::optional<std::string> title;
    std::optional<std::string> systemPrompt;
    std::optional<std::string> model;
    std::optional<double> temperature;
};


struct chat_message_request
{
    std::optional<std::string> content;
    std::optional<cnetmod::json::raw_value> context;
    std::optional<bool> stream;
};


struct chat_session_response
{
    std::uint64_t id{};
    std::string title;
    std::string category;
    std::string systemPrompt;
    std::string model;
    double temperature{};
    std::int64_t messageCount{};
    std::optional<std::string> lastMessageAt;
    std::string createdAt;
    std::string updatedAt;
};


struct chat_message_response
{
    std::uint64_t id{};
    std::uint64_t sessionId{};
    std::string role;
    std::string content;
    std::string model;
    std::string createdAt;
    std::optional<std::int64_t> tokensIn;
    std::optional<std::int64_t> tokensOut;
};


template <typename T> struct chat_page_response
{
    std::vector<T> items;
    std::int64_t total{};
    int page{};
    int page_size{};
};

[[nodiscard]] auto project(const cnetboot::chat::chat_session& value)
    -> chat_session_response;
[[nodiscard]] auto project(const cnetboot::chat::chat_message& value)
    -> chat_message_response;


template <typename Entity, typename Projection>
[[nodiscard]] auto project_chat_page(
    const cnetboot::framework::pagination::page_result<Entity>& page,
    Projection&& projection)
    -> chat_page_response<std::invoke_result_t<Projection, const Entity&>>
{
    using result_type = std::invoke_result_t<Projection, const Entity&>;
    chat_page_response<result_type> result;
    result.items.reserve(page.items.size());
    for (const auto& item : page.items)
        result.items.push_back(std::invoke(projection, item));
    result.total = page.total;
    result.page = page.page;
    result.page_size = page.page_size;
    return result;
}

[[nodiscard]] auto to_document(const chat_session_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(const chat_message_response& value)
    -> cnetmod::json::document;


template <typename T, typename ToDocument>
[[nodiscard]] auto page_document(const chat_page_response<T>& value,
    ToDocument&& to_document_element) -> cnetmod::json::document
{
    auto rows = cnetmod::json::array();
    rows.get_array().reserve(value.items.size());
    for (const auto& item : value.items)
        rows.get_array().push_back(std::invoke(to_document_element, item));
    return cnetmod::json::object({{"items", std::move(rows)},
        {"total", value.total}, {"page", value.page},
        {"pageSize", value.page_size}});
}

} // namespace cnetboot::chat::dto
