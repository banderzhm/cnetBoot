module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chat.controller.chat_route_context;

import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chat.domain.chat_domain;
import cnetnexus.chat.service.chat_service;
import cnetnexus.chat.service.chat_service_factory;
import cnetnexus.framework.error;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chat {

using chat_service_resolver = std::function<std::expected<
    std::unique_ptr<chat_service_scope>, cnetnexus::framework::error::error_code>(
    const cnetmod::http::request_context&)>;

} // namespace cnetnexus::chat

export namespace cnetnexus::chat::detail {

/** @brief Route dependencies kept as non-owning pointers by registered handlers. */
struct chat_route_context {
  cnetnexus::chat::chat_service_resolver resolve_chat;
};

/** @brief chartContext 归一化结果。 */
enum class chat_context_status {
  absent,       ///< 未提供 / null / 空字符串 / 空对象 / 空数组：等价于不传
  text,         ///< 已归一化为文本（字符串原样，对象与数组取紧凑 JSON）
  invalid_type, ///< 数字、布尔等其它类型：按 REQUEST.INVALID_ARGUMENT 拒绝
  too_large,    ///< 归一化后的字符数超过 max_chart_context_characters
};

/** @brief chartContext 归一化结果：文本 + 实际字符数。 */
struct chat_context_field {
  chat_context_status status{chat_context_status::absent};
  std::string value; ///< status == text 时的文本
  std::size_t
      characters{}; ///< 归一化后的 Unicode 字符数（too_large 时给出实际值）
};

/**
 * @brief 归一化请求体里的 chartContext。
 *
 *  口径（与前端冻结）：**string | object | array** 都接受——字符串原样使用，
 *  对象/数组序列化成紧凑 JSON 文本再用；null 与空值（空串 / 空对象 / 空数组）
 *  等价于不传；数字、布尔等其它类型拒绝。上限按归一化之后的字符数计。
 */
[[nodiscard]] auto
normalize_chart_context(const std::optional<cnetmod::json::raw_value> &value)
    -> chat_context_field;

/** @brief delta 帧载荷：{"text":"增量"}（字段名属于已冻结契约，与断语共用）。
 */
[[nodiscard]] auto chat_delta_payload(std::string_view text) -> std::string;

/** @brief done
 * 帧载荷：{"messageId":..,"content":..,"tokensIn":..,"tokensOut":..}。 */
[[nodiscard]] auto
chat_done_payload(const cnetnexus::chat::chat_message &message) -> std::string;

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

} // namespace cnetnexus::chat::detail
