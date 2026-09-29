module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.bazi_routes;

import std;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart::detail {

/** @brief 解析八字出生输入（calendar/year/month/day/hour/minute/gender/method）。 */
[[nodiscard]] auto parse_bazi_birth(std::string_view query_text,
    cnetnexus::chart::bazi::birth_input& input) -> bool;

/** @brief 注册 GET /api/v1/bazi/chart 与 GET /api/v1/bazi/liu_nian。 */
void declare_bazi_handlers(cnetmod::http::router& routes,
    cnetnexus::chart::bazi_service& service);

} // namespace cnetnexus::chart::detail

export namespace cnetnexus::chart {

/** @brief 注册八字排盘路由：
 *  - GET /api/v1/bazi/chart      命盘
 *  - GET /api/v1/bazi/liu_nian   目标公历年流年盘面
 *
 *  两条都是公开已发布契约（与原实现一致，不带权限码）。
 */
void declare_bazi_routes(cnetmod::http::router& routes,
    cnetnexus::chart::bazi_service& service);

} // namespace cnetnexus::chart
