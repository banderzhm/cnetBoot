module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.catalog_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册能力清单与错误码检索路由（两条都不鉴权）。
 *
 *  健康检查不在这里：由 Application 管理端点 /actuator/live、/actuator/ready、
 *  /actuator/health 提供，数据来自 health_registry 缓存。
 *  - GET /api/v1/capabilities
 *  - GET /api/v1/error_codes
 */
void declare_catalog_routes(cnetmod::http::router& routes);

} // namespace cnetnexus::chart
