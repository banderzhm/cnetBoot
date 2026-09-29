module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.reading_routes;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.reading_responder;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.sse_stream;
import cnetnexus.framework.web.rate_limit;
import cnetnexus.framework.web.route_handler;
import cnetnexus.reading.service.reading_use_case;

namespace cnetnexus::chart::detail {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief 六条 reading 共用的「开关 + 范围 + 问题」入口；失败时已写出错误响应。
[[nodiscard]] auto
accept_request(cnetmod::http::request_context &context,
               std::string_view algorithm, std::string_view query_text,
               std::optional<bool> &stream,
               cnetnexus::chart::reading_scope_request &scope) -> bool {
  const auto parsed = web::sse_requested(query_text);
  if (!parsed) {
    web::respond_error(context, parsed.error());
    return false;
  }
  stream = *parsed;
  return accept_scope(context, algorithm, query_text, scope);
}

/// @brief POST /api/v1/ba_zi/reading：子平报告 + 模型解读。
[[nodiscard]] auto
handle_bazi_reading_body(cnetmod::http::request_context &context,
                         const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "ba_zi", query_text, stream, scope))
    co_return;

  cnetnexus::chart::bazi::birth_input input{};
  if (!parse_calendar_input(query_text, input.year, input.month, input.day,
                            input.hour, input.minute, input.is_male, true)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  bool lunar = false;
  if (!parse_calendar_kind(query_text, lunar)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.calendar = lunar ? cnetnexus::chart::bazi::calendar_kind::lunar
                         : cnetnexus::chart::bazi::calendar_kind::solar;

  auto material = co_await services.charts->prepare(
      input, scope, question_of(query_text));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

/// @brief POST /api/v1/liu_yao/reading：六爻中文断卦 + 模型解读。
[[nodiscard]] auto
handle_liu_yao_reading_body(cnetmod::http::request_context &context,
                            const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "liu_yao", query_text, stream, scope))
    co_return;

  cnetnexus::chart::liu_yao::cast_input input{};
  int minute = 0;
  bool unused_gender = true; // 六爻用起卦时刻四柱，不需要性别
  if (!parse_calendar_input(query_text, input.year, input.month, input.day,
                            input.hour, minute, unused_gender, true)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.minute = minute;

  const auto code = web::query::value(query_text, "code");
  if (!code) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.hexagram_code = *code;
  if (const auto changing = web::query::integer_list(query_text, "changing");
      changing)
    input.changing_lines = *changing;
  if (const auto kind = web::query::value(query_text, "kind"); kind)
    input.kind = *kind;
  input.question = question_of(query_text);

  auto material = co_await services.charts->prepare(std::move(input));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

/// @brief POST /api/v1/mei_hua/reading：梅花中文占断 + 模型解读。
[[nodiscard]] auto
handle_mei_hua_reading_body(cnetmod::http::request_context &context,
                            const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "mei_hua", query_text, stream, scope))
    co_return;

  cnetnexus::chart::mei_hua::cast_input input{};
  bool unused_gender = true; // 梅花不需要性别，仅为复用解析函数占位
  if (!parse_calendar_input(query_text, input.year, input.month, input.day,
                            input.hour, input.minute, unused_gender, true)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  bool lunar = false;
  if (!parse_calendar_kind(query_text, lunar)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.calendar = lunar ? cnetnexus::chart::mei_hua::calendar_kind::lunar
                         : cnetnexus::chart::mei_hua::calendar_kind::solar;
  if (const auto kind = web::query::value(query_text, "kind"); kind)
    input.kind = *kind;
  input.question = question_of(query_text);

  auto material = co_await services.charts->prepare(std::move(input));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

/// @brief POST /api/v1/zi_wei/reading：紫微中文命盘 + 模型解读。
[[nodiscard]] auto
handle_zi_wei_reading_body(cnetmod::http::request_context &context,
                           const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "zi_wei", query_text, stream, scope))
    co_return;

  cnetnexus::chart::zi_wei::chart_input input{};
  int unused_minute = 0; // 紫微上游只到时辰粒度，分钟单独校验
  if (!parse_calendar_input(query_text, input.year, input.month, input.day,
                            input.hour, unused_minute, input.is_male, false)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  if (const auto minute = web::query::integer(query_text, "minute");
      minute && *minute != 0) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  bool lunar = false;
  if (!parse_calendar_kind(query_text, lunar)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  input.calendar = lunar ? cnetnexus::chart::zi_wei::calendar_kind::lunar
                         : cnetnexus::chart::zi_wei::calendar_kind::solar;

  auto material = co_await services.charts->prepare(
      input, scope, question_of(query_text));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

/// @brief POST /api/v1/da_liu_ren/reading：大六壬中文课体报告 + 模型解读。
[[nodiscard]] auto
handle_da_liu_ren_reading_body(cnetmod::http::request_context &context,
                               const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "da_liu_ren", query_text, stream, scope))
    co_return;

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
  input.question = question_of(query_text);
  if (const auto ben_ming = web::query::value(query_text, "ben_ming"); ben_ming)
    input.ben_ming = *ben_ming;
  if (const auto xing_nian = web::query::value(query_text, "xing_nian");
      xing_nian)
    input.xing_nian = *xing_nian;
  // 性别与出生信息与 /da_liu_ren/cast 同一套入参：性别决定行年方向与取类神。
  if (const auto gender = web::query::value(query_text, "gender"); gender)
    input.gender = *gender;
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

  auto material = co_await services.charts->prepare(std::move(input));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

/// @brief POST /api/v1/qi_men/reading：奇门中文盘面报告 + 模型解读。
[[nodiscard]] auto
handle_qi_men_reading_body(cnetmod::http::request_context &context,
                           const reading_route_context &services)
    -> cnetmod::task<void> {
  const auto query_text = context.query_string();
  std::optional<bool> stream;
  cnetnexus::chart::reading_scope_request scope;
  if (!accept_request(context, "qi_men", query_text, stream, scope))
    co_return;

  cnetnexus::chart::qi_men::chart_input input{};
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
  input.question = question_of(query_text);
  if (const auto nian_ming = web::query::value(query_text, "nian_ming");
      nian_ming)
    input.nian_ming = *nian_ming;

  auto material = co_await services.charts->prepare(std::move(input));
  if (!material) {
    web::respond_error(context, material.error());
    co_return;
  }
  co_await emit_reading(context, services, material->algorithm,
                        material->report_zh, material->question, *stream, scope);
  co_return;
}

} // namespace

auto handle_bazi_reading(cnetmod::http::request_context &context,
                         const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_bazi_reading_body(context, services);
}

auto handle_liu_yao_reading(cnetmod::http::request_context &context,
                            const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_liu_yao_reading_body(context, services);
}

auto handle_mei_hua_reading(cnetmod::http::request_context &context,
                            const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_mei_hua_reading_body(context, services);
}

auto handle_zi_wei_reading(cnetmod::http::request_context &context,
                           const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_zi_wei_reading_body(context, services);
}

auto handle_da_liu_ren_reading(cnetmod::http::request_context &context,
                               const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_da_liu_ren_reading_body(context, services);
}

auto handle_qi_men_reading(cnetmod::http::request_context &context,
                           const reading_route_context &services)
    -> cnetmod::task<void> {
  return handle_qi_men_reading_body(context, services);
}

void declare_reading_handlers(cnetmod::http::router &routes,
                              const reading_route_context &services) {
  // 解读会调用付费 LLM 并写缓存：非幂等，用 POST；受 reading 限流策略约束。
  // 参数仍走查询串（与排盘接口同一套解析）。
  namespace web = cnetnexus::framework::web;
  const auto policy = [] {
    return web::with_rate_limit(web::optional_identity(),
                                web::rate_limit_policy::reading);
  };
  using handler = web::route_handler<reading_route_context>;
  routes.post("/api/v1/ba_zi/reading", handler{services, handle_bazi_reading},
              policy());
  routes.post("/api/v1/liu_yao/reading",
              handler{services, handle_liu_yao_reading}, policy());
  routes.post("/api/v1/mei_hua/reading",
              handler{services, handle_mei_hua_reading}, policy());
  routes.post("/api/v1/zi_wei/reading",
              handler{services, handle_zi_wei_reading}, policy());
  routes.post("/api/v1/da_liu_ren/reading",
              handler{services, handle_da_liu_ren_reading}, policy());
  routes.post("/api/v1/qi_men/reading",
              handler{services, handle_qi_men_reading}, policy());
}

} // namespace cnetnexus::chart::detail

namespace cnetnexus::chart {

void declare_reading_routes(cnetmod::http::router &routes,
                            cnetnexus::chart::chart_reading_service &charts,
                            cnetnexus::reading::reading_use_case &reading) {
  const cnetnexus::chart::detail::reading_route_context services{
      &charts, &reading};
  cnetnexus::chart::detail::declare_reading_handlers(routes, services);
}

} // namespace cnetnexus::chart
