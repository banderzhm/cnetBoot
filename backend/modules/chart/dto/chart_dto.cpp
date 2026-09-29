module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.dto.chart;

// DTO projection and serialization stay outside the HTTP controller layer.

import std;
import cnetmod.json;
import cnetnexus.chart.domain.naming;

namespace cnetnexus::chart::dto {

namespace {

    /// @brief 可空文本：有值写字符串，无值写显式 null。
    [[nodiscard]] auto nullable(const std::optional<std::string>& value)
        -> cnetmod::json::document
    {
        cnetmod::json::document result;
        if (value)
            result = *value;
        else
            result = nullptr;
        return result;
    }

    /// @brief 可空 JSON 值：有值原样，无值是显式 null。
    template <typename T>
    [[nodiscard]] auto nullable_value(const std::optional<T>& value)
        -> cnetmod::json::document
    {
        cnetmod::json::document result;
        if (value)
            result = *value;
        else
            result = nullptr;
        return result;
    }

    [[nodiscard]] auto to_document(const algorithm_count_response& value)
        -> cnetmod::json::document
    {
        return cnetmod::json::object(
            {{"algorithm", value.algorithm}, {"count", value.count}});
    }

} // namespace

auto project(const cnetnexus::chart::archive_index_entry& value)
    -> archive_index_response
{
    return {.archive_id = value.archive_id,
        .algorithm = value.algorithm,
        .title = value.title,
        .owner_id = value.owner_id,
        .size_bytes = value.size_bytes,
        .created_at = cnetnexus::chart::iso8601_utc(value.created_at)};
}

auto project(const cnetnexus::chart::chart_tag& value) -> chart_tag_response
{
    return {.id = value.id,
        .name = value.name,
        .color = value.color,
        .owner_id = value.owner_id,
        .created_at = cnetnexus::chart::iso8601_utc(value.created_at)};
}

auto project(const cnetnexus::chart::chart_case& value) -> chart_case_response
{
    chart_case_response result{.id = value.id,
        .owner_id = value.owner_id,
        .title = value.title,
        .algorithm = value.algorithm,
        .archive_id = value.archive_id,
        .calendar = value.calendar,
        .gender = value.gender,
        .birth_year = value.birth_year,
        .birth_month = value.birth_month,
        .birth_day = value.birth_day,
        .birth_hour = value.birth_hour,
        .birth_minute = value.birth_minute,
        .birth_place = value.birth_place,
        .client_name = value.client_name,
        .question = value.question,
        .notes = value.notes,
        .reading = value.reading,
        .payload = value.payload,
        .created_at = cnetnexus::chart::iso8601_utc(value.created_at),
        .updated_at = cnetnexus::chart::iso8601_utc(value.updated_at),
        .tag_ids = value.tag_ids};
    result.tags.reserve(value.tags.size());
    for (const auto& tag : value.tags)
        result.tags.push_back(project(tag));
    return result;
}

auto project(const cnetnexus::chart::stats_overview& value)
    -> stats_overview_response
{
    stats_overview_response result;
    result.users = value.users;
    result.cases = value.cases;
    result.today = {.logins = value.today_logins, .charts = value.today_charts};
    for (const auto& item : value.archives)
    {
        if (item.algorithm == "ba_zi")
            result.archives.ba_zi = item.count;
        else if (item.algorithm == "liu_yao")
            result.archives.liu_yao = item.count;
        else if (item.algorithm == "mei_hua")
            result.archives.mei_hua = item.count;
        else if (item.algorithm == "zi_wei")
            result.archives.zi_wei = item.count;
        else if (item.algorithm == "da_liu_ren")
            result.archives.da_liu_ren = item.count;
        else if (item.algorithm == "qi_men")
            result.archives.qi_men = item.count;
    }
    result.top_algorithms.reserve(value.top_algorithms.size());
    for (const auto& item : value.top_algorithms)
        result.top_algorithms.push_back(
            {.algorithm = item.algorithm, .count = item.count});
    return result;
}

auto to_document(const archive_index_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"archiveId", value.archive_id},
        {"algorithm", value.algorithm}, {"title", value.title},
        {"ownerId", value.owner_id}, {"sizeBytes", value.size_bytes},
        {"createdAt", value.created_at}});
}

