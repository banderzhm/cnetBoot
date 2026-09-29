module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.reading_scope;

import std;
import cnetmod.json;
import cnetnexus.framework.error;
import cnetnexus.framework.web.query;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.chart.service.chart_reading_service;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace web = cnetnexus::framework::web;
using cnetnexus::reading::reading_scope;

namespace {

    /// 目标年与农历月/日的合法区间；与 chart / liu_nian 端点同一口径，不另立标准。
    constexpr int k_min_year = 1900;
    constexpr int k_max_year = 2100;
    constexpr int k_min_lunar_month = 1;
    constexpr int k_max_lunar_month = 12;
    constexpr int k_min_lunar_day = 1;
    constexpr int k_max_lunar_day = 30;

    /// @brief 去掉首尾 ASCII 空白。
    [[nodiscard]] auto trimmed(std::string_view value) -> std::string_view
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos)
            return {};
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    /// @brief 可选整数参数：缺失返回 nullopt；出现但非整数或越界返回带文案的 400。
    [[nodiscard]] auto optional_integer(std::string_view query, std::string_view key,
        int minimum, int maximum, std::string_view range_text)
        -> std::expected<std::optional<int>, reading_scope_error>
    {
        if (!web::query::value(query, key))
            return std::optional<int>{};
        const auto parsed = web::query::integer(query, key);
        if (!parsed)
            return std::unexpected{reading_scope_error{
                error::error_code::request_invalid_argument,
                std::format("{} 必须是整数", key)}};
        if (*parsed < minimum || *parsed > maximum)
            return std::unexpected{reading_scope_error{
                error::error_code::request_invalid_argument,
                std::format("{} 需在 {} 之间", key, range_text)}};
        return std::optional<int>{*parsed};
    }

} // namespace

auto parse_reading_scope_query(std::string_view algorithm, std::string_view query)
    -> std::expected<reading_scope_request, reading_scope_error>
{
    const auto raw_scope = web::query::value(query, "scope");
    const bool scope_given = raw_scope.has_value() && !trimmed(*raw_scope).empty();

    // 1. 只有 ba_zi 与 zi_wei 支持生成范围；其余术数只要出现 scope（哪怕取值合法）一律 400。
    if (scope_given && algorithm != "ba_zi" && algorithm != "zi_wei")
        return std::unexpected{reading_scope_error{
            error::error_code::request_invalid_argument,
            "scope 生成范围仅对 ba_zi 与 zi_wei 生效"}};

    reading_scope_request request;
    if (scope_given)
    {
        const auto parsed = cnetnexus::reading::parse_reading_scope_list(*raw_scope);
        if (!parsed)
            return std::unexpected{reading_scope_error{
                error::error_code::request_invalid_argument,
                std::format("scope 取值非法（{}）；可选值：benming、dayun、liunian、liuyue",
                    parsed.error().empty() ? "空项" : parsed.error())}};
        request.scopes = *parsed;
    }

    // 2. target_* 取值：出现即校验，非整数或越界一律 400，不静默当成未提供。
    const auto target_year = optional_integer(query, "target_year", k_min_year,
        k_max_year, "1900—2100");
    if (!target_year)
        return std::unexpected{target_year.error()};
    request.target_year = *target_year;
    const auto target_lunar_month = optional_integer(query, "target_lunar_month",
        k_min_lunar_month, k_max_lunar_month, "1—12");
    if (!target_lunar_month)
        return std::unexpected{target_lunar_month.error()};
    request.target_lunar_month = *target_lunar_month;
    const auto target_lunar_day = optional_integer(query, "target_lunar_day",
        k_min_lunar_day, k_max_lunar_day, "1—30");
    if (!target_lunar_day)
        return std::unexpected{target_lunar_day.error()};
    request.target_lunar_day = *target_lunar_day;
    // 时辰地支汉字原样透传：汉字→地支的唯一映射在上游适配层，本仓库不维护第二份对照表。
    if (const auto target_hour = web::query::value(query, "target_hour"); target_hour)
        request.target_hour = std::string{*target_hour};

    // 3. 生成范围对 target_* 的依赖：缺什么就点明什么，不替调用方补默认值。
    const bool has_year = request.target_year.has_value();
    const bool has_month = request.target_lunar_month.has_value();
    const bool has_day = request.target_lunar_day.has_value();
    const bool has_hour = request.target_hour.has_value();
    if (request.scopes.contains(reading_scope::liuyue) && (!has_year || !has_month))
        return std::unexpected{reading_scope_error{
            error::error_code::request_invalid_argument,
            "生成范围包含流月时必须提供 target_year 与 target_lunar_month"}};
    if (request.scopes.contains(reading_scope::liunian) && !has_year)
        return std::unexpected{reading_scope_error{
            error::error_code::request_invalid_argument,
            "生成范围包含流年时必须提供 target_year"}};
    if ((has_month || has_day || has_hour) && !has_year)
        return std::unexpected{reading_scope_error{
            error::error_code::request_invalid_argument,
            "提供 target_lunar_month、target_lunar_day 或 target_hour 时必须同时提供 target_year"}};
    if ((has_day || has_hour) && !has_month)
        return std::unexpected{reading_scope_error{
            error::error_code::request_invalid_argument,
            "提供 target_lunar_day 或 target_hour 时必须同时提供 target_lunar_month"}};

    return request;
}

} // namespace cnetnexus::chart
