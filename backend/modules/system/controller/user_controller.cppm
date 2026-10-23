export module cnetboot.system.controller.user_controller;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.route_handler;
import cnetboot.system.service.user_service;

export namespace cnetboot::system {

using user_service_resolver = std::function<std::expected<
    std::unique_ptr<user_service>, cnetboot::framework::error::error_code>(
    const cnetmod::http::request_context&)>;


void declare_user_routes(cnetmod::http::router& routes,
    user_service_resolver resolve_service);

} // namespace cnetboot::system
