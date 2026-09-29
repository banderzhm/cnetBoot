module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.batch_routes;

import cnetmod.protocol.http;
import cnetnexus.chart.service.chart_generation_service;

export namespace cnetnexus::chart {

/** @brief 注册批量排盘入口；业务编排全部由 chart_generation_service 承担。 */
void declare_batch_routes(cnetmod::http::router &routes,
                          chart_generation_service &service);

} // namespace cnetnexus::chart
