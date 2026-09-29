module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.liu_yao_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief GET /api/v1/liu_yao/cast：六爻装卦与断卦。
auto handle_liu_yao_cast(cnetmod::http::request_context &context,
                         cnetnexus::chart::liu_yao_service &service)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  cnetnexus::chart::liu_yao::cast_input input{};

  const auto year = web::query::integer(query_text, "year");
  const auto month = web::query::integer(query_text, "month");
  const auto day = web::query::integer(query_text, "day");
  const auto hour = web::query::integer(query_text, "hour");
  const auto code = web::query::value(query_text, "code");
  if (!year || !month || !day || !hour || !code) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.year = *year;
  input.month = *month;
  input.day = *day;
  input.hour = *hour;
  input.minute = web::query::integer(query_text, "minute").value_or(0);
  input.hexagram_code = *code;

  const auto changing = web::query::integer_list(query_text, "changing");
  if (!changing) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.changing_lines = *changing;

  if (const auto kind = web::query::value(query_text, "kind"); kind)
    input.kind = *kind;
  if (const auto question = web::query::value(query_text, "question"); question)
    input.question = *question;

  // yong_shen：人工指定用神爻位（1—6）。给了就必须合法，不静默忽略、不回退自动取用。
  if (web::query::value(query_text, "yong_shen")) {
    const auto position = web::query::integer(query_text, "yong_shen");
    if (!position || *position < 1 || *position > 6) {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
    input.yong_shen_position = *position;
  }

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

void declare_liu_yao_routes(cnetmod::http::router &routes,
                            cnetnexus::chart::liu_yao_service &service) {
  routes.get("/api/v1/liu_yao/cast",
             cnetnexus::framework::web::service_route_handler<liu_yao_service>{
                 service, handle_liu_yao_cast},
             cnetnexus::framework::web::anonymous());
}

} // namespace cnetnexus::chart
