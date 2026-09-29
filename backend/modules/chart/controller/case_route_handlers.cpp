module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.case_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.controller.reading_responder;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.service.case_service;
import cnetnexus.framework.error;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.security.current_user;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.sse_stream;

namespace cnetnexus::chart::detail {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

/// @brief 由请求体组装案例写入请求；返回 std::nullopt 表示字段类型非法。
[[nodiscard]] auto
case_request_from(const cnetnexus::chart::dto::chart_case_request &body)
    -> std::optional<cnetnexus::chart::case_write_request> {
  cnetnexus::chart::case_write_request request;
  auto &value = request.value;
  value.title = body.title.value_or("");
  if (body.algorithm && !body.algorithm->empty()) {
    value.algorithm = cnetnexus::chart::canonical_algorithm(*body.algorithm);
    request.has_algorithm = true;
  }
  if (body.archive_id) {
    const auto parsed = cnetnexus::chart::nullable_text(*body.archive_id);
    if (!parsed)
      return std::nullopt;
    value.archive_id = *parsed;
  }
  value.calendar = body.calendar.value_or("solar");
  value.gender = body.gender.value_or("");
  value.birth_year = body.birth_year.value_or(0);
  value.birth_month = body.birth_month.value_or(0);
  value.birth_day = body.birth_day.value_or(0);
  value.birth_hour = body.birth_hour.value_or(0);
  value.birth_minute = body.birth_minute.value_or(0);
  value.birth_place = body.birth_place.value_or("");
  value.client_name = body.client_name.value_or("");
  value.question = body.question.value_or("");
  if (body.notes) {
    const auto parsed = cnetnexus::chart::nullable_text(*body.notes);
    if (!parsed)
      return std::nullopt;
    value.notes = *parsed;
  }
  request.has_reading = body.reading.has_value();
  if (body.reading) {
    const auto parsed = cnetnexus::chart::nullable_text(*body.reading);
    if (!parsed)
      return std::nullopt;
    value.reading = *parsed;
  }
  request.has_payload = body.payload.has_value() || body.chart.has_value();
  if (body.chart) {
    // chart 只接受 JSON 对象文本（快照本体），不接受标量。
    if (!body.chart->is_object())
      return std::nullopt;
    const auto encoded = cnetmod::json::write_document(*body.chart);
    if (!encoded)
      return std::nullopt;
    value.payload = *encoded;
  } else if (body.payload) {
    const auto parsed = cnetnexus::chart::nullable_text(*body.payload);
    if (!parsed)
      return std::nullopt;
    value.payload = *parsed;
  }
  request.has_tag_ids = body.tag_ids.has_value();
  if (body.tag_ids) {
    const auto parsed = cnetnexus::chart::id_list(*body.tag_ids);
    if (!parsed)
      return std::nullopt;
    request.tag_ids = *parsed;
  }
  return request;
}

} // namespace

auto handle_case_list(cnetmod::http::request_context &context,
                      const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto raw = context.query_string();
  cnetnexus::chart::case_query filter;
  filter.keyword = web::query::value(raw, "keyword").value_or("");
  if (const auto algorithm = web::query::value(raw, "algorithm"); algorithm)
    filter.algorithm = cnetnexus::chart::canonical_algorithm(*algorithm);
  filter.tag_id = static_cast<std::uint64_t>(
      web::query::integer(raw, "tag_id").value_or(0));
  filter.page = cnetnexus::chart::page_from(raw);

  const auto result = co_await (*scope)->service.list(filter);
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  const auto page =
      cnetnexus::chart::dto::project_chart_page(*result, [](const auto &value) {
        return cnetnexus::chart::dto::project(value);
      });
  cnetnexus::framework::web::respond_document(
      context,
      cnetnexus::chart::dto::page_document(
          page, [](const cnetnexus::chart::dto::chart_case_response &value) {
            return cnetnexus::chart::dto::to_document(value);
          }));
  co_return;
}

auto handle_case_create(cnetmod::http::request_context &context,
                        const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto *principal = cnetnexus::chart::current_principal(context);
  auto body =
      cnetmod::json::parse_lenient<cnetnexus::chart::dto::chart_case_request>(
          co_await context.read_full_body());
  if (!body) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto request = case_request_from(*body);
  if (!request) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto created = co_await (*scope)->service.create(
      *request, cnetnexus::chart::audit_actor_from(context, principal));
  if (!created) {
    web::respond_error(context, created.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::project(*created)));
  co_return;
}

