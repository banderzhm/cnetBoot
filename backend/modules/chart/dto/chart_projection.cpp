module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.dto.chart_projection;

// Provider projections belong to the DTO layer, not HTTP route handling.

import std;
import cnetmod.json;

namespace cnetnexus::chart::projection {

namespace chart = cnetnexus::chart;

namespace {

    auto merge_provider_object(cnetmod::json::value& destination,
        const cnetmod::json::document& source)
        -> std::expected<void, std::error_code>
    {
        if (!source.is_object())
            return std::unexpected(cnetmod::json::make_error_code(
                cnetmod::json::errc::type_mismatch));
        for (const auto& [key, value] : source.get_object())
            destination[key] = value;
        return {};
    }

    auto casting(int year, int month, int day, int hour, int minute)
        -> cnetmod::json::value
    {
        cnetmod::json::value result;
        result["year"] = year;
        result["month"] = month;
        result["day"] = day;
        result["hour"] = hour;
        result["minute"] = minute;
        return result;
    }

    void optional_text(cnetmod::json::value& target, std::string_view key,
        const std::optional<std::string>& value)
    {
        if (value)
            target[key] = *value;
        else
            target[key] = nullptr;
    }

} // namespace

auto chart_response(const chart::bazi::chart& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value birth;
    birth["year"] = value.birth.year;
    birth["month"] = value.birth.month;
    birth["day"] = value.birth.day;
    birth["hour"] = value.birth.hour;
    birth["minute"] = value.birth.minute;

    cnetmod::json::value pillars;
    pillars["year"] = value.pillars.year;
    pillars["month"] = value.pillars.month;
    pillars["day"] = value.pillars.day;
    pillars["hour"] = value.pillars.hour;

    cnetmod::json::value::array_t xun_kong;
    if (!value.xun_kong_1.empty())
        xun_kong.emplace_back(value.xun_kong_1);
    if (!value.xun_kong_2.empty())
        xun_kong.emplace_back(value.xun_kong_2);

    cnetmod::json::value result;
    result["calendar"] =
        value.birth.calendar == chart::bazi::calendar_kind::lunar ? "lunar"
                                                                  : "solar";
    result["birth"] = std::move(birth);
    result["gender"] = value.birth.is_male ? "male" : "female";
    result["pillars"] = std::move(pillars);
    result["xunKong"] = std::move(xun_kong);
    if (auto merged = merge_provider_object(result, value.chart_projection); !merged)
        return std::unexpected(merged.error());
    result["analysis"] = value.analysis_projection;
    result["analysisZh"] = value.analysis_zh;
    return result;
}

auto chart_response(const chart::liu_yao::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value::array_t changing;
    changing.reserve(value.input.changing_lines.size());
    for (const auto line : value.input.changing_lines)
        changing.emplace_back(line);

    cnetmod::json::value result;
    result["hexagramCode"] = value.input.hexagram_code;
    result["changingLines"] = std::move(changing);
    result["questionKind"] = value.input.kind;
    result["question"] = value.input.question;
    if (auto merged = merge_provider_object(result, value.chart_projection); !merged)
        return std::unexpected(merged.error());
    result["pan"] = value.pan_projection;
    result["analysisZh"] = value.analysis_zh;
    return result;
}

auto chart_response(const chart::mei_hua::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value result;
    result["calendar"] =
        value.input.calendar == chart::mei_hua::calendar_kind::lunar ? "lunar"
                                                                     : "solar";
    result["casting"] = casting(value.input.year, value.input.month,
        value.input.day, value.input.hour, value.input.minute);
    result["questionKind"] = value.input.kind;
    result["question"] = value.input.question;
    if (auto merged = merge_provider_object(result, value.chart_projection); !merged)
        return std::unexpected(merged.error());
    result["pan"] = value.pan_projection;
    result["analysisZh"] = value.analysis_zh;
    return result;
}

auto chart_response(const chart::qi_men::chart_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value result;
    result["calendar"] =
        value.input.calendar == chart::qi_men::calendar_kind::lunar ? "lunar"
                                                                    : "solar";
    result["casting"] = casting(value.input.year, value.input.month,
        value.input.day, value.input.hour, value.input.minute);
    result["questionKind"] = value.input.kind;
    result["question"] = value.input.question;
    optional_text(result, "nianMing", value.input.nian_ming);
    if (auto merged = merge_provider_object(result, value.chart_projection); !merged)
        return std::unexpected(merged.error());
    result["pan"] = value.pan_projection;
    result["analysisZh"] = value.analysis_zh;
    return result;
}

auto chart_response(const chart::zi_wei::chart_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value birth;
    birth["year"] = value.input.year;
    birth["month"] = value.input.month;
    birth["day"] = value.input.day;
    birth["hour"] = value.input.hour;

    cnetmod::json::value result;
    result["calendar"] =
        value.input.calendar == chart::zi_wei::calendar_kind::lunar ? "lunar"
                                                                    : "solar";
    result["birth"] = std::move(birth);
    result["gender"] = value.input.is_male ? "male" : "female";
    result["chart"] = value.chart_projection;
    result["reportZh"] = value.report_zh;
    if (value.horoscope_projection)
        result["horoscope"] = *value.horoscope_projection;
    return result;
}

auto chart_response(const chart::da_liu_ren::cast_result& value)
    -> std::expected<cnetmod::json::value, std::error_code>
{
    cnetmod::json::value result;
    result["casting"] = casting(value.input.year, value.input.month,
        value.input.day, value.input.hour, value.input.minute);
    result["questionKind"] = value.input.kind;
    result["question"] = value.input.question;
    optional_text(result, "benMing", value.input.ben_ming);
    optional_text(result, "xingNian", value.input.xing_nian);
    optional_text(result, "gender", value.input.gender);
    if (value.derived)
    {
        result["derivedBenMing"] = value.derived->ben_ming;
        result["derivedXingNian"] = value.derived->xing_nian;
        result["xingNianGanZhi"] = value.derived->xing_nian_gan_zhi;
        result["nominalAge"] = value.derived->nominal_age;
        result["birthCycleYear"] = value.derived->birth_cycle_year;
        result["xingNianRule"] = value.derived->rule;
    }
    else
    {
        for (const auto key : {"derivedBenMing", "derivedXingNian",
                 "xingNianGanZhi", "nominalAge", "birthCycleYear", "xingNianRule"})
            result[key] = nullptr;
    }
    if (auto merged = merge_provider_object(result, value.chart_projection); !merged)
        return std::unexpected(merged.error());
    result["pan"] = value.pan_projection;
    result["analysisZh"] = value.analysis_zh;
    return result;
}

} // namespace cnetnexus::chart::projection
