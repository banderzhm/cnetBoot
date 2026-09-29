module cnetnexus.chart.service.xuanxue.bazi_projection;

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


namespace cnetnexus::chart {

using ZhouYi::BaZiBase::Pillar;
using ZhouYi::GanZhi::DiZhi;
using ZhouYi::GanZhi::ShiShen;
using ZhouYi::GanZhi::TianGan;
using ZhouYi::GanZhi::WuXing;
using ZhouYi::GanZhi::YinYang;

/// @brief 四柱固定顺序：年、月、日、时，与上游 BaZiBase::get_pillars 一致。
constexpr std::array<std::string_view, 4> k_pillar_keys{"year", "month", "day", "hour"};

/// @brief 藏干层位（本气、中气、余气），顺序即上游 get_cang_gan 的返回顺序。
constexpr std::array<std::string_view, 3> k_hidden_levels{"本气", "中气", "余气"};

/// @brief 五行到前端查表键的映射；配色分组属于展示约定，不参与任何命理推演。
auto bazi_element_key(WuXing element) -> std::string_view
{
    switch (element)
    {
    case WuXing::Mu:
        return "wood";
    case WuXing::Huo:
        return "fire";
    case WuXing::Tu:
        return "earth";
    case WuXing::Jin:
        return "metal";
    case WuXing::Shui:
        return "water";
    }
    return "";
}

/// @brief 十神到角色色分组键的映射（比劫/食伤/财/官杀/印），分组口径取前端设计 §5.3。
auto bazi_shi_shen_tone(ShiShen value) -> std::string_view
{
    switch (value)
    {
    case ShiShen::BiJian:
    case ShiShen::JieCai:
        return "bi_jie";
    case ShiShen::ShiShen:
    case ShiShen::ShangGuan:
        return "shi_shang";
    case ShiShen::PianCai:
    case ShiShen::ZhengCai:
        return "cai";
    case ShiShen::QiSha:
    case ShiShen::ZhengGuan:
        return "guan_sha";
    case ShiShen::PianYin:
    case ShiShen::ZhengYin:
        return "yin";
    }
    return "";
}

/// @brief 某柱地支的藏干清单：天干、相对日干的十神、十神分组键与层位。
///
/// 藏干与十神一律调上游 ZhouYi.GanZhi：get_cang_gan 给本气/中气/余气顺序，
/// get_shi_shen 给十神；适配层不另建藏干表，也不重排层位。
auto bazi_hidden_stems_json(TianGan day_gan, DiZhi zhi) -> nlohmann::ordered_json
{
    auto hidden = nlohmann::ordered_json::array();
    const auto stems = ZhouYi::GanZhi::get_cang_gan(zhi);
    for (std::size_t index = 0; index < stems.size(); ++index)
    {
        const auto ten_god = ZhouYi::GanZhi::get_shi_shen(day_gan, stems[index]);
        hidden.push_back({{"gan", std::string{ZhouYi::GanZhi::Mapper::to_zh(stems[index])}},
            {"tenGod", std::string{ZhouYi::GanZhi::shi_shen_to_zh(ten_god)}},
            {"tenGodTone", std::string{bazi_shi_shen_tone(ten_god)}},
            {"level", std::string{k_hidden_levels[std::min<std::size_t>(index, k_hidden_levels.size() - 1)]}}});
    }
    return hidden;
}

/// @brief 单柱五行对象：天干五行与地支五行各一个查表键。
auto bazi_element_json(const Pillar& pillar) -> nlohmann::ordered_json
{
    return {{"gan", std::string{bazi_element_key(ZhouYi::GanZhi::get_wu_xing(pillar.gan))}},
        {"zhi", std::string{bazi_element_key(ZhouYi::GanZhi::get_wu_xing(pillar.zhi))}}};
}

/// @brief 农历回显。
///
/// 公历入口用上游公共时间模块 ZhouYi.tyme 的 SolarDay::from_ymd(...).get_lunar_day()
/// 换算（与 BaZiBase::SolarDate::to_lunar_string @ ba_zi_base.cppm:307 同一入口）；
/// 农历入口直接用 LunarDay::from_ymd（与 pai_pan_lunar 用的同一模块）。不另造历法。
/// month 沿用上游约定，闰月为负数；text 取上游 LunarDay::to_string()。
auto bazi_lunar_json(const cnetnexus::chart::bazi::birth_input& input) -> nlohmann::ordered_json
{
    const auto lunar_day = input.calendar == cnetnexus::chart::bazi::calendar_kind::lunar
        ? tyme::LunarDay::from_ymd(input.year, input.month, input.day)
        : tyme::SolarDay::from_ymd(input.year, input.month, input.day).get_lunar_day();
    return {{"year", lunar_day.get_year()},
        {"month", lunar_day.get_month()},
        {"day", lunar_day.get_day()},
        {"isLeap", lunar_day.get_lunar_month().is_leap()},
        {"text", lunar_day.to_string()}};
}

/// @brief 盲派结构投影；只在 method=mangpai 且上游确实产出 blind_analysis 时调用。
///
/// 字段与上游 BlindAnalysisDetail 一一对应，只做 camelCase 改名与柱位映射，
/// 不新增任何盲派规则，也不把子平结论混进来。
auto bazi_mang_pai_json(const ZhouYi::BaZiAnalysis::BlindAnalysisDetail& blind) -> nlohmann::ordered_json
{
    auto palaces = nlohmann::ordered_json::array();
    for (const auto& palace : blind.palaces)
    {
        palaces.push_back({{"position", palace.position},
            {"pillar", std::string{k_pillar_keys[static_cast<std::size_t>(std::clamp(palace.position, 0, 3))]}},
            {"palace", palace.palace},
            {"role", palace.role},
            {"reason", palace.reason}});
    }

    auto zuo_gong = nlohmann::ordered_json::array();
    for (const auto& work : blind.work_chains)
    {
        auto basis = nlohmann::ordered_json::array();
        for (const auto& item : work.ming_li_basis)
            basis.push_back({{"rule", item.rule},
                {"subject", item.subject},
                {"relation", item.relation},
                {"points", item.points},
                {"reason", item.reason}});
        zuo_gong.push_back({{"source", work.source},
            {"target", work.target},
            {"direction", work.direction},
            {"relation", work.relation},
            {"result", work.result},
            {"sourcePosition", work.source_position},
            {"targetPosition", work.target_position},
            {"sourceTenGod", work.source_ten_god},
            {"targetTenGod", work.target_ten_god},
            {"effectivePower", work.effective_power},
            {"effective", work.effective},
            {"mingLiBasis", std::move(basis)}});
    }

    auto event_impacts = nlohmann::ordered_json::array();
    for (const auto& event : blind.event_impacts)
        event_impacts.push_back({{"type", event.type},
            {"pressure", event.pressure},
            {"triggers", event.triggers},
            {"mingLiBasis", event.ming_li_basis}});

    auto mu_ku = nlohmann::ordered_json::array();
    for (const auto& tomb : blind.tombs)
        mu_ku.push_back({{"tombBranch", tomb.tomb_branch},
            {"storedStem", tomb.stored_stem},
            {"tombPosition", tomb.tomb_position},
            {"storedStemPosition", tomb.stored_stem_position},
            {"relation", tomb.relation},
            {"state", tomb.state},
            {"opened", tomb.opened},
            {"mingLiBasis", tomb.ming_li_basis}});

    return {{"ruleSet", blind.rule_set},
        {"palaces", std::move(palaces)},
        {"resources", blind.body},
        {"targets", blind.targets},
        {"structures", blind.structures},
        {"symbols", blind.symbols},
        {"gongShen", blind.useful_gods},
        {"feiShen", blind.waste_gods},
        {"zuoGong", std::move(zuo_gong)},
        {"eventImpacts", std::move(event_impacts)},
        {"muKu", std::move(mu_ku)},
        {"transitImpacts", blind.transit_impacts},
        {"warnings", blind.warnings}};
}

/// @brief 生成盘面结构化 JSON（camelCase 提供方契约）。
///
/// 数据全部来自上游结构体与方法：四柱/旬空取 BaZiResult.ba_zi，十神与藏干取
/// ZhouYi.GanZhi，纳音取 BaZiBase::calculate_pillar_nayin，地势取
/// GanZhi::get_shi_er_chang_sheng（口径：**本柱天干在本柱地支**的十二长生，与
/// docs/ziping/example_ba_zi_output.txt 一致，不是日主对各地支），起运与大运取
/// DaYunSystem，人元司令取 AnalysisResult.birth_context，关系取
/// AnalysisResult.relations/stem_relations，五行占比取 element_balance，
/// 盲派结构取 AnalysisResult.blind_analysis。适配层只做投影与展示分组。
auto bazi_chart_projection_json(const cnetnexus::chart::bazi::birth_input& input,
    const ZhouYi::BaZi::BaZiResult& result, const ZhouYi::BaZiAnalysis::AnalysisResult& analysis)
    -> std::string
{
    const std::array<Pillar, 4> pillars{
        result.ba_zi.year, result.ba_zi.month, result.ba_zi.day, result.ba_zi.hour};
    const TianGan day_gan = result.ba_zi.day.gan;
    const auto kong_wang = ZhouYi::GanZhi::get_kong_wang(day_gan, result.ba_zi.day.zhi);

    // 盲派宫位主宾：只有盲派分析产出 palaces，子平留 null（上游没有该口径，不代填）。
    std::array<std::string, 4> palace_roles{};
    if (analysis.blind_analysis)
        for (const auto& palace : analysis.blind_analysis->palaces)
            if (palace.position >= 0 && palace.position < 4)
                palace_roles[static_cast<std::size_t>(palace.position)] = palace.role;

    nlohmann::ordered_json pillars_detail = nlohmann::ordered_json::object();
    for (std::size_t index = 0; index < pillars.size(); ++index)
    {
        const auto& pillar = pillars[index];
        // 日柱天干是命主立极点，对自身不成十神，按前端设计 §5.1 标「日主」。
        nlohmann::ordered_json ten_god = nlohmann::ordered_json::object();
        if (index == 2)
            ten_god = {{"gan", "日主"}, {"ganTone", "ri_zhu"}};
        else
        {
            const auto gan_shi_shen = ZhouYi::GanZhi::get_shi_shen(day_gan, pillar.gan);
            ten_god = {{"gan", std::string{ZhouYi::GanZhi::shi_shen_to_zh(gan_shi_shen)}},
                {"ganTone", std::string{bazi_shi_shen_tone(gan_shi_shen)}}};
        }
        // 地支十神取藏干主气，与上游 DaYun / LiuNian 构造同法。
        const auto zhi_shi_shen = ZhouYi::GanZhi::get_shi_shen(day_gan, ZhouYi::GanZhi::get_cang_gan(pillar.zhi)[0]);
        ten_god["zhi"] = std::string{ZhouYi::GanZhi::shi_shen_to_zh(zhi_shi_shen)};
        ten_god["zhiTone"] = std::string{bazi_shi_shen_tone(zhi_shi_shen)};

        pillars_detail[std::string{k_pillar_keys[index]}] = {
            {"gan", std::string{ZhouYi::GanZhi::Mapper::to_zh(pillar.gan)}},
            {"zhi", std::string{ZhouYi::GanZhi::Mapper::to_zh(pillar.zhi)}},
            {"element", bazi_element_json(pillar)},
            {"tenGod", std::move(ten_god)},
            {"hiddenStems", bazi_hidden_stems_json(day_gan, pillar.zhi)},
            {"naYin", ZhouYi::BaZiBase::calculate_pillar_nayin(pillar).name},
            {"diShi", std::string{ZhouYi::GanZhi::ShiErChangShengMapper::to_zh(ZhouYi::GanZhi::get_shi_er_chang_sheng(pillar.gan, pillar.zhi))}},
            {"isEmpty", pillar.zhi == kong_wang[0] || pillar.zhi == kong_wang[1]},
            {"palaceRole", palace_roles[index].empty() ? nlohmann::ordered_json(nullptr) : nlohmann::ordered_json(palace_roles[index])}};
    }

    nlohmann::ordered_json day_master = {
        {"gan", std::string{ZhouYi::GanZhi::Mapper::to_zh(day_gan)}},
        {"element", std::string{bazi_element_key(ZhouYi::GanZhi::get_wu_xing(day_gan))}},
        {"yinYang", ZhouYi::GanZhi::get_yin_yang(day_gan) == YinYang::Yang ? "yang" : "yin"}};

    // 人元司令：上游按节后日数与分日表判定；无精确时间或判不出时给 null。
    nlohmann::ordered_json meta = nlohmann::ordered_json::object();
    meta["siLing"] = analysis.birth_context.human_command
        ? nlohmann::ordered_json(std::string{ZhouYi::GanZhi::Mapper::to_zh(*analysis.birth_context.human_command)})
        : nlohmann::ordered_json(nullptr);

    const auto& da_yun_list = result.da_yun_system.get_da_yun_list();
    nlohmann::ordered_json qi_yun = {{"age", result.da_yun_system.get_qi_yun_age()},
        {"forward", result.da_yun_system.is_shun_pai()},
        {"year", da_yun_list.empty() ? 0 : da_yun_list.front().start_year}};

    const auto da_yun_json = [](const ZhouYi::BaZi::DaYun& step)
    {
        return nlohmann::ordered_json{{"ganZhi", step.pillar.to_string()},
            {"gan", step.pillar.stem()},
            {"zhi", step.pillar.branch()},
            {"element", bazi_element_json(step.pillar)},
            {"startAge", step.start_age},
            {"endAge", step.end_age},
            {"startYear", step.start_year},
            {"endYear", step.end_year},
            {"ganShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(step.gan_shi_shen)}},
            {"zhiShiShen", std::string{ZhouYi::GanZhi::shi_shen_to_zh(step.zhi_shi_shen)}}};
    };
    nlohmann::ordered_json da_yun = nlohmann::ordered_json::array();
    for (const auto& step : da_yun_list)
        da_yun.push_back(da_yun_json(step));

    nlohmann::ordered_json projection;
    projection["method"] = analysis.method == ZhouYi::BaZiAnalysis::AnalysisMethod::BlindSchool ? "mangpai" : "ziping";
    projection["lunar"] = bazi_lunar_json(input);
    projection["dayMaster"] = std::move(day_master);
    projection["pillarsDetail"] = std::move(pillars_detail);
    projection["meta"] = std::move(meta);
    projection["qiYun"] = std::move(qi_yun);
    projection["daYun"] = std::move(da_yun);
    if (analysis.blind_analysis)
        projection["mangPai"] = bazi_mang_pai_json(*analysis.blind_analysis);

    // 五行占比与合冲刑害只由子平分析产出；盲派不计算这两项，直接不输出该键。
    if (analysis.method != ZhouYi::BaZiAnalysis::AnalysisMethod::BlindSchool)
    {
        nlohmann::ordered_json wu_xing_ratio = nlohmann::ordered_json::object();
        for (const auto& stat : analysis.element_balance)
            wu_xing_ratio[std::string{bazi_element_key(stat.element)}] = stat.percent;
        projection["wuXingRatio"] = std::move(wu_xing_ratio);

        // 关系：地支与天干关系都投影，kind 保留上游 ZhMap 的专业中文名，
        // 柱位由上游给出的 0～3 位置映射成柱键，其余参与支用 members 原样给出。
        const auto positions_of = [](int first, int second)
        {
            auto positions = nlohmann::ordered_json::array();
            const auto first_index = static_cast<std::size_t>(std::clamp(first, 0, 3));
            positions.push_back(std::string{k_pillar_keys[first_index]});
            const auto second_index = static_cast<std::size_t>(std::clamp(second, 0, 3));
            if (second_index != first_index)
                positions.push_back(std::string{k_pillar_keys[second_index]});
            return positions;
        };
        const auto branch_names = [](const std::vector<DiZhi>& members)
        {
            auto names = nlohmann::ordered_json::array();
            for (const auto branch : members)
                names.push_back(std::string{ZhouYi::GanZhi::Mapper::to_zh(branch)});
            return names;
        };
        nlohmann::ordered_json relations = nlohmann::ordered_json::array();
        for (const auto& relation : analysis.relations)
            relations.push_back({{"kind", std::string{ZhouYi::Mapper::to_zh(relation.type)}},
                {"positions", positions_of(relation.first_position, relation.second_position)},
                {"members", branch_names(relation.members)},
                {"virtualBranch", relation.virtual_branch ? nlohmann::ordered_json(std::string{ZhouYi::GanZhi::Mapper::to_zh(*relation.virtual_branch)}) : nlohmann::ordered_json(nullptr)},
                {"detail", relation.impact},
                {"effective", relation.effective}});
        for (const auto& relation : analysis.stem_relations)
            relations.push_back({{"kind", std::string{ZhouYi::Mapper::to_zh(relation.type)}},
                {"positions", positions_of(relation.first_position, relation.second_position)},
                {"members", nlohmann::ordered_json::array()},
                {"virtualBranch", nullptr},
                {"detail", relation.transform},
                {"effective", relation.effective}});
        projection["relations"] = std::move(relations);
    }
    return projection.dump();
}

/// @brief 按输入历法排盘，供排盘与流年两条链路共用。
auto bazi_pai_pan_result(const cnetnexus::chart::bazi::birth_input& input) -> ZhouYi::BaZi::BaZiResult
{
    return input.calendar == cnetnexus::chart::bazi::calendar_kind::lunar
        ? ZhouYi::BaZiController::pai_pan_lunar(input.year, input.month, input.day, input.hour, input.minute,
              input.is_male)
        : ZhouYi::BaZiController::pai_pan_solar(input.year, input.month, input.day, input.hour, input.minute,
              input.is_male);
}

/// @brief 把请求里的流派翻成上游分析入口枚举。
auto bazi_analysis_request(const cnetnexus::chart::bazi::birth_input& input) -> ZhouYi::BaZiAnalysis::AnalysisRequest
{
    ZhouYi::BaZiAnalysis::AnalysisRequest request;
    request.method = input.method == cnetnexus::chart::bazi::analysis_method::mangpai
        ? ZhouYi::BaZiAnalysis::AnalysisMethod::BlindSchool
        : ZhouYi::BaZiAnalysis::AnalysisMethod::Ziping;
    return request;
}

} // namespace cnetnexus::chart
