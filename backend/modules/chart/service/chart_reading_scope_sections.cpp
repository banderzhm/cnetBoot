module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.service.chart_reading_service;

import std;
import cnetmod.json;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;

namespace cnetnexus::chart {

using cnetnexus::reading::reading_scope;

// 投影类型必须有外部链接：Glaze 的字段名反射要求类型具备链接性，
// 因此不能放进匿名命名空间。
namespace scope_projection {

/** @brief 八字盘面投影：只取渲染「生成范围数据」段所需的字段。 */
struct bazi_period_projection
{
    std::optional<int> index;
    std::optional<int> year;
    std::optional<int> age;
    std::optional<int> startAge;
    std::optional<int> endAge;
    std::optional<int> startYear;
    std::optional<int> endYear;
    std::optional<int> lunarMonthIndex;
    std::string ganZhi;
    std::string ganShiShen;
    std::string zhiShiShen;
    std::string startDate;
    std::string endDate;
};

struct bazi_qi_yun_projection
{
    std::optional<int> age;
    std::optional<int> year;
    std::optional<bool> forward;
};

struct bazi_chart_projection
{
    std::vector<bazi_period_projection> daYun;
    std::optional<bazi_qi_yun_projection> qiYun;
};

struct bazi_limit_projection
{
    std::optional<int> targetYear;
    std::optional<bazi_period_projection> daYun;
    std::optional<bazi_period_projection> liuNian;
    std::vector<bazi_period_projection> liuYue;
};

/** @brief 紫微四化投影。 */
struct zi_wei_si_hua_projection
{
    std::string star;
    std::string hua;
};

/** @brief 紫微大限起止虚岁（provider 键名是 snake_case）。 */
struct zi_wei_da_xian_range
{
    std::optional<int> start_age;
    std::optional<int> end_age;
};

struct zi_wei_palace_projection
{
    std::string name;
    std::string gan_zhi;
    std::optional<zi_wei_da_xian_range> da_xian;
};

struct zi_wei_period_projection
{
    std::optional<int> gongIndex;
    std::optional<int> startAge;
    std::optional<int> endAge;
    std::optional<int> year;
    std::optional<int> lunarMonthIndex;
    std::string tianGan;
    std::string diZhi;
    std::string ganZhi;
    std::vector<zi_wei_si_hua_projection> siHua;
};

struct zi_wei_chart_projection
{
    std::vector<zi_wei_palace_projection> palaces;
};

struct zi_wei_horoscope_projection
{
    std::optional<int> targetYear;
    std::optional<int> currentAge;
    std::optional<zi_wei_period_projection> daXian;
    std::optional<zi_wei_period_projection> liuNian;
    std::optional<zi_wei_period_projection> liuYue;
};

} // namespace scope_projection

using namespace cnetnexus::chart::scope_projection;

namespace error = cnetnexus::framework::error;

namespace {

    template <typename Projection>
    [[nodiscard]] auto parse_projection(const cnetmod::json::document& document)
        -> std::optional<Projection>
    {
        const auto encoded = cnetmod::json::write_document(document);
        if (!encoded)
            return std::nullopt;
        auto parsed = cnetmod::json::parse_lenient<Projection>(*encoded);
        if (!parsed)
            return std::nullopt;
        return std::move(*parsed);
    }

    /// @brief 流月起止日期文本：上游只给起始，结束为空时不输出悬空的「至 」。
    [[nodiscard]] auto date_range_text(const bazi_period_projection& value)
        -> std::string
    {
        const auto& start_date = value.startDate;
        const auto& end_date = value.endDate;
        if (start_date.empty())
            return end_date;
        if (end_date.empty())
            return start_date;
        return std::format("{} 至 {}", start_date, end_date);
    }

