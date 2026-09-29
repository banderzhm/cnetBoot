module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.qi_men_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册奇门遁甲排盘路由：GET /api/v1/qi_men/chart（公开已发布契约）。 */
void declare_qi_men_routes(cnetmod::http::router& routes,
    cnetnexus::chart::qi_men_service& service);

} // namespace cnetnexus::chart
