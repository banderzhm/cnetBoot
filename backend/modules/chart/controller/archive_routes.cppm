module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.archive_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.service.chart_generation_service;
import cnetnexus.framework.web.route_handler;

export namespace cnetnexus::chart {

/** @brief 注册排盘存档路由：
 *  - POST   /api/v1/bazi/archive           生成并保存八字命盘快照（需登录，archive_write 限流）
 *  - GET    /api/v1/archive/search?...     存档检索（chart:archive:list）
 *  - GET    /api/v1/archive/:algorithm/:id 按 ID 取回快照（公开）
 *  - DELETE /api/v1/archive/:algorithm/:id 删除存档（chart:archive:delete）
 *
 *  @note 前两条中的取回与写入是既有已发布契约（不带鉴权，供站点与验收脚本直接调用）；
 *  检索与删除必须带权限码。权限声明写在路由注册处，不下沉到 service。
 */
void declare_archive_routes(cnetmod::http::router& routes,
    cnetnexus::chart::chart_generation_service& generation,
    cnetnexus::chart::chart_archive_service& archive);

} // namespace cnetnexus::chart
