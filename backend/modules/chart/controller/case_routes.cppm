module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.case_routes;

import std;
import cnetmod.protocol.http;
import cnetmod.coro.task;
import cnetmod.json;
import cnetnexus.chart.service.case_service;
import cnetnexus.chart.service.case_service_factory;
import cnetnexus.framework.error;
import cnetnexus.reading.service.reading_use_case;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

using case_service_resolver = std::function<std::expected<
    std::unique_ptr<case_service_scope>, cnetnexus::framework::error::error_code>(
    const cnetmod::http::request_context&)>;

/** @brief 注册案例库与标签路由：
 *  - GET/POST       /api/v1/cases            权限 chart:case:list / :create
 *  - GET/PUT/DELETE /api/v1/cases/:id        权限 chart:case:read / :update /
 * :delete
 *  - POST           /api/v1/cases/:id/reading 权限 chart:reading:run，reading
 * 限流
 *  - GET/POST       /api/v1/tags             权限 chart:tag:manage
 *  - DELETE         /api/v1/tags/:id         权限 chart:tag:manage
 *
 *  权限声明写在路由注册处，不下沉到 service。/cases/:id/reading 与六条 reading
 *  共用断语响应器：stream=true 走 SSE，缺省保持一次性 JSON；
 *  两条路径都先查断语缓存，命中不调模型。
 */
void declare_case_routes(cnetmod::http::router &routes,
                         case_service_resolver resolve_cases,
                         cnetnexus::reading::reading_use_case &reading);

} // namespace cnetnexus::chart
export namespace cnetnexus::chart::detail {

/** @brief 案例与标签路由共用的依赖集合。 */
struct case_route_context {
  cnetnexus::chart::case_service_resolver resolve_cases;
  cnetnexus::reading::reading_use_case *reading{};
};

/** @brief GET /api/v1/cases 的处理器声明（显式类型，注册处不出现 lambda）。 */
[[nodiscard]] auto handle_case_list(cnetmod::http::request_context &context,
                                    const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_case_create(cnetmod::http::request_context &context,
                                      const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_case_find(cnetmod::http::request_context &context,
                                    const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_case_update(cnetmod::http::request_context &context,
                                      const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_case_remove(cnetmod::http::request_context &context,
                                      const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_case_reading(cnetmod::http::request_context &context,
                                       const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_tag_list(cnetmod::http::request_context &context,
                                   const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_tag_create(cnetmod::http::request_context &context,
                                     const case_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto handle_tag_remove(cnetmod::http::request_context &context,
                                     const case_route_context &services)
    -> cnetmod::task<void>;

/** @brief 逐路由注册（权限装饰在本处完成）。 */
void declare_case_handlers(cnetmod::http::router &routes,
                           const case_route_context &services);

} // namespace cnetnexus::chart::detail
