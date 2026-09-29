module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.reading_scope;

import std;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

struct reading_scope_error {
    cnetnexus::framework::error::error_code code{
        cnetnexus::framework::error::error_code::request_invalid_argument};
    std::string message;
};

[[nodiscard]] auto parse_reading_scope_query(std::string_view algorithm,
    std::string_view query)
    -> std::expected<reading_scope_request, reading_scope_error>;

} // namespace cnetnexus::chart