    /// @brief 渲染十神短语（干支由各段自己写在前面），供大运/流年/流月共用。
    [[nodiscard]] auto shi_shen_brief(const bazi_period_projection& value)
        -> std::string
    {
        if (value.ganShiShen.empty() && value.zhiShiShen.empty())
            return {};
        return std::format("天干十神 {}，地支十神 {}", value.ganShiShen,
            value.zhiShiShen);
    }

    /// @brief 八字大运段：起运 + 十步大运 + 目标年所处大运（取流年投影，不按年份自行匹配）。
    void append_bazi_dayun(std::string& section, const bazi_chart_projection& chart,
        const std::optional<bazi_limit_projection>& liu_nian)
    {
        const auto& da_yun = chart.daYun;
        section.append("[大运] ");
        if (chart.qiYun)
        {
            section.append(std::format("起运 {} 岁（{} 年）",
                chart.qiYun->age.value_or(0), chart.qiYun->year.value_or(0)));
            if (chart.qiYun->forward)
                section.append(*chart.qiYun->forward ? "，顺排" : "，逆排");
            section.append("，");
        }
        if (da_yun.empty())
        {
            section.append("盘面投影未给出大运列表。\n");
            return;
        }

        section.append(std::format("共 {} 步：\n", da_yun.size()));
        int step_index = 0;
        for (const auto& step : da_yun)
        {
            ++step_index;
            section.append(std::format(
                "  第 {} 步 {}（{}—{} 岁 / {}—{} 年；天干十神 {}，地支十神 {}）\n",
                step_index, step.ganZhi, step.startAge.value_or(0),
                step.endAge.value_or(0), step.startYear.value_or(0),
                step.endYear.value_or(0), step.ganShiShen, step.zhiShiShen));
        }

        // 目标年所处大运：直接用流年投影已经判定好的 daYun（index 与上面的列表同序），
        // 童限（未起运）时投影给 null，这里也不编造。
        if (liu_nian && liu_nian->daYun)
        {
            const auto& current = *liu_nian->daYun;
            section.append(std::format(
                "  当前大运（目标年 {}）：第 {} 步 {}（{}—{} 岁 / {}—{} 年；天干十神 {}，地支十神 {}）\n",
                liu_nian->targetYear.value_or(0), current.index.value_or(0) + 1,
                current.ganZhi, current.startAge.value_or(0),
                current.endAge.value_or(0), current.startYear.value_or(0),
                current.endYear.value_or(0), current.ganShiShen,
                current.zhiShiShen));
        }
    }

    /// @brief 八字流年段：目标年干支、虚岁与十神（取流年投影，不重算）。
    void append_bazi_liunian(std::string& section,
        const std::optional<bazi_limit_projection>& liu_nian)
    {
        if (!liu_nian || !liu_nian->liuNian)
        {
            section.append("[流年] 目标年流年数据缺失。\n");
            return;
        }
        const auto& nian = *liu_nian->liuNian;
        section.append(std::format("[流年] {} 年 {}，虚岁 {}；{}\n",
            nian.year.value_or(0), nian.ganZhi, nian.age.value_or(0),
            shi_shen_brief(nian)));
    }

    /// @brief 八字流月段：目标年 + 目标月对应的节气月干支与起止公历日期。
    void append_bazi_liuyue(std::string& section,
        const std::optional<bazi_limit_projection>& liu_nian, int target_lunar_month)
    {
        if (liu_nian)
        {
            for (const auto& month : liu_nian->liuYue)
            {
                if (month.lunarMonthIndex != target_lunar_month)
                    continue;
                const auto range = date_range_text(month);
                section.append(std::format(
                    "[流月] {} 年第 {} 月（正月=寅月起的节气月）{}{}；{}\n",
                    liu_nian->targetYear.value_or(0), target_lunar_month,
                    month.ganZhi,
                    range.empty() ? std::string{}
                                  : std::format("，{}", range),
                    shi_shen_brief(month)));
                return;
            }
        }
        section.append(std::format("[流月] 目标年第 {} 月的流月数据缺失。\n",
            target_lunar_month));
    }

