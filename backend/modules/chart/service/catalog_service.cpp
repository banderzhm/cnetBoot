module cnetnexus.chart.service.catalog_service;

import std;
import cnetnexus.chart.domain.catalog;

namespace cnetnexus::chart {

auto capabilities() -> capability_summary
{
    capability_summary result;
    result.algorithms.reserve(cnetnexus::chart::capability_catalog().size());
    for (const auto& capability : cnetnexus::chart::capability_catalog())
        result.algorithms.emplace_back(capability.key);
    return result;
}

} // namespace cnetnexus::chart
