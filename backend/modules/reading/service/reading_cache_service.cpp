module cnetnexus.reading.service.reading_cache_service;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.reading.mapper.reading_cache_mapper;
import cnetnexus.reading.service.reading_service;

namespace cnetnexus::reading {
namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 由用例输入算出缓存键（规范化只在 reading_service 里做一次；scope 并进盘面指纹）。
[[nodiscard]] auto key_of(const reading_cache_request& request) -> reading_cache_key
{
    return reading_cache_key{
        .user_id = request.user_id,
        .algorithm = request.algorithm,
        .chart_fingerprint = reading_chart_fingerprint(request.algorithm, request.scope,
            request.report_zh),
        .question_hash = reading_question_hash(request.question)};
}

} // namespace

auto reading_cache_service::lookup(const reading_cache_request& request)
    -> cnetmod::task<std::expected<std::optional<reading_cache_hit>, error_code>>
{
    const auto key = key_of(request);
    const auto found = co_await mapper_->find(key);
    if (!found)
        co_return std::unexpected{found.error()};
    if (!found->has_value())
        co_return std::optional<reading_cache_hit>{};

    const auto& row = **found;
    // 版本不一致 = 旧缓存，按未命中处理：不改行、不计数，等生成成功后被 UPSERT 覆盖。
    if (row.engine_version != reading_engine_version)
        co_return std::optional<reading_cache_hit>{};

    // 命中计数是尽力而为的副作用：失败不阻断本次命中（缓存本体仍然有效）。
    if (const auto counted = co_await mapper_->record_hit(key); !counted)
        logger::warn{"reading cache hit counter failed: algorithm={} user={} error={}",
            key.algorithm, key.user_id, static_cast<int>(counted.error())};

    logger::info{"reading cache hit: algorithm={} user={} fingerprint={} engine={} hit_count={}",
        key.algorithm, key.user_id, key.chart_fingerprint.substr(0, 12), row.engine_version,
        row.hit_count + 1};
    co_return std::optional<reading_cache_hit>{reading_cache_hit{.content = row.content,
        .model = row.model,
        .tokens_in = row.tokens_in,
        .tokens_out = row.tokens_out}};
}

auto reading_cache_service::save(const reading_cache_request& request, const reading_cache_hit& hit)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const auto key = key_of(request);
    reading_cache_row row;
    row.question = request.question;
    row.content = hit.content;
    row.model = hit.model;
    row.tokens_in = hit.tokens_in;
    row.tokens_out = hit.tokens_out;
    row.engine_version = std::string{reading_engine_version};
    const auto written = co_await mapper_->upsert(key, row);
    if (!written)
    {
        logger::warn{"reading cache save failed: algorithm={} user={} error={}", key.algorithm,
            key.user_id, static_cast<int>(written.error())};
        co_return std::unexpected{written.error()};
    }
    logger::info{"reading cache stored: algorithm={} user={} fingerprint={} bytes={}",
        key.algorithm, key.user_id, key.chart_fingerprint.substr(0, 12), row.content.size()};
    co_return std::expected<void, error_code>{};
}

} // namespace cnetnexus::reading
