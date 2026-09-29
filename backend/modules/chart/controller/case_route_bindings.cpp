module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.case_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.route_support;
import cnetnexus.chart.service.case_service;
import cnetnexus.framework.error;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.web.rate_limit;
import cnetnexus.framework.web.route_handler;
import cnetnexus.reading.service.reading_use_case;

namespace cnetnexus::chart::detail {

void declare_case_handlers(cnetmod::http::router &routes,
                           const case_route_context &services) {
  using handler = cnetnexus::framework::web::route_handler<case_route_context>;

  routes.get("/api/v1/cases", handler{services, handle_case_list},
             cnetnexus::security::requires_permission("chart:case:list"));
  routes.post("/api/v1/cases", handler{services, handle_case_create},
              cnetnexus::security::requires_permission("chart:case:create"));
  routes.get("/api/v1/cases/:id", handler{services, handle_case_find},
             cnetnexus::security::requires_permission("chart:case:read"));
  routes.put("/api/v1/cases/:id", handler{services, handle_case_update},
             cnetnexus::security::requires_permission("chart:case:update"));
  routes.del("/api/v1/cases/:id", handler{services, handle_case_remove},
             cnetnexus::security::requires_permission("chart:case:delete"));
  routes.post("/api/v1/cases/:id/reading",
              handler{services, handle_case_reading},
              cnetnexus::framework::web::with_rate_limit(
                  cnetnexus::security::requires_permission("chart:reading:run"),
                  cnetnexus::framework::web::rate_limit_policy::reading));
  routes.get("/api/v1/tags", handler{services, handle_tag_list},
             cnetnexus::security::requires_permission("chart:tag:manage"));
  routes.post("/api/v1/tags", handler{services, handle_tag_create},
              cnetnexus::security::requires_permission("chart:tag:manage"));
  routes.del("/api/v1/tags/:id", handler{services, handle_tag_remove},
             cnetnexus::security::requires_permission("chart:tag:manage"));
}

} // namespace cnetnexus::chart::detail

namespace cnetnexus::chart {

void declare_case_routes(cnetmod::http::router &routes,
                         case_service_resolver resolve_cases,
                         cnetnexus::reading::reading_use_case &reading) {
  if (!resolve_cases)
    throw std::invalid_argument{"case routes require a service resolver"};
  const detail::case_route_context services{std::move(resolve_cases), &reading};
  detail::declare_case_handlers(routes, services);
}

} // namespace cnetnexus::chart
