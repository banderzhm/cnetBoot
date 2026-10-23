module;

#include <cnetmod/json_codec.hpp>

export module cnetboot.chat.controller.chat_route_context;

import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.service.chat_service;
import cnetboot.chat.service.chat_service_factory;
import cnetboot.framework.error;
import cnetboot.framework.web.route_handler;

export namespace cnetboot::chat {

using chat_service_resolver = std::function<std::expected<
    std::unique_ptr<chat_service_scope>, cnetboot::framework::error::error_code>(
    const cnetmod::http::request_context&)>;

} // namespace cnetboot::chat

export namespace cnetboot::chat::detail {

/** @brief Route dependencies kept as non-owning pointers by registered handlers. */
struct chat_route_context {
  cnetboot::chat::chat_service_resolver resolve_chat;
};


enum class chat_context_status {
  absent,
  text,
  invalid_type,
  too_large,
};


struct chat_context_field {
  chat_context_status status{chat_context_status::absent};
  std::string value;
  std::size_t
      characters{};
};


[[nodiscard]] auto
normalize_reference_context(const std::optional<cnetmod::json::raw_value> &value)
    -> chat_context_field;


[[nodiscard]] auto chat_delta_payload(std::string_view text) -> std::string;


[[nodiscard]] auto
chat_done_payload(const cnetboot::chat::chat_message &message) -> std::string;

[[nodiscard]] auto
handle_session_create(cnetmod::http::request_context &context,
                      const chat_route_context &services) -> cnetmod::task<void>;
[[nodiscard]] auto handle_session_list(cnetmod::http::request_context &context,
                                       const chat_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_session_read(cnetmod::http::request_context &context,
                                       const chat_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_session_update(cnetmod::http::request_context &context,
                      const chat_route_context &services) -> cnetmod::task<void>;
[[nodiscard]] auto
handle_session_delete(cnetmod::http::request_context &context,
                      const chat_route_context &services) -> cnetmod::task<void>;
[[nodiscard]] auto handle_message_list(cnetmod::http::request_context &context,
                                       const chat_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_message_send(cnetmod::http::request_context &context,
                                       const chat_route_context &services)
    -> cnetmod::task<void>;


void declare_chat_handlers(cnetmod::http::router &routes,
                           const chat_route_context &services);

} // namespace cnetboot::chat::detail
