module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.catalog_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.service.catalog_service;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/capabilities：算法集合唯一来源是 domain.catalog。
auto handle_capabilities(cnetmod::http::request_context &context)
    -> cnetmod::task<void> {
  const auto summary = cnetnexus::chart::capabilities();
  auto algorithms = cnetmod::json::array();
  algorithms.get_array().reserve(summary.algorithms.size());
  for (const auto &key : summary.algorithms)
    algorithms.get_array().push_back(key);
  cnetnexus::framework::web::respond_document(
      context, cnetmod::json::object({{"algorithms", std::move(algorithms)}}));
  co_return;
}

/// @brief GET /api/v1/error_codes：注册表唯一来源是 framework.error。
auto handle_error_codes(cnetmod::http::request_context &context)
    -> cnetmod::task<void> {
  const auto registry_body = web::error_registry_body();
  auto errors = cnetmod::json::array();
  errors.get_array().reserve(registry_body.errors.size());
  for (const auto &entry : registry_body.errors)
    errors.get_array().push_back(
        cnetmod::json::object({{"status", entry.status},
                               {"httpStatus", entry.httpStatus},
                               {"message", entry.message},
                               {"owner", entry.owner},
                               {"category", entry.category},
                               {"retryable", entry.retryable},
                               {"exposable", entry.exposable},
                               {"deprecated", entry.deprecated}}));
  cnetnexus::framework::web::respond_document(
      context, cnetmod::json::object({{"errors", std::move(errors)}}));
  co_return;
}

} // namespace

void declare_catalog_routes(cnetmod::http::router &routes) {
  routes.get("/api/v1/capabilities",
             web::stateless_route_handler{handle_capabilities},
             web::anonymous());
  routes.get("/api/v1/error_codes",
             web::stateless_route_handler{handle_error_codes},
             web::anonymous());
}

} // namespace cnetnexus::chart