auto handle_case_find(cnetmod::http::request_context &context,
                      const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto found = co_await (*scope)->service.find(*id);
  if (!found) {
    web::respond_error(context, found.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::project(*found)));
  co_return;
}

auto handle_case_update(cnetmod::http::request_context &context,
                        const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto *principal = cnetnexus::chart::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  auto body =
      cnetmod::json::parse_lenient<cnetnexus::chart::dto::chart_case_request>(
          co_await context.read_full_body());
  if (!id || !body) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  auto request = case_request_from(*body);
  if (!request) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  request->value.id = *id;
  const auto updated = co_await (*scope)->service.update(
      *request, cnetnexus::chart::audit_actor_from(context, principal));
  if (!updated) {
    web::respond_error(context, updated.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::project(*updated)));
  co_return;
}

auto handle_case_remove(cnetmod::http::request_context &context,
                        const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto *principal = cnetnexus::chart::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto removed = co_await (*scope)->service.remove(
      *id, cnetnexus::chart::audit_actor_from(context, principal));
  if (!removed) {
    web::respond_error(context, removed.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::success_response{}));
  co_return;
}

auto handle_case_reading(cnetmod::http::request_context &context,
                         const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto stream = web::sse_requested(context.query_string());
  if (!stream) {
    web::respond_error(context, stream.error());
    co_return;
  }
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto found = co_await (*scope)->service.find(*id);
  if (!found) {
    web::respond_error(context, found.error());
    co_return;
  }
  const auto report = co_await (*scope)->service.report_text(*found);
  if (!report) {
    web::respond_error(context, report.error());
    co_return;
  }
  if (!report->has_value()) {
    // 案例既没有排盘快照也没有可用存档：没有可解读的素材。
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  // 断语写回案例（尽力而为）：失败不影响已发出的响应。
  std::optional<std::string> reading_content;
  const auto save_reading = [&reading_content](std::string_view content) {
    reading_content = std::string{content};
  };

  cnetnexus::chart::reading_response_request request;
  request.user_id = 0;
  if (const auto *principal = cnetnexus::chart::current_principal(context))
    request.user_id = static_cast<std::uint64_t>(principal->id);
  /// 算法维度用案例所属算法：与 /api/v1/<algo>/reading
  /// 同命名空间，两个入口可互相命中。
  request.algorithm = found->algorithm;
  request.report_zh = **report;
  request.question = found->question;
  request.stream = *stream;

  co_await cnetnexus::chart::respond_reading(context, *services.reading,
                                             request, save_reading);
  if (reading_content)
    (void)co_await (*scope)->service.save_reading(*id,
                                                  std::move(*reading_content));
  co_return;
}

auto handle_tag_list(cnetmod::http::request_context &context,
                     const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto tags = co_await (*scope)->service.list_tags();
  if (!tags) {
    web::respond_error(context, tags.error());
    co_return;
  }
  auto body = cnetmod::json::array();
  body.get_array().reserve(tags->size());
  for (const auto &tag : *tags)
    body.get_array().push_back(cnetnexus::chart::dto::to_document(
        cnetnexus::chart::dto::project(tag)));
  cnetnexus::framework::web::respond_document(context, std::move(body));
  co_return;
}

auto handle_tag_create(cnetmod::http::request_context &context,
                       const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto *principal = cnetnexus::chart::current_principal(context);
  auto body =
      cnetmod::json::parse_lenient<cnetnexus::chart::dto::chart_tag_request>(
          co_await context.read_full_body());
  if (!body || !body->name || body->name->empty()) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto created = co_await (*scope)->service.create_tag(
      *body->name, body->color.value_or(""),
      principal != nullptr ? static_cast<std::uint64_t>(principal->id)
                           : std::uint64_t{0},
      cnetnexus::chart::audit_actor_from(context, principal));
  if (!created) {
    web::respond_error(context, created.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::project(*created)));
  co_return;
}

auto handle_tag_remove(cnetmod::http::request_context &context,
                       const case_route_context &services)
    -> cnetmod::task<void> {
  auto scope = services.resolve_cases(context);
  if (!scope) {
    web::respond_error(context, scope.error());
    co_return;
  }
  const auto *principal = cnetnexus::chart::current_principal(context);
  const auto id = web::query::path_id(context.param("id"));
  if (!id) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }
  const auto removed = co_await (*scope)->service.remove_tag(
      *id, cnetnexus::chart::audit_actor_from(context, principal));
  if (!removed) {
    web::respond_error(context, removed.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::success_response{}));
  co_return;
}

} // namespace cnetnexus::chart::detail
