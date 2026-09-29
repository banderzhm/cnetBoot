module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.stats_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.stats_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册概览统计路由：GET /api/v1/stats/overview（已登录即可，无独立权限码）。
 *
 *  @param today_logins 今日登录数来源；组合根未装配时传 nullptr，该计数按 0 输出。
 */
void declare_stats_routes(cnetmod::http::router& routes,
    cnetnexus::chart::stats_service& stats);

} // namespace cnetnexus::chart
