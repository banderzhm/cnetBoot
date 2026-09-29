module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.da_liu_ren_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册大六壬起课路由：GET /api/v1/da_liu_ren/cast（公开已发布契约）。 */
void declare_da_liu_ren_routes(cnetmod::http::router& routes,
    cnetnexus::chart::da_liu_ren_service& service);

} // namespace cnetnexus::chart
