module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.stats_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.service.stats_service;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/stats/overview：登录即可读取的概览统计。
auto handle_stats_overview(cnetmod::http::request_context &context,
                           cnetnexus::chart::stats_service &stats)
    -> cnetmod::task<void> {
  const auto overview = co_await stats.overview();
  if (!overview) {
    web::respond_error(context, overview.error());
    co_return;
  }
  auto body = cnetnexus::chart::dto::project(*overview);
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(body));
  co_return;
}

} // namespace

void declare_stats_routes(cnetmod::http::router &routes,
                          cnetnexus::chart::stats_service &stats) {
  routes.get(
      "/api/v1/stats/overview",
      web::service_route_handler<stats_service>{stats, handle_stats_overview});
}

} // namespace cnetnexus::chart
