module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.route_support;

import std;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.domain.cases;
import cnetnexus.framework.audit;
import cnetnexus.framework.security.current_user;
import cnetnexus.framework.pagination;
import cnetnexus.framework.web.query;

namespace cnetnexus::chart {

auto page_from(std::string_view query_text)
    -> cnetnexus::framework::pagination::page_request
{
    cnetnexus::framework::pagination::page_request page;
    page.page = cnetnexus::framework::web::query::integer(query_text, "page")
                    .value_or(1);
    page.page_size =
        cnetnexus::framework::web::query::integer(query_text, "page_size")
            .value_or(cnetnexus::framework::pagination::default_page_size);
    return cnetnexus::framework::pagination::normalize(page);
}

auto optional_int_param(std::string_view query_text, std::string_view key,
    int minimum, int maximum, std::optional<int>& output) -> bool
{
    if (!cnetnexus::framework::web::query::value(query_text, key))
        return true;
    const auto parsed = cnetnexus::framework::web::query::integer(query_text, key);
    if (!parsed || *parsed < minimum || *parsed > maximum)
        return false;
    output = *parsed;
    return true;
}

auto audit_actor_from(const cnetmod::http::request_context& context,
    const cnetnexus::security::current_user* principal)
    -> cnetnexus::framework::audit_actor
{
    cnetnexus::framework::audit_actor actor;
    actor.ip = context.client_address();
    actor.user_agent = std::string{context.get_header("User-Agent")};
    actor.request_id = std::string{context.trace_id()};
    actor.method = std::string{context.method()};
    actor.path = std::string{context.path()};
    if (principal != nullptr)
    {
        actor.user_id = static_cast<std::uint64_t>(principal->id);
        actor.username = principal->username;
    }
    return actor;
}

auto current_principal(cnetmod::http::request_context& context) noexcept
    -> const cnetnexus::security::current_user*
{
    return context.scope().find<cnetnexus::security::current_user>();
}

auto nullable_text(const cnetmod::json::raw_value& raw)
    -> std::optional<std::optional<std::string>>
{
    if (raw.is_null())
        return std::optional<std::string>{};
    auto value = cnetmod::json::from_document<std::string>(raw);
    if (!value)
        return std::nullopt;
    return cnetnexus::chart::optional_text(std::move(*value));
}

auto id_list(const cnetmod::json::raw_value& raw)
    -> std::optional<std::vector<std::uint64_t>>
{
    if (raw.is_null())
        return std::vector<std::uint64_t>{};
    auto values = cnetmod::json::from_document<std::vector<std::uint64_t>>(raw);
    if (!values)
        return std::nullopt;
    return std::move(*values);
}

} // namespace cnetnexus::chart
