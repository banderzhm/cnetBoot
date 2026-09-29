/**
 * @brief llm_routes 的实现：把 llm_catalog_json 作为响应体直接下发。
 *
 * 走 cnetmod 的 context.json(status, body)（与 chart 的 respond_document
 * 同一口径）， 因此 content-type / 状态码由框架统一设置，不需要手工拼响应头。
 */
module cnetnexus.reading.controller.llm_routes;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.framework.web.route_handler;
import cnetnexus.reading.service.llm_catalog;
import cnetnexus.framework.config.llm_settings;

namespace cnetnexus::reading {
namespace {

using directory_ptr =
    std::shared_ptr<const cnetnexus::framework::config::llm_directory>;

auto handle_llm_providers(cnetmod::http::request_context &context,
                          const directory_ptr &directory)
    -> cnetmod::task<void> {
  context.json(cnetmod::http::status::ok, llm_catalog_json(*directory));
  co_return;
}

} // namespace

void declare_llm_routes(cnetmod::http::router &routes,
                        directory_ptr directory) {
  if (!directory)
    throw std::invalid_argument("llm routes require a directory snapshot");
  routes.get("/api/v1/llm/providers",
             cnetnexus::framework::web::route_handler<directory_ptr>{
                 std::move(directory), handle_llm_providers},
             cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::reading
