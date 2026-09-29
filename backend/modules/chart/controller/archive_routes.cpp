module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.archive_routes;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.bazi_routes;
import cnetnexus.chart.controller.archive_access;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.service.chart_generation_service;
import cnetnexus.framework.error;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.query;
import cnetnexus.framework.web.rate_limit;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;

namespace {

struct archive_services {
  cnetnexus::chart::chart_generation_service *generation;
  cnetnexus::chart::chart_archive_service *archive;
};

/// @brief POST /api/v1/bazi/archive：排盘后把命盘快照写入仓储并返回快照 ID。
auto handle_bazi_archive(cnetmod::http::request_context &context,
                         cnetnexus::chart::chart_generation_service &generation)
    -> cnetmod::task<void> {
  cnetnexus::chart::bazi::birth_input input{};
  if (!cnetnexus::chart::detail::parse_bazi_birth(context.query_string(),
                                                  input)) {
    web::respond_error(context, error::error_code::request_invalid_argument);
    co_return;
  }

  const auto access = cnetnexus::chart::archive_access_from(context);
  const auto result = co_await generation.archive_bazi(input, access.owner_id);
  if (!result) {
    web::respond_error(context, result.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(*result));
  co_return;
}

/// @brief GET /api/v1/archive/:algorithm/:id：按 ID 取回快照正文。
auto handle_archive_lookup(cnetmod::http::request_context &context,
                           cnetnexus::chart::chart_archive_service &archive)
    -> cnetmod::task<void> {
  // 已发布公开契约：注册处声明 optional_identity()，免令牌可访问。
  // 可选鉴权 + 归属校验：带令牌时可读本人和匿名分区，匿名只可见匿名分区，
  // 因此不再暴露其他用户的记录（P0-1）。
  const auto access = cnetnexus::chart::archive_access_from(context);
  const auto snapshot = co_await archive.fetch(context.param("algorithm"),
                                               context.param("id"), access);
  if (!snapshot) {
    web::respond_error(context, snapshot.error());
    co_return;
  }
  if (!snapshot->has_value()) {
    web::respond_error(context, error::error_code::archive_not_found);
    co_return;
  }
  // 快照是此前写出的 JSON 文本：原样回写，不重新序列化。
  context.json(cnetmod::http::status::ok, **snapshot);
  co_return;
}

/// @brief GET /api/v1/archive/search：按术数 / 标题关键字 / 时间范围检索存档。
auto handle_archive_search(cnetmod::http::request_context &context,
                           cnetnexus::chart::chart_archive_service &archive)
    -> cnetmod::task<void> {
  const auto raw = context.query_string();
  cnetnexus::chart::archive_query filter;
  if (const auto algorithm = web::query::value(raw, "algorithm");
      algorithm && !algorithm->empty())
    filter.algorithm = cnetnexus::chart::canonical_algorithm(*algorithm);
  filter.keyword = web::query::value(raw, "keyword").value_or("");
  if (const auto from = web::query::value(raw, "from");
      from && !from->empty()) {
    const auto parsed = cnetnexus::chart::parse_iso8601(*from);
    if (!parsed) {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
    filter.from = *parsed;
  }
  if (const auto to = web::query::value(raw, "to"); to && !to->empty()) {
    const auto parsed = cnetnexus::chart::parse_iso8601(*to);
    if (!parsed) {
      web::respond_error(context, error::error_code::request_invalid_argument);
      co_return;
    }
    filter.to = *parsed;
  }
  filter.page = cnetnexus::chart::page_from(raw);

  // 检索按存档归属过滤，普通用户只看本人及公共存档。
  const auto access = cnetnexus::chart::archive_access_from(context);
  const auto result = co_await archive.search(filter, access);
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
          page, [](const cnetnexus::chart::dto::archive_index_response &value) {
            return cnetnexus::chart::dto::to_document(value);
          }));
  co_return;
}

/// @brief DELETE /api/v1/archive/:algorithm/:id：删除存档。
auto handle_archive_remove(cnetmod::http::request_context &context,
                           cnetnexus::chart::chart_archive_service &archive)
    -> cnetmod::task<void> {
  // P0-1：守卫只保证「有 chart:archive:delete 权限」，归属由 service 判定：
  // 非全量范围的调用方删他人（或公共匿名分区）记录返回 AUTH.FORBIDDEN。
  const auto access = cnetnexus::chart::archive_access_from(context);
  const auto removed = co_await archive.remove(context.param("algorithm"),
                                               context.param("id"), access);
  if (!removed) {
    web::respond_error(context, removed.error());
    co_return;
  }
  cnetnexus::framework::web::respond_document(
      context, cnetnexus::chart::dto::to_document(
                   cnetnexus::chart::dto::success_response{}));
  co_return;
}

auto handle_bazi_archive_route(cnetmod::http::request_context &context,
                               const archive_services &services)
    -> cnetmod::task<void> {
  return handle_bazi_archive(context, *services.generation);
}

} // namespace

void declare_archive_routes(cnetmod::http::router &routes,
                            cnetnexus::chart::chart_generation_service &generation,
                            cnetnexus::chart::chart_archive_service &archive) {
  using archive_handler = web::service_route_handler<chart_archive_service>;
  // 写存档需要登录（落盘 / 落库有成本且需要归属），并受 archive_write 限流。
  routes.post("/api/v1/bazi/archive",
              web::route_handler<archive_services>{{&generation, &archive},
                                                   handle_bazi_archive_route},
              cnetnexus::framework::web::with_rate_limit(
                  cnetmod::http::endpoint_metadata{},
                  cnetnexus::framework::web::rate_limit_policy::archive_write));
  // 路由器按具体度打分匹配，字面量 /search 优先于 :algorithm/:id；
  // 访问策略按方法声明：GET 公开且可选鉴权，DELETE 需要权限。
  routes.get("/api/v1/archive/search",
             archive_handler{archive, handle_archive_search},
             cnetnexus::security::requires_permission("chart:archive:list"));
  routes.get("/api/v1/archive/:algorithm/:id",
             archive_handler{archive, handle_archive_lookup},
             cnetnexus::framework::web::optional_identity());
  routes.del("/api/v1/archive/:algorithm/:id",
             archive_handler{archive, handle_archive_remove},
             cnetnexus::security::requires_permission("chart:archive:delete"));
}

} // namespace cnetnexus::chart
