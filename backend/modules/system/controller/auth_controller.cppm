export module cnetboot.system.controller.auth_controller;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.web.rate_limit;
import cnetboot.system.service.auth_service;

export namespace cnetboot::system {

struct auth_route_dependencies
{
    std::shared_ptr<auth_service> service;
    std::shared_ptr<const cnetboot::framework::web::edge_options> edge;
};

void declare_auth_routes(cnetmod::http::router& routes,
    auth_route_dependencies dependencies);

} // namespace cnetboot::system

export namespace cnetboot::system::detail {

/** Registers route metadata; dependency validation stays in the public entry. */
void declare_auth_handlers(cnetmod::http::router& routes,
    auth_route_dependencies dependencies);

} // namespace cnetboot::system::detail
