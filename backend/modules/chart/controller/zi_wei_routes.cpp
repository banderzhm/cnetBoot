module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.zi_wei_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/zi_wei/chart：紫微斗数排盘。
auto handle_zi_wei_chart(cnetmod::http::request_context &context,
                         cnetnexus::chart::zi_wei_service &service)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  cnetnexus::chart::zi_wei::chart_input input{};

  if (const auto calendar = web::query::value(query_text, "calendar");
      calendar) {
    if (*calendar == "solar")
      input.calendar = cnetnexus::chart::zi_wei::calendar_kind::solar;
    else if (*calendar == "lunar")
      input.calendar = cnetnexus::chart::zi_wei::calendar_kind::lunar;
    else {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
  }

  // 上游紫微接口只到时辰粒度：显式传入非零 minute 一律拒绝，不做静默截断。
  if (const auto minute = web::query::integer(query_text, "minute");
      minute && *minute != 0) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
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

  if (const auto gender = web::query::value(query_text, "gender"); gender) {
    if (*gender == "male")
      input.is_male = true;
    else if (*gender == "female")
      input.is_male = false;
    else {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
  }

  // 运限参数：非数字在这里拒绝，越界与成套语义由 service 统一拒绝。
  if (!optional_int_param(query_text, "target_year", 1, 9999,
                          input.target_year) ||
      !optional_int_param(query_text, "current_age", 1, 9999,
                          input.current_age) ||
      !optional_int_param(query_text, "target_lunar_month", 1, 12,
                          input.target_lunar_month) ||
      !optional_int_param(query_text, "target_lunar_day", 1, 30,
                          input.target_lunar_day)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  // 时辰地支汉字原样透传：汉字→地支的唯一映射在上游适配层，本层不建对照表。
  if (const auto target_hour = web::query::value(query_text, "target_hour");
      target_hour)
    input.target_hour = std::string{*target_hour};

  const auto result = co_await service.pai_pan(input);
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

void declare_zi_wei_routes(cnetmod::http::router &routes,
                           cnetnexus::chart::zi_wei_service &service) {
  routes.get("/api/v1/zi_wei/chart",
             cnetnexus::framework::web::service_route_handler<zi_wei_service>{
                 service, handle_zi_wei_chart},
             cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::chart
