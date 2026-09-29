module cnetnexus.chart.service.xuanxue.json_bridge;

import std;
import cnetmod.json;

namespace cnetnexus::chart {

auto parse_provider_object(std::string_view encoded)
    -> std::expected<cnetmod::json::document, std::error_code>
{
    auto parsed = cnetmod::json::parse_document(encoded);
    if (!parsed)
        return std::unexpected(parsed.error());
    if (!parsed->is_object())
        return std::unexpected(
            cnetmod::json::make_error_code(cnetmod::json::errc::type_mismatch));
    return parsed;
}

} // namespace cnetnexus::chart
