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


void declare_chat_routes(
    cnetmod::http::router &routes,
    cnetboot::chat::chat_service_resolver resolve_chat);

} // namespace cnetboot::chat
