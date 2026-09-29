module cnetnexus.chart.service.bazi_service;

import std;
import cnetmod.coro.task;
import cnetmod.core.log;
import cnetmod.protocol.redis;
import cnetnexus.chart.domain.bazi;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;
import cnetnexus.chart.service.compute_executor;

namespace {

/// 支持的公历年份区间；超出范围时上游历法库无法给出可靠节气与干支。
constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;

/// @brief 请求范围校验；只在 service 层判定，不把用户输入直接透给上游。
auto valid(const cnetnexus::chart::bazi::birth_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month == 0 || input.month < -12 || input.month > 12)
        return false;
    if (input.calendar == cnetnexus::chart::bazi::calendar_kind::solar && input.month < 0)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    if (input.minute < 0 || input.minute > 59)
        return false;
    return true;
}

} // namespace

namespace cnetnexus::chart {

auto bazi_service::cache_key(const cnetnexus::chart::bazi::birth_input& input)
    -> std::string
{
    return std::format("{}:{}:{}:{}:{}:{}:{}:{}",
        input.calendar == cnetnexus::chart::bazi::calendar_kind::lunar ? "lunar" : "solar",
        input.year, input.month, input.day, input.hour, input.minute, input.is_male ? 1 : 0,
        input.method == cnetnexus::chart::bazi::analysis_method::mangpai ? "mangpai" : "ziping");
}

auto bazi_service::pai_pan(cnetnexus::chart::bazi::birth_input input)
    -> cnetmod::task<std::expected<cnetnexus::chart::bazi::chart,
        cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    const auto calculate = [this, input] { return algorithm_->pai_pan(input); };
    if (!cache_)
        co_return co_await compute(*compute_, calculate);

    const auto key = cache_key(input);
    // 读失败（Redis 不可用）与解码失败（旧格式）都按未命中处理，排盘结果随后覆盖该键。
    if (auto cached = co_await cache_->get_as<cnetnexus::chart::bazi::chart>(key); cached && *cached)
        co_return std::move(**cached);
    else if (!cached)
        logger::debug{"bazi cache miss on read failure key={} error={}", key, cached.error().message()};

    auto computed = co_await compute(*compute_, calculate);
    if (!computed)
        co_return computed;
    if (auto stored = co_await cache_->set_as(key, *computed, bazi_cache_ttl); !stored)
        logger::warn{"bazi cache write failed key={} error={}", key, stored.error().message()};
    co_return computed;
}

auto bazi_service::liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::bazi::liu_nian_result,
        cnetnexus::framework::error::error_code>>
{
    if (!valid(input.birth))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    // 目标年与出生年同区间；大运序号只接受 0-9（上游固定排十步大运）。
    if (input.target_year < k_min_year || input.target_year > k_max_year)
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    if (input.da_yun_index && (*input.da_yun_index < 0 || *input.da_yun_index > 9))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    co_return co_await compute(*compute_,
        [this, input] { return algorithm_->liu_nian(input); });
}

} // namespace cnetnexus::chart
