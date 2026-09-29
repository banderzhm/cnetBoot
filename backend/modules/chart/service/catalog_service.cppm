export module cnetnexus.chart.service.catalog_service;

import std;

export namespace cnetnexus::chart {

struct capability_summary
{
    std::vector<std::string> algorithms;
};

/** @brief Returns the algorithm keys advertised by the domain catalog. */
[[nodiscard]] auto capabilities() -> capability_summary;

} // namespace cnetnexus::chart
