module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.service.case_service;

import std;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.json;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.naming;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;
import cnetnexus.framework.audit;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.mapper.chart_case_mapper;

namespace cnetnexus::chart::detail {

struct report_projection
{
    std::optional<std::string> analysisZh;
    std::optional<std::string> reportZh;
};

struct archive_projection
{
    std::optional<cnetmod::json::raw_value> snapshot;
};

} // namespace cnetnexus::chart::detail

namespace {

namespace chart = cnetnexus::chart;

/// @brief 从盘面 JSON 里取中文报告字段：八字等用 analysisZh，紫微用 reportZh。
auto report_from_json(const std::string& text) -> std::optional<std::string>
{
    const auto parsed = cnetmod::json::parse_lenient<cnetnexus::chart::detail::report_projection>(text);
    if (!parsed)
        return std::nullopt;
    if (parsed->analysisZh && !parsed->analysisZh->empty())
        return parsed->analysisZh;
    if (parsed->reportZh && !parsed->reportZh->empty())
        return parsed->reportZh;
    return std::nullopt;
}

} // namespace

namespace cnetnexus::chart {

auto case_service::list(const cnetnexus::chart::case_query& query)
    -> cnetmod::task<std::expected<cnetnexus::framework::pagination::page_result<cnetnexus::chart::chart_case>,
        cnetnexus::framework::error::error_code>>
{
    co_return co_await cases_->list(query);
}

auto case_service::find(std::uint64_t id)
    -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>>
{
    const auto found = co_await cases_->find_by_id(id);
    if (!found)
        co_return std::unexpected{found.error()};
    if (!found->has_value())
        co_return std::unexpected{cnetnexus::framework::error::error_code::case_not_found};
    co_return **found;
}

auto case_service::create(const case_write_request& request, const cnetnexus::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>>
{
    if (request.value.title.empty())
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    auto value = request.value;
    value.id = 0;
    value.owner_id = actor.user_id;
    value.algorithm = cnetnexus::chart::canonical_algorithm(value.algorithm);
    if (!cnetnexus::chart::is_known_algorithm(value.algorithm))
        co_return std::unexpected{cnetnexus::framework::error::error_code::algorithm_not_supported};

    value.tag_ids = request.tag_ids;
    const auto created = co_await cases_->create(value);
    if (!created)
        co_return std::unexpected{created.error()};

    (void)co_await audit_->record(actor, cnetnexus::framework::audit_action_create, "case", 200,
        std::to_string(created->id));
    co_return co_await find(created->id);
}

auto case_service::update(const case_write_request& request, const cnetnexus::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetnexus::chart::chart_case, cnetnexus::framework::error::error_code>>
{
    if (request.value.id == 0 || request.value.title.empty())
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    const auto current = co_await cases_->find_by_id(request.value.id);
    if (!current)
        co_return std::unexpected{current.error()};
    if (!current->has_value())
        co_return std::unexpected{cnetnexus::framework::error::error_code::case_not_found};

    auto merged = current->value();
    const auto& incoming = request.value;
    merged.title = incoming.title;
    // algorithm 建库后不允许改（前端编辑态也禁用）：提供了就必须与现值同口径，
    // 未提供则保持原值，避免默认值把 liu_yao 之类的案例误改成 ba_zi。
    if (request.has_algorithm && cnetnexus::chart::canonical_algorithm(incoming.algorithm) != merged.algorithm)
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    merged.archive_id = incoming.archive_id;
    merged.calendar = incoming.calendar;
    merged.gender = incoming.gender;
    merged.birth_year = incoming.birth_year;
    merged.birth_month = incoming.birth_month;
    merged.birth_day = incoming.birth_day;
    merged.birth_hour = incoming.birth_hour;
    merged.birth_minute = incoming.birth_minute;
    merged.birth_place = incoming.birth_place;
    merged.client_name = incoming.client_name;
    merged.question = incoming.question;
    merged.notes = incoming.notes;
    if (request.has_payload)
        merged.payload = incoming.payload;
    if (request.has_reading)
        merged.reading = incoming.reading;

    if (request.has_tag_ids)
        merged.tag_ids = request.tag_ids;
    const auto updated = co_await cases_->update(merged, request.has_tag_ids);
    if (!updated)
        co_return std::unexpected{updated.error()};

    (void)co_await audit_->record(actor, cnetnexus::framework::audit_action_update, "case", 200,
        std::to_string(merged.id));
    co_return co_await find(merged.id);
}

auto case_service::remove(std::uint64_t id, const cnetnexus::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    const auto removed = co_await cases_->remove(id);
    if (!removed)
        co_return std::unexpected{removed.error()};
    (void)co_await audit_->record(actor, cnetnexus::framework::audit_action_delete, "case", 200,
        std::to_string(id));
    co_return std::expected<void, cnetnexus::framework::error::error_code>{};
}

auto case_service::save_reading(std::uint64_t id, std::string reading)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    const auto current = co_await cases_->find_by_id(id);
    if (!current)
        co_return std::unexpected{current.error()};
    if (!current->has_value())
        co_return std::unexpected{cnetnexus::framework::error::error_code::case_not_found};
    auto value = current->value();
    value.reading = std::move(reading);
    const auto updated = co_await cases_->update(value, false);
    if (!updated)
        co_return std::unexpected{updated.error()};
    co_return std::expected<void, cnetnexus::framework::error::error_code>{};
}

auto case_service::report_text(const cnetnexus::chart::chart_case& value)
    -> cnetmod::task<std::expected<std::optional<std::string>, cnetnexus::framework::error::error_code>>
{
    // 1) 案例自带的排盘快照优先。
    if (value.payload.has_value() && !value.payload->empty())
    {
        if (auto report = report_from_json(*value.payload); report.has_value())
            co_return std::optional<std::string>{std::move(*report)};
        co_return std::optional<std::string>{*value.payload};
    }

    // 2) 只关联了存档时，读回落库的排盘文档（外层 {algorithm,savedAt,title,snapshot}）。
    if (value.archive_id.has_value() && !value.archive_id->empty())
    {
        // 存档读取是异步的：先把要用的字段复制进协程帧，避免挂起期间依赖调用方对象。
        const auto algorithm = value.algorithm;
        const auto archive_id = *value.archive_id;
        const auto document = co_await archives_->fetch(algorithm, archive_id);
        if (!document)
            co_return std::unexpected{document.error()};
        if (!document->has_value())
            co_return std::unexpected{cnetnexus::framework::error::error_code::archive_not_found};

        const auto archive = cnetmod::json::parse_lenient<detail::archive_projection>(**document);
        // 外层文档形如 {algorithm,savedAt,title,snapshot}：能取到 snapshot 字符串就用内层，
        // 否则按整篇文档当快照解析。Glaze 的 native document 用 get<std::string>() 取字符串值。
        std::string source = **document;
        if (archive && archive->snapshot && archive->snapshot->is_string())
            source = archive->snapshot->get<std::string>();
        if (auto report = report_from_json(source); report)
            co_return std::optional<std::string>{std::move(*report)};
        co_return std::optional<std::string>{source};
    }
    co_return std::optional<std::string>{};
}

auto case_service::list_tags()
    -> cnetmod::task<std::expected<std::vector<cnetnexus::chart::chart_tag>, cnetnexus::framework::error::error_code>>
{
    co_return co_await cases_->list_tags();
}

auto case_service::create_tag(std::string name, std::string color, std::uint64_t owner_id, const cnetnexus::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetnexus::chart::chart_tag, cnetnexus::framework::error::error_code>>
{
    if (name.empty() || name.size() > 32)
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    if (color.empty())
        color = "#1677ff";

    cnetnexus::chart::chart_tag value;
    value.name = std::move(name);
    value.color = std::move(color);
    value.owner_id = owner_id;

    const auto created = co_await cases_->create_tag(value);
    if (!created)
        co_return std::unexpected{created.error()};
    (void)co_await audit_->record(actor, cnetnexus::framework::audit_action_create, "tag", 200,
        std::to_string(created->id));
    co_return *created;
}

auto case_service::remove_tag(std::uint64_t id, const cnetnexus::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<void, cnetnexus::framework::error::error_code>>
{
    const auto removed = co_await cases_->remove_tag(id);
    if (!removed)
        co_return std::unexpected{removed.error()};
    (void)co_await audit_->record(actor, cnetnexus::framework::audit_action_delete, "tag", 200,
        std::to_string(id));
    co_return std::expected<void, cnetnexus::framework::error::error_code>{};
}

} // namespace cnetnexus::chart
