export module cnetboot.system.controller.authorization_controller;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.web.route_handler;
import cnetboot.system.service.authorization_service;

export namespace cnetboot::system {

/** @brief 声明用户授权快照路由；由组合根统一装配。 */
void declare_authorization_routes(
    cnetmod::http::router& routes,
    std::shared_ptr<authorization_service> service);

} // namespace cnetboot::system