auto to_document(const chart_tag_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"id", value.id}, {"name", value.name},
        {"color", value.color}, {"ownerId", value.owner_id},
        {"createdAt", value.created_at}});
}

auto to_document(const chart_case_response& value) -> cnetmod::json::document
{
    auto tags = cnetmod::json::array();
    tags.get_array().reserve(value.tags.size());
    for (const auto& tag : value.tags)
        tags.get_array().push_back(to_document(tag));

    auto tag_ids = cnetmod::json::array();
    tag_ids.get_array().reserve(value.tag_ids.size());
    for (const auto id : value.tag_ids)
        tag_ids.get_array().push_back(id);

    return cnetmod::json::object({{"id", value.id},
        {"ownerId", value.owner_id}, {"title", value.title},
        {"algorithm", value.algorithm},
        {"archiveId", nullable(value.archive_id)},
        {"calendar", value.calendar}, {"gender", value.gender},
        {"birthYear", value.birth_year}, {"birthMonth", value.birth_month},
        {"birthDay", value.birth_day}, {"birthHour", value.birth_hour},
        {"birthMinute", value.birth_minute},
        {"birthPlace", value.birth_place},
        {"clientName", value.client_name}, {"question", value.question},
        {"notes", nullable(value.notes)}, {"reading", nullable(value.reading)},
        {"payload", nullable(value.payload)}, {"createdAt", value.created_at},
        {"updatedAt", value.updated_at}, {"tagIds", std::move(tag_ids)},
        {"tags", std::move(tags)}});
}

auto to_document(const stats_overview_response& value) -> cnetmod::json::document
{
    auto archives = cnetmod::json::object({{"ba_zi", value.archives.ba_zi},
        {"liu_yao", value.archives.liu_yao}, {"mei_hua", value.archives.mei_hua},
        {"zi_wei", value.archives.zi_wei},
        {"da_liu_ren", value.archives.da_liu_ren},
        {"qi_men", value.archives.qi_men}});
    auto today = cnetmod::json::object(
        {{"logins", value.today.logins}, {"charts", value.today.charts}});
    auto top = cnetmod::json::array();
    top.get_array().reserve(value.top_algorithms.size());
    for (const auto& item : value.top_algorithms)
        top.get_array().push_back(to_document(item));
    return cnetmod::json::object({{"users", value.users},
        {"cases", value.cases}, {"archives", std::move(archives)},
        {"today", std::move(today)}, {"topAlgorithms", std::move(top)}});
}

auto to_document(const archive_created_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object(
        {{"id", value.id}, {"algorithm", value.algorithm}});
}

auto to_document(const success_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"success", value.success}});
}

auto to_document(const chart_batch_response& value) -> cnetmod::json::document
{
    auto items = cnetmod::json::array();
    items.get_array().reserve(value.items.size());
    for (const auto& item : value.items)
    {
        cnetmod::json::document body;
        if (item.error)
            body = cnetmod::json::object({{"code", item.error->code},
                {"status", item.error->status},
                {"message", item.error->message},
                {"retryable", item.error->retryable}});
        else
            body = nullptr;
        items.get_array().push_back(cnetmod::json::object({{"index", item.index},
            {"title", item.title}, {"ok", item.ok},
            {"algorithm", item.algorithm},
            {"archiveId", nullable_value(item.archive_id)},
            {"chart", nullable_value(item.chart)}, {"error", std::move(body)}}));
    }
    return cnetmod::json::object({{"items", std::move(items)},
        {"succeeded", value.succeeded}, {"failed", value.failed}});
}

} // namespace cnetnexus::chart::dto
