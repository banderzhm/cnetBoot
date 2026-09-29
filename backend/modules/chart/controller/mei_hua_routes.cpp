module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.mei_hua_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/mei_hua/cast：梅花易数时间起卦与占断。
auto handle_mei_hua_cast(cnetmod::http::request_context &context,
                         cnetnexus::chart::mei_hua_service &service)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  cnetnexus::chart::mei_hua::cast_input input{};

  if (const auto calendar = web::query::value(query_text, "calendar");
      calendar) {
    if (*calendar == "solar")
      input.calendar = cnetnexus::chart::mei_hua::calendar_kind::solar;
    else if (*calendar == "lunar")
      input.calendar = cnetnexus::chart::mei_hua::calendar_kind::lunar;
    else {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
  }

  const auto year = web::query::integer(query_text, "year");
  const auto month = web::query::integer(query_text, "month");
  const auto day = web::query::integer(query_text, "day");
  const auto hour = web::query::integer(query_text, "hour");
  if (!year || !month || !day || !hour) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.year = *year;
  input.month = *month;
  input.day = *day;
  input.hour = *hour;
  input.minute = web::query::integer(query_text, "minute").value_or(0);

  if (const auto kind = web::query::value(query_text, "kind"); kind)
    input.kind = *kind;
  if (const auto question = web::query::value(query_text, "question"); question)
    input.question = *question;

  const auto result = co_await service.cast(input);
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  auto body = cnetnexus::chart::projection::chart_response(*result);
  if (!body) {
    web::respond_error(context, error::error_code::system_internal_error);
    co_return;
  }
  cnetnexus::framework::web::respond_document(context, std::move(*body));
  co_return;
}

} // namespace

void declare_mei_hua_routes(cnetmod::http::router &routes,
                            cnetnexus::chart::mei_hua_service &service) {
  routes.get("/api/v1/mei_hua/cast",
             cnetnexus::framework::web::service_route_handler<mei_hua_service>{
                 service, handle_mei_hua_cast},
             cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::chart
