export module cnetnexus.system.controller.user_controller;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.error;
import cnetnexus.framework.web.route_handler;
import cnetnexus.system.service.user_service;

export namespace cnetnexus::system {

using user_service_resolver = std::function<std::expected<
    std::unique_ptr<user_service>, cnetnexus::framework::error::error_code>(
    const cnetmod::http::request_context&)>;

/** @brief 声明系统用户 HTTP 路由；由组合根统一装配。 */
void declare_user_routes(cnetmod::http::router& routes,
    user_service_resolver resolve_service);

} // namespace cnetnexus::system
