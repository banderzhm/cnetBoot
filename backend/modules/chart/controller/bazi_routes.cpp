module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.bazi_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.json;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart::detail {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

auto parse_bazi_birth(std::string_view query_text,
                      cnetnexus::chart::bazi::birth_input &input) -> bool {
  if (const auto calendar = web::query::value(query_text, "calendar");
      calendar) {
    if (*calendar == "solar")
      input.calendar = cnetnexus::chart::bazi::calendar_kind::solar;
    else if (*calendar == "lunar")
      input.calendar = cnetnexus::chart::bazi::calendar_kind::lunar;
    else
      return false;
  }

  const auto year = web::query::integer(query_text, "year");
  const auto month = web::query::integer(query_text, "month");
  const auto day = web::query::integer(query_text, "day");
  const auto hour = web::query::integer(query_text, "hour");
  if (!year || !month || !day || !hour)
    return false;
  input.year = *year;
  input.month = *month;
  input.day = *day;
  input.hour = *hour;
  input.minute = web::query::integer(query_text, "minute").value_or(0);

  if (const auto gender = web::query::value(query_text, "gender"); gender) {
    if (*gender == "male")
      input.is_male = true;
    else if (*gender == "female")
      input.is_male = false;
    else
      return false;
  }

  if (const auto method = web::query::value(query_text, "method"); method) {
    if (*method == "ziping")
      input.method = cnetnexus::chart::bazi::analysis_method::ziping;
    else if (*method == "mangpai")
      input.method = cnetnexus::chart::bazi::analysis_method::mangpai;
    else
      return false;
  }
  return true;
}

namespace {

/// @brief GET /api/v1/bazi/chart：八字排盘。
auto handle_bazi_chart(cnetmod::http::request_context &context,
                       cnetnexus::chart::bazi_service &service)
    -> cnetmod::task<void> {
  cnetnexus::chart::bazi::birth_input input{};
  if (!parse_bazi_birth(context.query_string(), input)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  const auto chart = co_await service.pai_pan(input);
  if (!chart) {
    web::respond_error(context, chart.error());
    co_return;
  }
  auto body = cnetnexus::chart::projection::chart_response(*chart);
  if (!body) {
    web::respond_error(context, error::error_code::system_internal_error);
    co_return;
  }
  cnetnexus::framework::web::respond_document(context, std::move(*body));
  co_return;
}

/// @brief GET /api/v1/bazi/liu_nian：按目标公历年取流年盘面。
auto handle_bazi_liu_nian(cnetmod::http::request_context &context,
                          cnetnexus::chart::bazi_service &service)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  cnetnexus::chart::bazi::liu_nian_input input{};
  if (!parse_bazi_birth(query_text, input.birth)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  const auto target_year = web::query::integer(query_text, "target_year");
  if (!target_year) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.target_year = *target_year;
  if (const auto da_yun = web::query::integer(query_text, "da_yun"); da_yun)
    input.da_yun_index = *da_yun;

  const auto result = co_await service.liu_nian(input);
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, result->chart_projection);
  co_return;
}

} // namespace

void declare_bazi_handlers(cnetmod::http::router &routes,
                           cnetnexus::chart::bazi_service &service) {
  using handler =
      cnetnexus::framework::web::service_route_handler<bazi_service>;
  routes.get("/api/v1/bazi/chart", handler{service, handle_bazi_chart},
             cnetnexus::framework::web::anonymous());
  routes.get("/api/v1/bazi/liu_nian", handler{service, handle_bazi_liu_nian},
             cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::chart::detail

namespace cnetnexus::chart {

void declare_bazi_routes(cnetmod::http::router &routes,
                         cnetnexus::chart::bazi_service &service) {
  detail::declare_bazi_handlers(routes, service);
}

} // namespace cnetnexus::chart
