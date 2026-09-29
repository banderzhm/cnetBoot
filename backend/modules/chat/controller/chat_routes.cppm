module;

#include <cnetmod/json_codec.hpp>

export module cnetboot.chat.controller.chat_routes;

import std;
import cnetmod.protocol.http;
import cnetboot.chat.controller.chat_route_context;
import cnetboot.chat.service.chat_service;
import cnetboot.chat.service.chat_service_factory;
import cnetboot.framework.web.route_handler;

export namespace cnetboot::chat {

/** @brief 声明多轮对话路由（全部需要 Bearer 令牌）：
 *  - POST   /api/v1/chat/sessions 新建会话（chat:session:create）
 *  - GET    /api/v1/chat/sessions?category=&page=&page_size=
 *                                                     会话列表（chat:session:list）
 *  - GET    /api/v1/chat/sessions/:id 会话详情（chat:session:list）
 *  - PATCH  /api/v1/chat/sessions/:id
 * 改标题/提示词/模型/温度（chat:session:update）
 *  - DELETE /api/v1/chat/sessions/:id 删除会话（chat:session:delete）
 *  - GET    /api/v1/chat/sessions/:id/messages 消息分页（chat:session:list）
 *  - POST   /api/v1/chat/sessions/:id/messages 发送消息（chat:message:send）
 *
 *  发送消息 stream=true（默认）走 text/event-stream：delta / done / error
 * 三种具名事件； stream=false 直接返回
 * {message:{...}}。鉴权、参数与会话存在性都在开始流之前判定，
 *  这些错误仍以统一错误体的 HTTP 响应返回，而不是 200 的流。
 */
void declare_chat_routes(
    cnetmod::http::router &routes,
    cnetboot::chat::chat_service_resolver resolve_chat);

} // namespace cnetboot::chat
