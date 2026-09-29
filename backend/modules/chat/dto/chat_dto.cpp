module;

#include <cnetmod/json_codec.hpp>

module cnetboot.chat.dto.chat;

// DTO projection and serialization stay outside the HTTP controller layer.

import std;
import cnetmod.json;
import cnetboot.framework.time;

namespace cnetboot::chat::dto {

namespace {

    [[nodiscard]] auto nullable(const std::optional<std::string>& value)
        -> cnetmod::json::document
    {
        cnetmod::json::document result;
        if (value)
            result = *value;
        else
            result = nullptr;
        return result;
    }

    template <typename T>
    [[nodiscard]] auto nullable_number(const std::optional<T>& value)
        -> cnetmod::json::document
    {
        cnetmod::json::document result;
        if (value)
            result = *value;
        else
            result = nullptr;
        return result;
    }

} // namespace

auto project(const cnetboot::chat::chat_session& value) -> chat_session_response
{
    chat_session_response result{.id = value.id,
        .title = value.title,
        .category = value.category,
        .systemPrompt = value.system_prompt,
        .model = value.model,
        .temperature = value.temperature,
        .messageCount = value.message_count,
        .createdAt = cnetboot::framework::iso8601_utc(value.created_at),
        .updatedAt = cnetboot::framework::iso8601_utc(value.updated_at)};
    if (value.last_message_at != 0)
        result.lastMessageAt = cnetboot::framework::iso8601_utc(value.last_message_at);
    return result;
}

auto project(const cnetboot::chat::chat_message& value) -> chat_message_response
{
    chat_message_response result{.id = value.id,
        .sessionId = value.session_id,
        .role = value.role,
        .content = value.content,
        .model = value.model,
        .createdAt = cnetboot::framework::iso8601_utc(value.created_at)};
    if (value.role == cnetboot::chat::role_assistant)
    {
        result.tokensIn = value.tokens_in;
        result.tokensOut = value.tokens_out;
    }
    return result;
}

auto to_document(const chat_session_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"id", value.id}, {"title", value.title},
        {"category", value.category}, {"systemPrompt", value.systemPrompt},
        {"model", value.model}, {"temperature", value.temperature},
        {"messageCount", value.messageCount},
        {"lastMessageAt", nullable(value.lastMessageAt)},
        {"createdAt", value.createdAt}, {"updatedAt", value.updatedAt}});
}

auto to_document(const chat_message_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"id", value.id},
        {"sessionId", value.sessionId}, {"role", value.role},
        {"content", value.content}, {"model", value.model},
        {"createdAt", value.createdAt},
        {"tokensIn", nullable_number(value.tokensIn)},
        {"tokensOut", nullable_number(value.tokensOut)}});
}

} // namespace cnetboot::chat::dto
