module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.reading_routes;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.reading_responder;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.chart.controller.route_support;
import cnetnexus.framework.error;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.security.current_user;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;

namespace cnetnexus::chart::detail {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

auto question_of(std::string_view query_text) -> std::string {
  return web::query::value(query_text, "question").value_or("");
}

auto parse_calendar_input(std::string_view query_text, int &year, int &month,
                          int &day, int &hour, int &minute, bool &is_male,
                          bool with_minute) -> bool {
  const auto parsed_year = web::query::integer(query_text, "year");
  const auto parsed_month = web::query::integer(query_text, "month");
  const auto parsed_day = web::query::integer(query_text, "day");
  const auto parsed_hour = web::query::integer(query_text, "hour");
  if (!parsed_year || !parsed_month || !parsed_day || !parsed_hour)
    return false;
  year = *parsed_year;
  month = *parsed_month;
  day = *parsed_day;
  hour = *parsed_hour;
  minute =
      with_minute ? web::query::integer(query_text, "minute").value_or(0) : 0;

  if (const auto gender = web::query::value(query_text, "gender"); gender) {
    if (*gender == "male")
      is_male = true;
    else if (*gender == "female")
      is_male = false;
    else
      return false;
  }
  return true;
}

auto parse_calendar_kind(std::string_view query_text, bool &lunar) -> bool {
  if (const auto calendar = web::query::value(query_text, "calendar");
      calendar) {
    if (*calendar == "lunar")
      lunar = true;
    else if (*calendar != "solar")
      return false;
  }
  return true;
}

auto accept_scope(cnetmod::http::request_context &context,
                  std::string_view algorithm, std::string_view query_text,
                  cnetnexus::chart::reading_scope_request &scope) -> bool {
  const auto parsed =
      cnetnexus::chart::parse_reading_scope_query(algorithm, query_text);
  if (!parsed) {
    web::respond_error(context, parsed.error().code, parsed.error().message);
    return false;
  }
  scope = *parsed;
  return true;
}

auto emit_reading(cnetmod::http::request_context &context,
                  const reading_route_context &services,
                  std::string_view algorithm, std::string_view report_zh,
                  std::string_view question, bool stream,
                  const cnetnexus::chart::reading_scope_request &scope)
    -> cnetmod::task<void> {
  // 缓存分区用户来自可选 Bearer：未带令牌（jwt
  // 中间件不拦公开端点）或未登录一律按匿名 0。
  std::uint64_t user_id = 0;
  if (const auto *principal = cnetnexus::chart::current_principal(context))
    user_id = static_cast<std::uint64_t>(principal->id);

  cnetnexus::chart::reading_response_request request;
  request.user_id = user_id;
  request.algorithm = std::string{algorithm};
  request.report_zh = std::string{report_zh};
  request.question = std::string{question};
  request.stream = stream;
  request.scope = scope;
  co_await cnetnexus::chart::respond_reading(context, *services.reading,
                                             request);
}

} // namespace cnetnexus::chart::detail
