export module cnetnexus.chart.service.case_service_factory;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetnexus.chart.mapper.mysql_chart_case_mapper;
import cnetnexus.chart.service.case_service;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

struct case_service_scope
{
    case_service_scope(
        cnetmod::application::managed_repository<chart_case_storage_record> cases,
        cnetmod::application::managed_repository<chart_tag_storage_record> tags,
        cnetmod::orm::mapper_registry& registry,
        chart_archive_service& archives,
        cnetnexus::framework::audit_log_service& audit) noexcept;

    mysql_chart_case_mapper mapper;
    case_service service;
};

/** Provides request-scoped case services without exposing Mapper to controllers. */
class case_service_factory
{
public:
    case_service_factory(
        cnetmod::orm::mapper_registry& registry,
        chart_archive_service& archives,
        cnetnexus::framework::audit_log_service& audit) noexcept;

    [[nodiscard]] auto make(
        cnetmod::application::managed_repository<chart_case_storage_record> cases,
        cnetmod::application::managed_repository<chart_tag_storage_record> tags) const
        -> std::unique_ptr<case_service_scope>;

private:
    cnetmod::orm::mapper_registry* registry_;
    chart_archive_service* archives_;
    cnetnexus::framework::audit_log_service* audit_;
};

} // namespace cnetnexus::chart
