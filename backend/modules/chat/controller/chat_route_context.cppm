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

/** @brief context 归一化结果。 */
enum class chat_context_status {
  absent,       ///< 未提供 / null / 空字符串 / 空对象 / 空数组：等价于不传
  text,         ///< 已归一化为文本（字符串原样，对象与数组取紧凑 JSON）
  invalid_type, ///< 数字、布尔等其它类型：按 REQUEST.INVALID_ARGUMENT 拒绝
  too_large,    ///< 归一化后的字符数超过 max_reference_context_characters
};

/** @brief context 归一化结果：文本 + 实际字符数。 */
struct chat_context_field {
  chat_context_status status{chat_context_status::absent};
  std::string value; ///< status == text 时的文本
  std::size_t
      characters{}; ///< 归一化后的 Unicode 字符数（too_large 时给出实际值）
};

/**
 * @brief 归一化请求体里的 context。
 *
 *  口径（与前端冻结）：**string | object | array** 都接受——字符串原样使用，
 *  对象/数组序列化成紧凑 JSON 文本再用；null 与空值（空串 / 空对象 / 空数组）
 *  等价于不传；数字、布尔等其它类型拒绝。上限按归一化之后的字符数计。
 */
[[nodiscard]] auto
normalize_reference_context(const std::optional<cnetmod::json::raw_value> &value)
    -> chat_context_field;

/** @brief delta 帧载荷：{"text":"增量"}（字段名属于已冻结契约，与断语共用）。
 */
[[nodiscard]] auto chat_delta_payload(std::string_view text) -> std::string;

/** @brief done
 * 帧载荷：{"messageId":..,"content":..,"tokensIn":..,"tokensOut":..}。 */
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

/** @brief 逐路由声明（权限装饰在本处完成），由组合根统一装配。 */
void declare_chat_handlers(cnetmod::http::router &routes,
                           const chat_route_context &services);

} // namespace cnetboot::chat::detail
