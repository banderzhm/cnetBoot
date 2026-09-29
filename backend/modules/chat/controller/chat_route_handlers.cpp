module;

#include <cnetmod/json_codec.hpp>

module cnetboot.chat.controller.chat_route_context;

import std;
import cnetboot.framework.web.response;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.chat.dto.chat;
import cnetboot.chat.controller.chat_routes;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.service.chat_service;
import cnetboot.framework.security.request_identity;
import cnetboot.framework.error;
import cnetboot.framework.security.current_user;
import cnetboot.framework.security.authorization;
import cnetboot.framework.security.current_user;
import cnetboot.framework.web.error_response;
import cnetboot.framework.pagination;
import cnetboot.framework.web.query;
import cnetboot.framework.web.sse_stream;

namespace cnetboot::chat::detail {

namespace error = cnetboot::framework::error;
namespace web = cnetboot::framework::web;

// 具名响应体必须有外部链接：Glaze 的字段名反射要求类型具备链接性。
struct done_response {
  std::uint64_t messageId{};
  std::string content;
  std::int64_t tokensIn{};
  std::int64_t tokensOut{};
};

namespace {

[[nodiscard]] auto page_from(std::string_view raw)
    -> cnetboot::framework::pagination::page_request {
  cnetboot::framework::pagination::page_request page;
  page.page = web::query::integer(raw, "page").value_or(1);
  page.page_size =
      web::query::integer(raw, "page_size").value_or(
          cnetboot::framework::pagination::default_page_size);
  return cnetboot::framework::pagination::normalize(page);
}

} // namespace

auto chat_delta_payload(std::string_view text) -> std::string {
  // delta 载荷与断语共用同一份实现（契约字段名 text 冻结在 sse_stream）。
  return web::sse_delta_payload(text);
}

auto chat_done_payload(const cnetboot::chat::chat_message &message)
    -> std::string {
  const auto payload =
      cnetmod::json::write(done_response{.messageId = message.id,
                                         .content = message.content,
                                         .tokensIn = message.tokens_in,
                                         .tokensOut = message.tokens_out});
  return payload.value_or(std::string{});
}

auto normalize_reference_context(
    const std::optional<cnetmod::json::raw_value> &input)
    -> chat_context_field {
  chat_context_field result;
  if (!input || input->is_null())
    return result;

  if (input->is_string()) {
    const auto decoded = cnetmod::json::from_document<std::string>(*input);
    if (!decoded) {
      result.status = chat_context_status::invalid_type;
      return result;
    }
    result.value = *decoded;
  } else if (input->is_object() || input->is_array()) {
    // 空对象 / 空数组等价于不传。
    if (input->is_object() && input->get_object().empty())
      return result;
    if (input->is_array() && input->get_array().empty())
      return result;
    auto compact = cnetmod::json::write_document(*input);
    if (!compact) {
      result.status = chat_context_status::invalid_type;
      return result;
    }
    result.value = std::move(*compact);
  } else {
    result.status = chat_context_status::invalid_type;
    return result;
  }

  // 上限按字符数（Unicode 码点）而不是字节数：中文 1 字 = 1。
  result.characters = cnetboot::chat::count_characters(result.value);
  if (result.value.empty())
    return result;
  if (result.characters > cnetboot::chat::max_reference_context_characters) {
    result.status = chat_context_status::too_large;
    return result;
  }
  result.status = chat_context_status::text;
  return result;
}

auto handle_session_create(cnetmod::http::request_context &context,
                           const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto *principal = cnetboot::security::current_principal(context);

  auto body =
      cnetmod::json::parse_lenient<cnetboot::chat::dto::chat_session_request>(
          co_await context.read_full_body());
  if (!body) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  cnetboot::chat::chat_session_create_request request;
  request.category = body->category.value_or("");
  request.title = body->title.value_or("");
  request.system_prompt = body->systemPrompt.value_or("");
  request.model = body->model.value_or("");
  if (body->temperature) {
    request.temperature = *body->temperature;
    request.has_temperature = true;
  }

  const auto created = co_await chat.create_session(
      request, cnetboot::security::audit_actor_from(context, principal));
  if (!created) {
    web::respond_error(context, created.error());
    co_return;
  }
  cnetboot::framework::web::respond_document(
      context, cnetboot::chat::dto::to_document(
                   cnetboot::chat::dto::project(*created)));
  co_return;
}

auto handle_session_list(cnetmod::http::request_context &context,
                         const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto raw = context.query_string();
  cnetboot::chat::chat_session_query filter;
  filter.category = web::query::value(raw, "category").value_or("");
  filter.page = page_from(raw);

  const auto result = co_await chat.list_sessions(filter);
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  const auto page =
      cnetboot::chat::dto::project_chat_page(*result, [](const auto &value) {
        return cnetboot::chat::dto::project(value);
      });
  cnetboot::framework::web::respond_document(
      context,
      cnetboot::chat::dto::page_document(
          page, [](const cnetboot::chat::dto::chat_session_response &value) {
            return cnetboot::chat::dto::to_document(value);
          }));
  co_return;
}

auto handle_session_read(cnetmod::http::request_context &context,
                         const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto found = co_await chat.find_session(*id);
  if (!found) {
    web::respond_error(context, found.error());
    co_return;
  }
  cnetboot::framework::web::respond_document(
      context,
      cnetboot::chat::dto::to_document(cnetboot::chat::dto::project(*found)));
  co_return;
}

auto handle_session_update(cnetmod::http::request_context &context,
                           const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto *principal = cnetboot::security::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  auto body =
      cnetmod::json::parse_lenient<cnetboot::chat::dto::chat_session_request>(
          co_await context.read_full_body());
  if (!body) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  cnetboot::chat::chat_session_patch patch;
  if (body->title) {
    patch.has_title = true;
    patch.title = *body->title;
  }
  if (body->systemPrompt) {
    patch.has_system_prompt = true;
    patch.system_prompt = *body->systemPrompt;
  }
  if (body->model) {
    patch.has_model = true;
    patch.model = *body->model;
  }
  if (body->temperature) {
    patch.has_temperature = true;
    patch.temperature = *body->temperature;
  }

  const auto updated = co_await chat.update_session(
      *id, patch, cnetboot::security::audit_actor_from(context, principal));
  if (!updated) {
    web::respond_error(context, updated.error());
    co_return;
  }
  cnetboot::framework::web::respond_document(
      context, cnetboot::chat::dto::to_document(
                   cnetboot::chat::dto::project(*updated)));
  co_return;
}

auto handle_session_delete(cnetmod::http::request_context &context,
                           const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto *principal = cnetboot::security::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto removed = co_await chat.remove_session(
      *id, cnetboot::security::audit_actor_from(context, principal));
  if (!removed) {
    web::respond_error(context, removed.error());
    co_return;
  }
  cnetboot::framework::web::respond_document(
      context, cnetmod::json::object({{"success", true}}));
  co_return;
}

auto handle_message_list(cnetmod::http::request_context &context,
                         const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto result =
      co_await chat.list_messages(*id, page_from(context.query_string()));
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  const auto page =
      cnetboot::chat::dto::project_chat_page(*result, [](const auto &value) {
        return cnetboot::chat::dto::project(value);
      });
  cnetboot::framework::web::respond_document(
      context,
      cnetboot::chat::dto::page_document(
          page, [](const cnetboot::chat::dto::chat_message_response &value) {
            return cnetboot::chat::dto::to_document(value);
          }));
  co_return;
}

auto handle_message_send(cnetmod::http::request_context &context,
                         const chat_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_chat(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  auto &chat = (*scope)->service;
  const auto *principal = cnetboot::security::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  auto body =
      cnetmod::json::parse_lenient<cnetboot::chat::dto::chat_message_request>(
          co_await context.read_full_body());
  if (!body) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  if (!body->content) {
    web::respond_error(context, error::error_code::request_invalid_argument,
                       "content 必须是字符串");
    co_return;
  }
  if (body->content->empty()) {
    web::respond_error(context, error::error_code::request_invalid_argument,
                       "content 不能为空");
    co_return;
  }
  if (const auto characters = cnetboot::chat::count_characters(*body->content);
      characters > cnetboot::chat::max_message_characters) {
    web::respond_error(context, error::error_code::request_invalid_argument,
                       std::format("内容过长（{} 字，上限 {} 字）", characters,
                                   cnetboot::chat::max_message_characters));
    co_return;
  }

  // context 接受 string | object | array；对象/数组归一化成紧凑 JSON
  // 文本。
  const auto reference_context = normalize_reference_context(body->context);
  switch (reference_context.status) {
  case chat_context_status::invalid_type:
    web::respond_error(context, error::error_code::request_invalid_argument,
                       "context 只接受字符串或 JSON 对象/数组");
    co_return;
  case chat_context_status::too_large:
    web::respond_error(
        context, error::error_code::request_invalid_argument,
        std::format("参考上下文过大（{} 字，上限 {} 字）",
                    reference_context.characters,
                    cnetboot::chat::max_reference_context_characters));
    co_return;
  case chat_context_status::absent:
  case chat_context_status::text:
    break;
  }

  const auto stream = body->stream.value_or(true);

  cnetboot::chat::chat_message_request request;
  request.content = *body->content;
  request.reference_context = reference_context.value;
  const auto actor = cnetboot::security::audit_actor_from(context, principal);

  if (!stream) {
    const auto turn = co_await chat.send_message(*id, request, actor);
    if (!turn) {
      web::respond_error(context, turn.error());
      co_return;
    }
    cnetboot::framework::web::respond_document(
        context,
        cnetmod::json::object(
            {{"message",
              cnetboot::chat::dto::to_document(
                  cnetboot::chat::dto::project(turn->assistant_message))}}));
    co_return;
  }

  // 会话不存在 / 参数非法在开始流之前判定：这些错误仍走统一错误体的 HTTP 响应，
  // 不把 404 塞进 200 的事件流里。
  const auto session = co_await chat.find_session(*id);
  if (!session) {
    web::respond_error(context, session.error());
    co_return;
  }
  co_await context.with_sse(
      [&](cnetmod::http::request_context &stream_context,
          cnetmod::http::sse_stream &stream) -> cnetmod::task<void> {
        // The shared writer owns the event contract; with_sse owns the
        // lifetime.
        web::sse_writer writer{stream};
        if (!co_await writer.begin())
          co_return;

        const auto turn = co_await chat.stream_message(
            *id, request, actor, web::sse_delta_callback(writer));
        if (!turn) {
          (void)co_await writer.error(turn.error());
          (void)co_await writer.finish();
          co_return;
        }
        (void)co_await writer.done(chat_done_payload(turn->assistant_message));
        (void)co_await writer.finish();
      });
  co_return;
}

} // namespace cnetboot::chat::detail
