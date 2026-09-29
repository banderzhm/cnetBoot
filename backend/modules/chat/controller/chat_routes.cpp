module;

#include <cnetmod/json_codec.hpp>

module cnetboot.chat.controller.chat_routes;

import std;
import cnetmod.protocol.http;
import cnetboot.chat.controller.chat_route_context;
import cnetboot.chat.service.chat_service_factory;
import cnetboot.framework.web.route_handler;

namespace cnetboot::chat {

void declare_chat_routes(
    cnetmod::http::router &routes,
    cnetboot::chat::chat_service_resolver resolve_chat) {
  if (!resolve_chat)
    throw std::invalid_argument{"chat routes require a service resolver"};
  cnetboot::chat::detail::declare_chat_handlers(
      routes, cnetboot::chat::detail::chat_route_context{
                  std::move(resolve_chat)});
}

} // namespace cnetboot::chat
