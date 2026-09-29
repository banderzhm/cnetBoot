module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.mei_hua_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册梅花易数时间起卦路由：GET /api/v1/mei_hua/cast（公开已发布契约）。 */
void declare_mei_hua_routes(cnetmod::http::router& routes,
    cnetnexus::chart::mei_hua_service& service);

} // namespace cnetnexus::chart
