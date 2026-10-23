
module cnetboot.ai.controller.llm_routes;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetboot.framework.web.route_handler;
import cnetboot.framework.web.response;
import cnetboot.ai.service.llm_catalog;
import cnetboot.framework.config.llm_settings;

namespace cnetboot::ai {
namespace {

using directory_ptr =
    std::shared_ptr<const cnetboot::framework::config::llm_directory>;

auto handle_llm_providers(cnetmod::http::request_context &context,
                          const directory_ptr &directory)
    -> cnetmod::task<void> {
  cnetboot::framework::web::respond_document(
      context, llm_catalog_document(*directory));
  co_return;
}

} // namespace

void declare_llm_routes(cnetmod::http::router &routes,
                        directory_ptr directory) {
  if (!directory)
    throw std::invalid_argument("llm routes require a directory snapshot");
  routes.get("/api/v1/llm/providers",
             cnetboot::framework::web::route_handler<directory_ptr>{
                 std::move(directory), handle_llm_providers},
             cnetboot::framework::web::anonymous());
}

} // namespace cnetboot::ai
