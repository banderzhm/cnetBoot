module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.da_liu_ren_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/da_liu_ren/cast：大六壬起课与占断。
auto handle_da_liu_ren_cast(cnetmod::http::request_context &context,
                            cnetnexus::chart::da_liu_ren_service &service)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  cnetnexus::chart::da_liu_ren::cast_input input{};

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
  if (const auto ben_ming = web::query::value(query_text, "ben_ming"); ben_ming)
    input.ben_ming = *ben_ming;
  if (const auto xing_nian = web::query::value(query_text, "xing_nian");
      xing_nian)
    input.xing_nian = *xing_nian;
  if (const auto gender = web::query::value(query_text, "gender"); gender)
    input.gender = *gender;
  // 出生时刻（可选）：齐备时后端自动推本命/行年。
  if (const auto value = web::query::integer(query_text, "birth_year"); value)
    input.birth_year = *value;
  if (const auto value = web::query::integer(query_text, "birth_month"); value)
    input.birth_month = *value;
  if (const auto value = web::query::integer(query_text, "birth_day"); value)
    input.birth_day = *value;
  if (const auto value = web::query::integer(query_text, "birth_hour"); value)
    input.birth_hour = *value;
  if (const auto rule = web::query::value(query_text, "xing_nian_rule"); rule)
    input.xing_nian_rule = *rule;

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

void declare_da_liu_ren_routes(cnetmod::http::router &routes,
                               cnetnexus::chart::da_liu_ren_service &service) {
  routes.get(
      "/api/v1/da_liu_ren/cast",
      cnetnexus::framework::web::service_route_handler<da_liu_ren_service>{
          service, handle_da_liu_ren_cast},
      cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::chart
