module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.liu_yao_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册六爻装卦断卦路由：GET /api/v1/liu_yao/cast（公开已发布契约）。 */
void declare_liu_yao_routes(cnetmod::http::router& routes,
    cnetnexus::chart::liu_yao_service& service);

} // namespace cnetnexus::chart
