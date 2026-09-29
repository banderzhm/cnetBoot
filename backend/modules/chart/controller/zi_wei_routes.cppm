module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.zi_wei_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册紫微斗数排盘路由：GET /api/v1/zi_wei/chart（公开已发布契约）。 */
void declare_zi_wei_routes(cnetmod::http::router& routes,
    cnetnexus::chart::zi_wei_service& service);

} // namespace cnetnexus::chart
