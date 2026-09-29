module;

#include <cnetmod/json_codec.hpp>

/**
 * @brief Provider-boundary JSON projections owned by the chart DTO layer.
 *
 * 上游 provider JSON（camelCase）原样并入响应根，键名与嵌套结构一个不改；
 * 本模块只负责把 domain 结构投影成 draft 文档。
 */
export module cnetnexus.chart.dto.chart_projection;

import std;
import cnetmod.json;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;

export namespace cnetnexus::chart::projection {

/** @brief 八字命盘：既有顶层键 + provider 扩展键 + 完整分析 JSON。 */
[[nodiscard]] auto chart_response(const cnetnexus::chart::bazi::chart& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

/** @brief 六爻盘面。 */
[[nodiscard]] auto chart_response(
    const cnetnexus::chart::liu_yao::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

/** @brief 梅花盘面。 */
[[nodiscard]] auto chart_response(
    const cnetnexus::chart::mei_hua::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

/** @brief 奇门盘面。 */
[[nodiscard]] auto chart_response(
    const cnetnexus::chart::qi_men::chart_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

/** @brief 紫微盘面（含可选运限文档）。 */
[[nodiscard]] auto chart_response(
    const cnetnexus::chart::zi_wei::chart_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

/** @brief 大六壬课盘。 */
[[nodiscard]] auto chart_response(
    const cnetnexus::chart::da_liu_ren::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>;

} // namespace cnetnexus::chart::projection
