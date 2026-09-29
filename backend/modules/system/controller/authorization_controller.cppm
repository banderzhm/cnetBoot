export module cnetnexus.system.controller.authorization_controller;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.web.route_handler;
import cnetnexus.system.service.authorization_service;

export namespace cnetnexus::system {

/** @brief 声明用户授权快照路由；由组合根统一装配。 */
void declare_authorization_routes(
    cnetmod::http::router& routes,
    std::shared_ptr<authorization_service> service);

} // namespace cnetnexus::system
