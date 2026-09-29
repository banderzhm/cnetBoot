module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chat.controller.chat_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chat.controller.chat_route_context;
import cnetnexus.chat.service.chat_service_factory;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chat {

void declare_chat_routes(
    cnetmod::http::router &routes,
    cnetnexus::chat::chat_service_resolver resolve_chat) {
  if (!resolve_chat)
    throw std::invalid_argument{"chat routes require a service resolver"};
  cnetnexus::chat::detail::declare_chat_handlers(
      routes, cnetnexus::chat::detail::chat_route_context{
                  std::move(resolve_chat)});
}

} // namespace cnetnexus::chat
