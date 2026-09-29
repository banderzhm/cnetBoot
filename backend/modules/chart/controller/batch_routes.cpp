module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.batch_routes;

import std;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.service.chart_generation_service;
import cnetnexus.framework.error;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.response;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {
namespace {

auto handle_chart_generation(cnetmod::http::request_context &context,
                             const chart_generation_service &service)
    -> cnetmod::task<void> {
  auto request = cnetmod::json::parse_lenient<dto::chart_batch_request>(
      co_await context.read_full_body());
  if (!request || !request->items) {
    framework::web::respond_error(
        context, framework::error::error_code::request_invalid_argument);
    co_return;
  }
  if (const auto violation = batch_limit_violation(request->items->size())) {
    framework::web::respond_error(
        context, framework::error::error_code::request_invalid_argument,
        *violation);
    co_return;
  }
  std::uint64_t owner_id = 0;
  if (const auto *principal = current_principal(context))
    owner_id = static_cast<std::uint64_t>(principal->id);
  auto result = co_await service.execute(*request, owner_id);
  if (!result) {
    framework::web::respond_error(context, result.error());
    co_return;
  }
  framework::web::respond_document(context, dto::to_document(*result));
}

} // namespace

void declare_batch_routes(cnetmod::http::router &routes,
                          chart_generation_service &service) {
  routes.post("/api/v1/charts/batch",
              framework::web::route_handler<chart_generation_service>{
                  service, handle_chart_generation},
              cnetnexus::security::requires_permission("chart:batch:run"));
}

} // namespace cnetnexus::chart