    /// @brief 紫微某一宫（palaces[] 下标，0 = 寅宫）的宫名或宫干支；越界返回空串。
    [[nodiscard]] auto palace_field(const zi_wei_chart_projection& chart, int index,
        bool name) -> std::string
    {
        if (index < 0 || static_cast<std::size_t>(index) >= chart.palaces.size())
            return {};
        const auto& palace = chart.palaces[static_cast<std::size_t>(index)];
        return name ? palace.name : palace.gan_zhi;
    }

    /// @brief 紫微四化短语：固定按投影顺序拼「星名 + 化象」（如「太阳化禄」）。
    [[nodiscard]] auto si_hua_text(const zi_wei_period_projection& value)
        -> std::string
    {
        std::string text;
        for (const auto& entry : value.siHua)
        {
            if (entry.star.empty() && entry.hua.empty())
                continue;
            if (!text.empty())
                text.append("、");
            text.append(entry.star);
            text.append(entry.hua);
        }
        return text;
    }

    /// @brief 紫微大限段：十二步大限 + 运限投影给出的当前大限与四化。
    void append_zi_wei_daxian(std::string& section,
        const zi_wei_chart_projection& chart,
        const std::optional<zi_wei_horoscope_projection>& horoscope)
    {
        struct da_xian_step
        {
            int start_age{};
            int end_age{};
            std::string name;
            std::string gan_zhi;
        };

        std::vector<da_xian_step> steps;
        for (const auto& palace : chart.palaces)
        {
            if (!palace.da_xian || !palace.da_xian->start_age ||
                !palace.da_xian->end_age)
                continue;
            steps.push_back(da_xian_step{*palace.da_xian->start_age,
                *palace.da_xian->end_age, palace.name, palace.gan_zhi});
        }
        // 宫位按地支排布，大限起止虚岁则从命宫起顺/逆行；按虚岁排序只为阅读顺序，不改任何口径。
        std::ranges::sort(steps, {}, &da_xian_step::start_age);

        if (steps.empty())
            section.append("[大限] 盘面投影未给出大限列表。\n");
        else
        {
            section.append(std::format("[大限] 共 {} 步：\n", steps.size()));
            int step_index = 0;
            for (const auto& step : steps)
            {
                ++step_index;
                section.append(std::format("  第 {} 步 {}—{} 岁 {}（{}）\n",
                    step_index, step.start_age, step.end_age, step.name,
                    step.gan_zhi));
            }
        }

        // 当前大限：直接用运限投影已经判定好的 daXian，不在这里按虚岁匹配。
        if (horoscope && horoscope->daXian)
        {
            const auto& da_xian = *horoscope->daXian;
            const int gong_index = da_xian.gongIndex.value_or(-1);
            const auto gan_zhi = da_xian.tianGan + da_xian.diZhi;
            section.append(std::format(
                "  当前大限（目标年 {}，虚岁 {}）：{}—{} 岁 {}（{}）",
                horoscope->targetYear.value_or(0),
                horoscope->currentAge.value_or(0), da_xian.startAge.value_or(0),
                da_xian.endAge.value_or(0),
                palace_field(chart, gong_index, true), gan_zhi));
            if (const auto si_hua = si_hua_text(da_xian); !si_hua.empty())
                section.append(std::format("；大限四化：{}", si_hua));
            section.append("\n");
        }
    }

    /// @brief 紫微流年段：流年干支、流年命宫落宫与四化（取运限投影，不重算）。
    void append_zi_wei_liunian(std::string& section,
        const zi_wei_chart_projection& chart,
        const std::optional<zi_wei_horoscope_projection>& horoscope)
    {
        if (!horoscope || !horoscope->liuNian)
        {
            section.append("[流年] 目标年流年数据缺失。\n");
            return;
        }
        const auto& nian = *horoscope->liuNian;
        const int gong_index = nian.gongIndex.value_or(-1);
        section.append(std::format("[流年] {} 年 {}；流年命宫落于本命「{}」（宫干支 {}）",
            nian.year.value_or(0), nian.ganZhi, palace_field(chart, gong_index, true),
            palace_field(chart, gong_index, false)));
        if (const auto si_hua = si_hua_text(nian); !si_hua.empty())
            section.append(std::format("；流年四化：{}", si_hua));
        section.append("\n");
    }

