module cnetnexus.reading.mapper.reading_cache_mapper;

import std;
import cnetmod.core.time;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_cache_record;

namespace cnetnexus::reading {
namespace {

namespace db_api = cnetmod::database;
namespace orm_api = cnetmod::orm;
using error_code = cnetnexus::framework::error::error_code;

constexpr std::string_view upsert_statement{"ReadingCacheMapper.upsert"};
constexpr std::string_view record_hit_statement{"ReadingCacheMapper.recordHit"};

[[nodiscard]] auto now_wall_clock() -> db_api::calendar_datetime
{
    return db_api::datetime_from_unix_seconds(cnetmod::unix_time_seconds())
        .value_or(db_api::calendar_datetime{});
}

[[nodiscard]] auto to_row(const reading_cache_record& value) -> reading_cache_row
{
    return reading_cache_row{
        .question = value.question.value_or(std::string{}),
        .content = value.content,
        .model = value.model,
        .tokens_in = static_cast<int>(value.tokens_in),
        .tokens_out = static_cast<int>(value.tokens_out),
        .engine_version = value.engine_version,
        .hit_count = value.hit_count};
}

/// @brief 唯一键四列绑参（find / upsert / recordHit 共用）。
[[nodiscard]] auto key_parameters(const reading_cache_key& key) -> orm_api::param_context
{
    orm_api::param_context parameters;
    parameters.set("user_id", key.user_id);
    parameters.set("algorithm", key.algorithm);
    parameters.set("chart_fingerprint", key.chart_fingerprint);
    parameters.set("question_hash", key.question_hash);
    return parameters;
}

} // namespace

mysql_reading_cache_mapper::mysql_reading_cache_mapper(
    cnetmod::application::managed_repository<reading_cache_record> cache,
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry)
    : cache_(std::move(cache)), registry_(std::move(registry))
{
    if (!registry_)
        throw std::invalid_argument("reading cache mapper registry is required");
}

auto mysql_reading_cache_mapper::find(const reading_cache_key& key)
    -> cnetmod::task<std::expected<std::optional<reading_cache_row>, error_code>>
{
    // 唯一键四列等值 + LIMIT 1：走 query_wrapper（成员指针 -> 列名由模型元数据解析），
    // 结果按列名映射回模型，不再手写下标。
    auto query = orm_api::query_wrapper<reading_cache_record>{};
    query.eq(&reading_cache_record::user_id, key.user_id)
        .eq(&reading_cache_record::algorithm, key.algorithm)
        .eq(&reading_cache_record::chart_fingerprint, key.chart_fingerprint)
        .eq(&reading_cache_record::question_hash, key.question_hash)
        .limit(1);

    auto found = co_await cache_.list(query);
    if (!found.ok())
        co_return std::unexpected{found.framework_error ? error_code::upstream_unavailable
                                                        : error_code::system_internal_error};
    if (found.data.empty())
        co_return std::optional<reading_cache_row>{};
    co_return std::optional<reading_cache_row>{to_row(found.data.front())};
}

auto mysql_reading_cache_mapper::upsert(const reading_cache_key& key,
    const reading_cache_row& row) -> cnetmod::task<std::expected<void, error_code>>
{
    // INSERT ... ON DUPLICATE KEY UPDATE 是构建器表达不了的（insert 会撞唯一键、update 不会
    // 建行），保留在 XML mapper 里；时间列按 UTC 墙钟参数写入。
    auto parameters = key_parameters(key);
    parameters.set("question", row.question);
    parameters.set("content", row.content);
    parameters.set("model", row.model);
    parameters.set("tokens_in", row.tokens_in);
    parameters.set("tokens_out", row.tokens_out);
    parameters.set("engine_version", row.engine_version);
    parameters.set("now_at", now_wall_clock());
    auto written = co_await cache_.execute_xml(*registry_, upsert_statement, parameters);
    if (!written.ok())
        co_return std::unexpected{written.framework_error
                ? error_code::upstream_unavailable
                : error_code::system_internal_error};
    co_return {};
}

auto mysql_reading_cache_mapper::record_hit(const reading_cache_key& key)
    -> cnetmod::task<std::expected<void, error_code>>
{
    // hit_count = hit_count + 1 是「列自增」语义，构建器只能写常量，保留在 XML mapper。
    auto parameters = key_parameters(key);
    parameters.set("now_at", now_wall_clock());
    auto touched = co_await cache_.execute_xml(*registry_, record_hit_statement, parameters);
    if (!touched.ok())
        co_return std::unexpected{touched.framework_error
                ? error_code::upstream_unavailable
                : error_code::system_internal_error};
    co_return {};
}

auto validate_reading_cache_mapper_registry(const cnetmod::orm::mapper_registry& registry)
    -> std::expected<void, std::string>
{
    // 启动期校验必需语句，避免运行到第一个请求才发现 XML 写错。
    // find 已改走 query_wrapper，XML 只留构建器表达不了的 upsert / recordHit。
    for (const auto statement : {upsert_statement, record_hit_statement})
    {
        if (registry.find_statement(statement) == nullptr)
            return std::unexpected{std::string{"reading cache mapper 缺少语句："} +
                std::string{statement}};
    }
    return {};
}

} // namespace cnetnexus::reading
