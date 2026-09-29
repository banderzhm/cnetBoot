module cnetnexus.chart.service.chart_archive_service;

import std;
import cnetmod.core.time;
import cnetmod.coro;
import cnetmod.security.jwt;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.naming;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;
import cnetnexus.chart.mapper.chart_archive_repository;

namespace cnetnexus::chart {

auto chart_archive_service::archive(std::string_view algorithm,
    std::string snapshot, std::string title, std::uint64_t owner_id)
    -> cnetmod::task<std::expected<std::string,
        cnetnexus::framework::error::error_code>>
{
    if (snapshot.empty())
        co_return std::unexpected{
            cnetnexus::framework::error::error_code::request_invalid_argument};

    archive_snapshot value;
    value.metadata.archive_id = cnetmod::security::generate_secure_token(8);
    value.metadata.algorithm = canonical_algorithm(algorithm);
    value.metadata.title = std::move(title);
    value.metadata.owner_id = owner_id;
    value.metadata.size_bytes = snapshot.size();
    value.metadata.created_at = cnetmod::unix_time_seconds();
    value.payload = std::move(snapshot);

    const auto saved = co_await archives_->save(value);
    if (!saved)
        co_return std::unexpected{saved.error()};
    co_return value.metadata.archive_id;
}

auto chart_archive_service::fetch(std::string_view algorithm,
    std::string_view id, const archive_access& access)
    -> cnetmod::task<std::expected<std::optional<std::string>,
        cnetnexus::framework::error::error_code>>
{
    const auto loaded = co_await archives_->load(id);
    if (!loaded)
        co_return std::unexpected{loaded.error()};
    if (!loaded->has_value())
        co_return std::optional<std::string>{};

    const auto& value = **loaded;
    if (value.metadata.algorithm != canonical_algorithm(algorithm))
        co_return std::optional<std::string>{};
    if (access.scoped() && !access.may_read(value.metadata.owner_id))
        co_return std::unexpected{access.denial()};
    co_return std::optional<std::string>{value.payload};
}

auto chart_archive_service::search(const archive_query& query,
    const archive_access& access)
    -> cnetmod::task<std::expected<
        cnetnexus::framework::pagination::page_result<archive_index_entry>,
        cnetnexus::framework::error::error_code>>
{
    auto scoped = query;
    scoped.algorithm = canonical_algorithm(query.algorithm);
    if (access.scoped())
        scoped.owner = access.filter();
    co_return co_await archives_->search(scoped);
}

auto chart_archive_service::remove(std::string_view algorithm,
    std::string_view id, const archive_access& caller)
    -> cnetmod::task<std::expected<void,
        cnetnexus::framework::error::error_code>>
{
    const auto found = co_await archives_->find(id);
    if (!found)
        co_return std::unexpected{found.error()};
    if (!found->has_value() ||
        (*found)->algorithm != canonical_algorithm(algorithm))
        co_return std::unexpected{
            cnetnexus::framework::error::error_code::archive_not_found};
    if (caller.scoped() && !caller.may_delete((*found)->owner_id))
        co_return std::unexpected{caller.denial()};

    const auto removed = co_await archives_->remove(id);
    if (!removed)
        co_return std::unexpected{removed.error()};
    co_return {};
}

} // namespace cnetnexus::chart