    /// @brief 紫微流月段：目标农历月的流月干支、流月命宫与四化（取运限投影，不重算）。
    void append_zi_wei_liuyue(std::string& section,
        const zi_wei_chart_projection& chart,
        const std::optional<zi_wei_horoscope_projection>& horoscope)
    {
        if (!horoscope || !horoscope->liuYue)
        {
            section.append("[流月] 目标农历月的流月数据缺失。\n");
            return;
        }
        const auto& yue = *horoscope->liuYue;
        const int gong_index = yue.gongIndex.value_or(-1);
        section.append(
            std::format("[流月] {} 年农历 {} 月 {}；流月命宫落于本命「{}」（宫干支 {}）",
                horoscope->targetYear.value_or(0),
                yue.lunarMonthIndex.value_or(0), yue.ganZhi,
                palace_field(chart, gong_index, true),
                palace_field(chart, gong_index, false)));
        if (const auto si_hua = si_hua_text(yue); !si_hua.empty())
            section.append(std::format("；流月四化：{}", si_hua));
        section.append("\n");
    }

} // namespace

auto bazi_scope_section(const cnetmod::json::document& chart_document,
    const cnetmod::json::document* liu_nian_document,
    const reading_scope_request& request) -> std::string
{
    const auto& scopes = request.scopes;
    const bool want_dayun = scopes.contains(reading_scope::dayun);
    const bool want_liunian = scopes.contains(reading_scope::liunian);
    const bool want_liuyue = scopes.contains(reading_scope::liuyue);
    if (!want_dayun && !want_liunian && !want_liuyue)
        return {};

    const auto chart = parse_projection<bazi_chart_projection>(chart_document);
    const auto liu_nian = liu_nian_document
        ? parse_projection<bazi_limit_projection>(*liu_nian_document)
        : std::nullopt;
    if (!chart && !liu_nian)
        return {};

    std::string section{"【生成范围数据】\n"};
    if (want_dayun)
        append_bazi_dayun(section, chart.value_or(bazi_chart_projection{}), liu_nian);
    if (want_liunian)
        append_bazi_liunian(section, liu_nian);
    if (want_liuyue)
        append_bazi_liuyue(section, liu_nian, request.target_lunar_month.value_or(0));
    return section;
}

auto zi_wei_scope_section(const cnetmod::json::document& chart_document,
    const cnetmod::json::document* horoscope_document,
    const reading_scope_request& request)
    -> std::string
{
    const auto& scopes = request.scopes;
    const bool want_dayun = scopes.contains(reading_scope::dayun);
    const bool want_liunian = scopes.contains(reading_scope::liunian);
    const bool want_liuyue = scopes.contains(reading_scope::liuyue);
    if (!want_dayun && !want_liunian && !want_liuyue)
        return {};

    const auto chart = parse_projection<zi_wei_chart_projection>(chart_document);
    const auto horoscope = horoscope_document
        ? parse_projection<zi_wei_horoscope_projection>(*horoscope_document)
        : std::nullopt;
    if (!chart && !horoscope)
        return {};

    std::string section{"【生成范围数据】\n"};
    if (want_dayun)
        append_zi_wei_daxian(section, chart.value_or(zi_wei_chart_projection{}),
            horoscope);
    if (want_liunian)
        append_zi_wei_liunian(section, chart.value_or(zi_wei_chart_projection{}),
            horoscope);
    if (want_liuyue)
        append_zi_wei_liuyue(section, chart.value_or(zi_wei_chart_projection{}),
            horoscope);
    return section;
}

} // namespace cnetnexus::chart
