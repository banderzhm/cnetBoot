module cnetnexus.chart.service.liu_yao_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

namespace {

constexpr int k_min_year = 1900;
constexpr int k_max_year = 2100;

/// @brief 主卦码必须恰为六位 0/1，顺序为初爻到上爻。
auto valid_hexagram_code(std::string_view code) noexcept -> bool
{
    if (code.size() != 6)
        return false;
    return std::ranges::all_of(code, [](char c)
        {
            return c == '0' || c == '1';
        });
}

/// @brief 请求范围校验；动爻允许重复传入，由用例去重。
auto valid(const cnetnexus::chart::liu_yao::cast_input& input) noexcept -> bool
{
    if (input.year < k_min_year || input.year > k_max_year)
        return false;
    if (input.month < 1 || input.month > 12)
        return false;
    if (input.day < 1 || input.day > 31)
        return false;
    if (input.hour < 0 || input.hour > 23)
        return false;
    if (input.minute < 0 || input.minute > 59)
        return false;
    if (!valid_hexagram_code(input.hexagram_code))
        return false;
    if (input.kind.empty())
        return false;
    return std::ranges::all_of(input.changing_lines, [](int line)
        {
            return line >= 1 && line <= 6;
        });
}

} // namespace

namespace cnetnexus::chart {

auto liu_yao_service::cast(const cnetnexus::chart::liu_yao::cast_input& input) const
    -> cnetmod::task<std::expected<cnetnexus::chart::liu_yao::cast_result, cnetnexus::framework::error::error_code>>
{
    if (!valid(input))
        co_return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    auto normalized = input;
    std::ranges::sort(normalized.changing_lines);
    normalized.changing_lines.erase(std::ranges::unique(normalized.changing_lines).begin(),
        normalized.changing_lines.end());
    co_return co_await compute(*compute_,
        [this, normalized = std::move(normalized)] { return algorithm_->cast(normalized); });
}

} // namespace cnetnexus::chart