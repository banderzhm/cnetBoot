export module cnetnexus.system.controller.auth_controller;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.web.rate_limit;
import cnetnexus.system.service.auth_service;

export namespace cnetnexus::system {

struct auth_route_dependencies
{
    std::shared_ptr<auth_service> service;
    std::shared_ptr<const cnetnexus::framework::web::edge_options> edge;
};

void declare_auth_routes(cnetmod::http::router& routes,
    auth_route_dependencies dependencies);

} // namespace cnetnexus::system

export namespace cnetnexus::system::detail {

/** Registers route metadata; dependency validation stays in the public entry. */
void declare_auth_handlers(cnetmod::http::router& routes,
    auth_route_dependencies dependencies);

} // namespace cnetnexus::system::detail
