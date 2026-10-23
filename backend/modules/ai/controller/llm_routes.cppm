
export module cnetboot.ai.controller.llm_routes;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.web.route_handler;
import cnetboot.framework.config.llm_settings;

export namespace cnetboot::ai {


void declare_llm_routes(cnetmod::http::router& routes,
    std::shared_ptr<const cnetboot::framework::config::llm_directory> directory);

} // namespace cnetboot::ai
