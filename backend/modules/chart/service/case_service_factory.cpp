module cnetnexus.chart.service.case_service_factory;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetnexus.chart.mapper.mysql_chart_case_mapper;
import cnetnexus.chart.service.case_service;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;

namespace cnetnexus::chart {

case_service_scope::case_service_scope(
    cnetmod::application::managed_repository<chart_case_storage_record> cases,
    cnetmod::application::managed_repository<chart_tag_storage_record> tags,
    cnetmod::orm::mapper_registry& registry,
    chart_archive_service& archives,
    cnetnexus::framework::audit_log_service& audit) noexcept
    : mapper{std::move(cases), std::move(tags), registry},
      service{mapper, archives, audit}
{
}

case_service_factory::case_service_factory(
    cnetmod::orm::mapper_registry& registry,
    chart_archive_service& archives,
    cnetnexus::framework::audit_log_service& audit) noexcept
    : registry_{&registry}, archives_{&archives}, audit_{&audit}
{
}

auto case_service_factory::make(
    cnetmod::application::managed_repository<chart_case_storage_record> cases,
    cnetmod::application::managed_repository<chart_tag_storage_record> tags) const
    -> std::unique_ptr<case_service_scope>
{
    return std::make_unique<case_service_scope>(std::move(cases),
        std::move(tags), *registry_, *archives_, *audit_);
}

} // namespace cnetnexus::chart
