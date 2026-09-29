module cnetnexus.chart.service.xuanxue.bazi_liu_nian_projection;

import std;
import nlohmann.json;
import ZhouYi.BaZi;
import ZhouYi.BaZiAnalysis.Contract;
import ZhouYi.BaZiAnalysis.Report;
import ZhouYi.BaZiAnalysis.ZiPing.Contract;
import ZhouYi.BaZiBase;
import ZhouYi.BaZiController;
import ZhouYi.BaZiPresenter;
import ZhouYi.GanZhi;
import ZhouYi.ZhMapper;
import ZhouYi.tyme;
import cnetnexus.chart.domain.bazi;
import cnetnexus.framework.error;
import cnetnexus.chart.service.xuanxue.bazi_projection;


namespace cnetnexus::chart {

/// @brief 单条流年（含是否当前公历年）。
auto bazi_liu_nian_json(const ZhouYi::BaZi::LiuNian& value, int current_year) -> nlohmann::ordered_json
{
    return {{"year", value.year},
        {"ganZhi", value.pillar.to_string()},
        {"gan", value.pillar.stem()},
        {"zhi", value.pillar.branch()},
        {"age", value.age},
        {"ganShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(value.gan_shi_shen)}},
        {"zhiShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(value.zhi_shi_shen)}},
        {"isCurrent", value.year == current_year}};
}

/// @brief 当前公历年；仅用于 isCurrent 角标，属日历比较，不是命理推演。
auto bazi_current_solar_year() -> int
{
    const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    return static_cast<int>(std::chrono::year_month_day{today}.year());
}

/// @brief 生成流年盘面 JSON（camelCase）。
///
/// 数据全部取上游 BaZiResult：该年流年 get_liu_nian(year)，该大运下十个流年同法，
/// 该年十二流月 get_liu_yue_list(year)，所处大运取 DaYunSystem 的大运列表。
/// 适配层不按 startYear + i 生成年份，也不自行推流月干支。
auto bazi_liu_nian_projection_json(
    const cnetnexus::chart::bazi::liu_nian_input& input, const ZhouYi::BaZi::BaZiResult& result) -> std::string
{
    const auto& birth = input.birth;
    const int target_year = input.target_year;
    const int current_year = bazi_current_solar_year();
    const auto& da_yun_list = result.da_yun_system.get_da_yun_list();

    // 目标年所处大运：显式指定序号优先，否则按上游给出的 start_year/end_year 命中；
    // 目标年落在童限（未起运）时不输出 daYun 键内的结构，daYun 为 null。
    std::optional<std::size_t> da_yun_index;
    if (input.da_yun_index && static_cast<std::size_t>(*input.da_yun_index) < da_yun_list.size())
        da_yun_index = static_cast<std::size_t>(*input.da_yun_index);
    else
        for (std::size_t index = 0; index < da_yun_list.size(); ++index)
            if (target_year >= da_yun_list[index].start_year && target_year <= da_yun_list[index].end_year)
            {
                da_yun_index = index;
                break;
            }

    nlohmann::ordered_json da_yun = nullptr;
    if (da_yun_index)
    {
        const auto& step = da_yun_list[*da_yun_index];
        da_yun = {{"index", static_cast<int>(*da_yun_index)},
            {"ganZhi", step.pillar.to_string()},
            {"gan", step.pillar.stem()},
            {"zhi", step.pillar.branch()},
            {"element", bazi_element_json(step.pillar)},
            {"startAge", step.start_age},
            {"endAge", step.end_age},
            {"startYear", step.start_year},
            {"endYear", step.end_year},
            {"ganShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(step.gan_shi_shen)}},
            {"zhiShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(step.zhi_shi_shen)}}};
        auto list = nlohmann::ordered_json::array();
        for (int year = step.start_year; year <= step.end_year; ++year)
            list.push_back(bazi_liu_nian_json(result.get_liu_nian(year), current_year));
        da_yun["liuNian"] = std::move(list);
    }

    auto liu_yue = nlohmann::ordered_json::array();
    for (const auto& month : result.get_liu_yue_list(target_year))
        liu_yue.push_back({{"lunarMonthIndex", month.lunar_month_index},
            {"ganZhi", month.pillar.to_string()},
            {"gan", month.pillar.stem()},
            {"zhi", month.pillar.branch()},
            {"ganShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(month.gan_shi_shen)}},
            {"zhiShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(month.zhi_shi_shen)}},
            {"startDate", month.start_date},
            {"endDate", month.end_date}});

    nlohmann::ordered_json birth_json;
    birth_json["year"] = birth.year;
    birth_json["month"] = birth.month;
    birth_json["day"] = birth.day;
    birth_json["hour"] = birth.hour;
    birth_json["minute"] = birth.minute;

    nlohmann::ordered_json document;
    document["calendar"] = birth.calendar == cnetnexus::chart::bazi::calendar_kind::lunar ? "lunar" : "solar";
    document["birth"] = std::move(birth_json);
    document["gender"] = birth.is_male ? "male" : "female";
    document["method"] = birth.method == cnetnexus::chart::bazi::analysis_method::mangpai ? "mangpai" : "ziping";
    document["targetYear"] = target_year;
    document["currentYear"] = current_year;
    document["daYun"] = std::move(da_yun);
    document["liuNian"] = bazi_liu_nian_json(result.get_liu_nian(target_year), current_year);
    document["liuYue"] = std::move(liu_yue);
    return document.dump();
}

/// @brief 运限详情的固定跨度，与上游 examples/example_ba_zi.cpp 同量：
///        大运 10 步、流年从目标年起 30 年、流月目标年整年 12 个节气月。
constexpr int k_qi_yun_dayun_steps = 10;
constexpr int k_qi_yun_liu_nian_years = 30;

/// @brief 运限详情中文表：全部取上游 presenter 的 to_zh_*，本层只负责挑跨度与拼接顺序，
///        不自己排版、不重算干支。顺序照 example：大运表 → 童限详情 → 流年表 → 流月表
///        （→ 目标月所在公历月的流日表）。
auto bazi_qi_yun_zh(const cnetnexus::chart::bazi::liu_nian_input& input,
    const ZhouYi::BaZi::BaZiResult& result) -> std::string
{
    std::string text;
    text += ZhouYi::BaZiPresenter::to_zh_da_yun(result, k_qi_yun_dayun_steps);
    text += "\n";
    text += ZhouYi::BaZiPresenter::to_zh_child_limit_detail(result);
    text += "\n";
    text += ZhouYi::BaZiPresenter::to_zh_liu_nian(result, input.target_year, k_qi_yun_liu_nian_years);
    text += "\n";
    text += ZhouYi::BaZiPresenter::to_zh_liu_yue(result, input.target_year);

    // 流日：只有给了目标农历月才有意义，且按「整月」给（30 天，上游 display_liu_ri 会按该月实际
    // 天数收口）。公历月从该节气月的起始日期文本取：上游 LiuYue::start_date 形如 "2月4日起"
    // （ba_zi.cppm:470-472，**不含年份**），因此年份按上游自己的 term_year 口径补：
    // 丑月（lunar_month_index == 12）的节气在下一年 1 月，其余都在目标年（ba_zi.cppm:445-452）。
    // 取不到起止日期就整段不出，不按农历月号猜公历月。
    if (input.target_lunar_month)
    {
        const auto months = result.get_liu_yue_list(input.target_year);
        for (const auto& month : months)
        {
            if (month.lunar_month_index != *input.target_lunar_month)
                continue;
            const std::string_view start{month.start_date};
            const auto month_mark = start.find(std::string_view{"月"});
            int solar_month = 0;
            if (month_mark != std::string_view::npos)
            {
                const auto parsed = std::from_chars(start.data(), start.data() + month_mark, solar_month);
                if (parsed.ec != std::errc{})
                    solar_month = 0;
            }
            if (solar_month < 1 || solar_month > 12)
                break;
            const int solar_year = month.lunar_month_index == 12 ? input.target_year + 1 : input.target_year;
            text += "\n";
            text += ZhouYi::BaZiPresenter::to_zh_liu_ri(result, solar_year, solar_month, 30);
            break;
        }
    }
    return text;
}

} // namespace cnetnexus::chart
