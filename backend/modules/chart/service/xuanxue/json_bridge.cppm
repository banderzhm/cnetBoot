/**
 * @brief ZhouYiLab JSON compatibility boundary.
 *
 * The upstream library still emits nlohmann JSON text. It is accepted only at
 * this adapter boundary and immediately converted to cnetmod's Glaze document;
 * domain and controller layers never exchange JSON strings.
 */
export module cnetnexus.chart.service.xuanxue.json_bridge;

import std;
import cnetmod.json;

export namespace cnetnexus::chart {

[[nodiscard]] auto parse_provider_object(std::string_view encoded)
    -> std::expected<cnetmod::json::document, std::error_code>;

} // namespace cnetnexus::chart
