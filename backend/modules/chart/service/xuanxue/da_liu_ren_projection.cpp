module cnetnexus.chart.service.xuanxue.da_liu_ren_projection;

import std;
import magic_enum;
import nlohmann.json;
import ZhouYi.DaLiuRen;
import ZhouYi.DaLiuRen.Controller;
import ZhouYi.DaLiuRen.ShenSha;
import ZhouYi.DaLiuRen.XingNian;
import ZhouYi.DaLiuRenAnalysis;
import ZhouYi.DaLiuRenAnalysis.Presenter;
import ZhouYi.GanZhi;
import ZhouYi.ZhMapper;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.framework.error;


namespace cnetnexus::chart {

using ZhouYi::DaLiuRen::DaLiuRenResult;
using ZhouYi::DaLiuRen::TianJiang;
using ZhouYi::DaLiuRenAnalysis::AnalysisResult;
using ZhouYi::DaLiuRenAnalysis::BaMenAnalysis;
using ZhouYi::DaLiuRenAnalysis::BranchRelationAnalysis;
using ZhouYi::DaLiuRenAnalysis::BranchRelationKind;
using ZhouYi::DaLiuRenAnalysis::KeChuanBasis;
using ZhouYi::DaLiuRenAnalysis::LeiShenSelection;
using ZhouYi::DaLiuRenAnalysis::LessonAnalysis;
using ZhouYi::DaLiuRenAnalysis::MissingBranchContext;
using ZhouYi::DaLiuRenAnalysis::PalaceAnalysis;
using ZhouYi::DaLiuRenAnalysis::PatternAnalysis;
using ZhouYi::DaLiuRenAnalysis::PersonalMarkerAnalysis;
using ZhouYi::DaLiuRenAnalysis::TimingCandidate;
using ZhouYi::DaLiuRenAnalysis::TransmissionAnalysis;
using ZhouYi::GanZhi::DiZhi;
using ZhouYi::GanZhi::LiuQin;
using ZhouYi::GanZhi::TianGan;

/**
 * @brief 上游枚举名转前端契约的 lowerCamel 取值（如 XuanWu -> xuanWu）。
 *
 * 只做首字母降格，不维护第二份枚举名对照表；中文一律另给 *Label 字段，
 * 由上游 ZhouYi.Mapper 的 ZhMap（或上游 GanZhi 的映射函数）提供。
 */
template <typename enum_type>
auto dlr_enum_code(enum_type value) -> std::string
{
    std::string name{magic_enum::enum_name(value)};
    if (!name.empty())
        name.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(name.front())));
    return name;
}

/// @brief 上游枚举值的中文标签；一律取 ZhouYi.Mapper 的 ZhMap。
template <typename enum_type>
auto dlr_enum_label(enum_type value) -> std::string
{
    return std::string{ZhouYi::Mapper::to_zh(value)};
}

/// @brief 地支中文；取上游 ZhouYi.GanZhi::Mapper。
auto dlr_zhi_zh(DiZhi value) -> std::string
{
    return std::string{ZhouYi::GanZhi::Mapper::to_zh(value)};
}

/// @brief 天干中文；取上游 ZhouYi.GanZhi::Mapper。
auto dlr_gan_zh(TianGan value) -> std::string
{
    return std::string{ZhouYi::GanZhi::Mapper::to_zh(value)};
}

/// @brief 六亲中文；取上游 liu_qin_to_zh（LiuQin 没有 ZhMap 特化）。
auto dlr_liu_qin_zh(LiuQin value) -> std::string
{
    return std::string{ZhouYi::GanZhi::liu_qin_to_zh(value)};
}

/// @brief 枚举值成对输出：value 为上游枚举名首字母降格，label 为上游中文标签。
template <typename enum_type>
auto dlr_enum_item(enum_type value) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"value", dlr_enum_code(value)}, {"label", dlr_enum_label(value)}};
}

/// @brief 枚举数组成对输出（如所缺支出现在哪些课位、哪些传位）。
template <typename enum_type>
auto dlr_enum_items(const std::vector<enum_type>& values) -> nlohmann::ordered_json
{
    nlohmann::ordered_json array = nlohmann::ordered_json::array();
    for (const auto value : values)
        array.push_back(dlr_enum_item(value));
    return array;
}

/// @brief 逐条依据投影；rule/detail/nature 全部取上游 KeChuanBasis，不重算。
auto dlr_basis_json(const std::vector<KeChuanBasis>& items) -> nlohmann::ordered_json
{
    nlohmann::ordered_json array = nlohmann::ordered_json::array();
    for (const auto& item : items)
        array.push_back(nlohmann::ordered_json{{"rule", item.rule},
            {"detail", item.detail},
            {"nature", dlr_enum_code(item.nature)},
            {"natureLabel", dlr_enum_label(item.nature)}});
    return array;
}

/**
 * @brief 半合所缺支的课盘落处投影（上游 MissingBranchContext）。
 *
 * 上游 presenter 把课位/传位拼成顿号串、把月建日辰占时月将拼成一个字段；
 * 结构化出口按契约原样给出枚举对数组与四个独立布尔，信息量不减少。
 */
auto dlr_missing_context_json(const MissingBranchContext& context) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"branch", dlr_zhi_zh(context.branch)},
        {"heavenLandsOnEarth", dlr_zhi_zh(context.heaven_lands_on_earth)},
        {"heavenOverEarth", dlr_zhi_zh(context.heaven_over_earth)},
        {"lessonUpperPositions", dlr_enum_items(context.lesson_upper_positions)},
        {"lessonLowerPositions", dlr_enum_items(context.lesson_lower_positions)},
        {"transmissionPositions", dlr_enum_items(context.transmission_positions)},
        {"isMonthCommand", context.is_month_command},
        {"isDayBranch", context.is_day_branch},
        {"isHourBranch", context.is_hour_branch},
        {"isMonthGeneral", context.is_month_general},
        {"isVoid", context.is_void},
        {"assessment", context.assessment}};
}

/// @brief 支际关系投影（上游 BranchRelationAnalysis）；所缺支缺失时给 null，不写占位串。
auto dlr_relation_json(const BranchRelationAnalysis& relation) -> nlohmann::ordered_json
{
    nlohmann::ordered_json branches = nlohmann::ordered_json::array();
    for (const auto branch : relation.branches)
        branches.push_back(dlr_zhi_zh(branch));
    return nlohmann::ordered_json{{"kind", dlr_enum_code(relation.kind)},
        {"kindLabel", dlr_enum_label(relation.kind)},
        {"branches", std::move(branches)},
        {"sources", relation.sources},
        {"missingBranch",
            relation.missing_branch ? nlohmann::ordered_json(dlr_zhi_zh(*relation.missing_branch))
                                    : nlohmann::ordered_json(nullptr)},
        {"missingContext", relation.missing_context ? dlr_missing_context_json(*relation.missing_context) : nlohmann::ordered_json(nullptr)},
        {"detail", relation.detail},
        {"nature", dlr_enum_code(relation.nature)},
        {"natureLabel", dlr_enum_label(relation.nature)}};
}

/**
 * @brief 结构化四课投影（前端设计文档 §5.3、§2.6）。
 *
 * 前端此前只能用 排盘.si_ke.first..fourth 的拼接字符串（上游 GanZhiKe::to_string()），
 * 拆不出上神/下神/天将/六亲；这里按上游 LessonAnalysis 逐字段给出语义字段与序号，
 * 列序（设计文档锁定「左起 第四→第一」）由前端按 position/order 决定。
 */
auto dlr_lesson_json(const LessonAnalysis& lesson) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"position", dlr_enum_code(lesson.position)},
        {"positionLabel", dlr_enum_label(lesson.position)},
        {"order", static_cast<int>(lesson.position) + 1},
        {"door", dlr_enum_code(lesson.door)},
        {"doorLabel", dlr_enum_label(lesson.door)},
        {"lower", lesson.lower},
        {"lowerPalace", dlr_zhi_zh(lesson.lower_palace)},
        {"upper", dlr_zhi_zh(lesson.upper)},
        {"relative", dlr_enum_code(lesson.relative)},
        {"relativeLabel", dlr_liu_qin_zh(lesson.relative)},
        {"general", dlr_enum_code(lesson.general)},
        {"generalLabel", dlr_enum_label(lesson.general)},
        {"relation", dlr_enum_code(lesson.relation)},
        {"relationLabel", dlr_enum_label(lesson.relation)},
        {"sendsInitial", lesson.sends_initial},
        {"basis", dlr_basis_json(lesson.ke_chuan_basis)}};
}

/**
 * @brief 结构化三传投影（前端设计文档 §5.4）。
 *
 * 遁干取上游 TransmissionAnalysis::hidden_stem（空亡时为 null，前端整行不渲染）；
 * 依据取上游 ke_chuan_basis。两者都是上游已有结构体，适配层不推遁干、不造依据。
 */
auto dlr_transmission_json(const TransmissionAnalysis& transmission) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"stage", dlr_enum_code(transmission.stage)},
        {"stageLabel", dlr_enum_label(transmission.stage)},
        {"order", static_cast<int>(transmission.stage) + 1},
        {"branch", dlr_zhi_zh(transmission.branch)},
        {"earthPosition", dlr_zhi_zh(transmission.earth_position)},
        {"hiddenStem", transmission.hidden_stem ? nlohmann::ordered_json(dlr_gan_zh(*transmission.hidden_stem)) : nlohmann::ordered_json(nullptr)},
        {"relative", dlr_enum_code(transmission.relative)},
        {"relativeLabel", dlr_liu_qin_zh(transmission.relative)},
        {"general", dlr_enum_code(transmission.general)},
        {"generalLabel", dlr_enum_label(transmission.general)},
        {"seasonalState", transmission.seasonal_state},
        {"isVoid", transmission.is_void},
        {"shenSha", transmission.shen_sha},
        {"basis", dlr_basis_json(transmission.ke_chuan_basis)}};
}

/// @brief 天地盘单宫投影；数组顺序即上游 palaces 顺序（子→亥），落格一律按 earth 汉字。
auto dlr_palace_json(const PalaceAnalysis& palace) -> nlohmann::ordered_json
{
    nlohmann::ordered_json relations = nlohmann::ordered_json::array();
    for (const auto& relation : palace.relations)
        relations.push_back(dlr_relation_json(relation));
    return nlohmann::ordered_json{{"earth", dlr_zhi_zh(palace.earth)},
        {"heaven", dlr_zhi_zh(palace.heaven)},
        {"general", dlr_enum_code(palace.general)},
        {"generalLabel", dlr_enum_label(palace.general)},
        {"relative", dlr_enum_code(palace.relative)},
        {"relativeLabel", dlr_liu_qin_zh(palace.relative)},
        {"relations", std::move(relations)}};
}

/// @brief 本命/行年临盘投影；两个空亡布尔分列，不合并。
auto dlr_personal_marker_json(const PersonalMarkerAnalysis& marker) -> nlohmann::ordered_json
{
    nlohmann::ordered_json relations = nlohmann::ordered_json::array();
    for (const auto& relation : marker.transmission_relations)
        relations.push_back(dlr_relation_json(relation));
    return nlohmann::ordered_json{{"kind", dlr_enum_code(marker.kind)},
        {"kindLabel", dlr_enum_label(marker.kind)},
        {"branch", dlr_zhi_zh(marker.branch)},
        {"upper", dlr_zhi_zh(marker.upper)},
        {"general", dlr_enum_code(marker.general)},
        {"generalLabel", dlr_enum_label(marker.general)},
        {"relative", dlr_enum_code(marker.relative)},
        {"relativeLabel", dlr_liu_qin_zh(marker.relative)},
        {"branchIsVoid", marker.branch_is_void},
        {"upperIsVoid", marker.upper_is_void},
        {"transmissionRelations", std::move(relations)},
        {"basis", dlr_basis_json(marker.ke_chuan_basis)}};
}

/// @brief 八门占断单门投影（上游 BaMenAnalysis）。
auto dlr_eight_door_json(const BaMenAnalysis& door) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"door", dlr_enum_code(door.door)},
        {"doorLabel", dlr_enum_label(door.door)},
        {"subject", door.subject},
        {"conclusion", door.conclusion},
        {"basis", dlr_basis_json(door.ke_chuan_basis)}};
}

/// @brief 类神定位投影；主六亲缺失时给 null，不猜六亲。
auto dlr_lei_shen_json(const LeiShenSelection& selection) -> nlohmann::ordered_json
{
    nlohmann::ordered_json generals = nlohmann::ordered_json::array();
    for (const auto general : selection.supporting_generals)
        generals.push_back(dlr_enum_item(general));
    nlohmann::ordered_json appearances = nlohmann::ordered_json::array();
    for (const auto branch : selection.appearances)
        appearances.push_back(dlr_zhi_zh(branch));
    nlohmann::ordered_json plate_positions = nlohmann::ordered_json::array();
    for (const auto branch : selection.plate_positions)
        plate_positions.push_back(dlr_zhi_zh(branch));
    return nlohmann::ordered_json{{"name", selection.name},
        {"primaryRelative",
            selection.primary_relative ? nlohmann::ordered_json(dlr_enum_code(*selection.primary_relative))
                                       : nlohmann::ordered_json(nullptr)},
        {"primaryRelativeLabel",
            selection.primary_relative ? nlohmann::ordered_json(dlr_liu_qin_zh(*selection.primary_relative))
                                       : nlohmann::ordered_json(nullptr)},
        {"supportingGenerals", std::move(generals)},
        {"appearances", std::move(appearances)},
        {"platePositions", std::move(plate_positions)},
        {"reason", selection.reason}};
}

/// @brief 课体与毕法结构投影（上游 PatternAnalysis）。
auto dlr_pattern_json(const PatternAnalysis& pattern) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{
        {"name", pattern.name}, {"implication", pattern.implication}, {"basis", dlr_basis_json(pattern.ke_chuan_basis)}};
}

/// @brief 应期候选投影（上游 TimingCandidate）。
auto dlr_timing_json(const TimingCandidate& candidate) -> nlohmann::ordered_json
{
    return nlohmann::ordered_json{{"priority", candidate.priority},
        {"trigger", dlr_zhi_zh(candidate.trigger)},
        {"rule", dlr_enum_code(candidate.rule)},
        {"ruleLabel", dlr_enum_label(candidate.rule)},
        {"phase", dlr_enum_code(candidate.phase)},
        {"phaseLabel", dlr_enum_label(candidate.phase)},
        {"pace", dlr_enum_code(candidate.pace)},
        {"paceLabel", dlr_enum_label(candidate.pace)},
        {"timeScope", candidate.time_scope},
        {"condition", candidate.condition},
        {"manifestation", candidate.manifestation},
        {"constraints", candidate.constraints},
        {"priorityReason", candidate.priority_reason},
        {"description", candidate.description},
        {"basis", dlr_basis_json(candidate.ke_chuan_basis)}};
}

/**
 * @brief 「临盘作用」的判定谓词，与上游 presenter 同一口径。
 *
 * 上游 presenter.cpp:83-94 用同一组字面谓词把 cross_layer_relations 分到
 * 四课临盘作用 / 三传临盘作用 / 四课三传合局；这里复刻的是展示层分类，
 * 不是术数规则重算（参与者、关系与作用全部取上游结构体）。
 */
auto dlr_is_landing_relation(const BranchRelationAnalysis& relation) -> bool
{
    if (relation.sources.size() != 2)
        return false;
    return relation.sources[1] == "所临地盘" || relation.sources[1].ends_with("下神所居地盘");
}

/// @brief 是否成局类关系（三合 / 三会 / 半合）；口径同上游 presenter。
auto dlr_is_group_relation(const BranchRelationAnalysis& relation) -> bool
{
    return relation.kind == BranchRelationKind::SanHe || relation.kind == BranchRelationKind::SanHui ||
        relation.kind == BranchRelationKind::BanHe;
}

/**
 * @brief 大六壬盘面与占断的结构化投影（前端设计文档 §5 全部字段的 camelCase 出口）。
 *
 * 字段值全部来自上游 pai_pan_solar 返回的 DaLiuRenResult 与 DaLiuRenAnalysis::analyze
 * 的 AnalysisResult，以及上游 ZhMap / GanZhi 映射；适配层只做重排、枚举名转换与中文
 * 标签投影，不重算天地盘、四课、三传、遁干、课体、类神、应期或任何生克关系。
 * 设计文档 §6 中上游确实没有结构体成员的项（天地盘逐宫旬空位、课局、时间口径）
 * 一律不输出，不用默认值或 analysisZh 文本顶替。
 */
auto dlr_chart_json(const DaLiuRenResult& pan, const AnalysisResult& analysis) -> nlohmann::ordered_json
{
    nlohmann::ordered_json chart;
    // 契约版本置于结构化块首位，与 bazi/liu_yao/mei_hua 接口保持同一键序。
    chart["schemaVersion"] = analysis.schema_version;

    nlohmann::ordered_json pillars;
    pillars["year"] = pan.ba_zi.year.to_string();
    pillars["month"] = pan.ba_zi.month.to_string();
    pillars["day"] = pan.ba_zi.day.to_string();
    pillars["hour"] = pan.ba_zi.hour.to_string();
    chart["castingPillars"] = std::move(pillars);
    chart["hourBranch"] = pan.ba_zi.hour.branch();
    nlohmann::ordered_json xun_kong = nlohmann::ordered_json::array();
    for (const auto* value : {&pan.ba_zi.xun_kong_1, &pan.ba_zi.xun_kong_2})
        if (!value->empty())
            xun_kong.push_back(*value);
    chart["xunKong"] = std::move(xun_kong);
    chart["yueJiang"] = dlr_zhi_zh(pan.yue_jiang);
    chart["guiRen"] = dlr_zhi_zh(pan.gui_ren);
    chart["isDay"] = pan.is_day;
    chart["jiuZongMen"] = dlr_enum_code(analysis.method);
    chart["jiuZongMenLabel"] = dlr_enum_label(analysis.method);
    chart["lessonPatterns"] = analysis.lesson_patterns;
    chart["guaTi"] = pan.gua_ti;

    chart["palaces"] = nlohmann::ordered_json::array();
    for (const auto& palace : analysis.palaces)
        chart["palaces"].push_back(dlr_palace_json(palace));

    chart["lessons"] = nlohmann::ordered_json::array();
    for (const auto& lesson : analysis.lessons)
        chart["lessons"].push_back(dlr_lesson_json(lesson));

    chart["transmissions"] = nlohmann::ordered_json::array();
    for (const auto& transmission : analysis.transmissions)
        chart["transmissions"].push_back(dlr_transmission_json(transmission));

    chart["personalMarkers"] = nlohmann::ordered_json::array();
    for (const auto& marker : analysis.personal_markers)
        chart["personalMarkers"].push_back(dlr_personal_marker_json(marker));

    chart["transmissionRelations"] = nlohmann::ordered_json::array();
    for (const auto& relation : analysis.transmission_relations)
        chart["transmissionRelations"].push_back(dlr_relation_json(relation));

    // 跨层关系一次遍历，同时填三张下游视图（联断 / 四课临盘 / 三传临盘 / 合局），
    // 与上游 presenter.cpp:237-264 同一分类口径。
    chart["crossLayerRelations"] = nlohmann::ordered_json::array();
    chart["lessonPlateRelations"] = nlohmann::ordered_json::array();
    chart["transmissionPlateRelations"] = nlohmann::ordered_json::array();
    for (const auto& relation : analysis.cross_layer_relations)
    {
        const auto item = dlr_relation_json(relation);
        chart["crossLayerRelations"].push_back(item);
        if (!dlr_is_landing_relation(relation))
            continue;
        if (!relation.sources.empty() && relation.sources.front().find("课") != std::string::npos)
            chart["lessonPlateRelations"].push_back(item);
        else
            chart["transmissionPlateRelations"].push_back(item);
    }

    // 四课三传半合与成局：上游 presenter 的中文表把「三传之间」与「跨层」两张表按
    // 三合→三会→半合合并；这里投影同一并集，layer 标明取自哪张表，isComplete 由
    // 上游 missing_branch 是否存在直接得出（不重算成局）。
    chart["groupCompositions"] = nlohmann::ordered_json::array();
    for (const auto kind : {BranchRelationKind::SanHe, BranchRelationKind::SanHui, BranchRelationKind::BanHe})
    {
        const auto same_kind = [kind](const BranchRelationAnalysis& relation)
        {
            return relation.kind == kind;
        };
        for (const auto& relation : analysis.transmission_relations)
        {
            if (same_kind(relation))
            {
                auto item = dlr_relation_json(relation);
                item["layer"] = "transmission";
                item["isComplete"] = !relation.missing_branch.has_value();
                chart["groupCompositions"].push_back(std::move(item));
            }
        }
        for (const auto& relation : analysis.cross_layer_relations)
        {
            if (same_kind(relation))
            {
                auto item = dlr_relation_json(relation);
                item["layer"] = "crossLayer";
                item["isComplete"] = !relation.missing_branch.has_value();
                chart["groupCompositions"].push_back(std::move(item));
            }
        }
    }

    // 半合所缺支查验：与上游 presenter.cpp:557-564 同一去重口径（按所缺支升序、首见为准），
    // 汇总表内容全部取上游 MissingBranchContext。
    std::map<int, std::pair<const MissingBranchContext*, std::vector<std::string>>> missing;
    const auto collect_missing = [&missing](const std::vector<BranchRelationAnalysis>& relations)
    {
        for (const auto& relation : relations)
        {
            if (!relation.missing_context)
                continue;
            auto& entry = missing[static_cast<int>(relation.missing_context->branch)];
            if (entry.first == nullptr)
                entry.first = &*relation.missing_context;
            const auto kind = dlr_enum_code(relation.kind);
            if (std::ranges::find(entry.second, kind) == entry.second.end())
                entry.second.push_back(kind);
        }
    };
    collect_missing(analysis.transmission_relations);
    collect_missing(analysis.cross_layer_relations);
    chart["missingBranches"] = nlohmann::ordered_json::array();
    for (const auto& [_, entry] : missing)
    {
        auto item = dlr_missing_context_json(*entry.first);
        item["kinds"] = entry.second;
        chart["missingBranches"].push_back(std::move(item));
    }

    chart["eightDoors"] = nlohmann::ordered_json::array();
    for (const auto& door : analysis.eight_doors)
        chart["eightDoors"].push_back(dlr_eight_door_json(door));

    chart["leiShen"] = dlr_lei_shen_json(analysis.lei_shen);

    chart["patterns"] = nlohmann::ordered_json::array();
    for (const auto& pattern : analysis.patterns)
        chart["patterns"].push_back(dlr_pattern_json(pattern));

    chart["favorable"] = dlr_basis_json(analysis.favorable);
    chart["unfavorable"] = dlr_basis_json(analysis.unfavorable);

    chart["timing"] = nlohmann::ordered_json::array();
    for (const auto& candidate : analysis.timing)
        chart["timing"].push_back(dlr_timing_json(candidate));

    chart["judgment"] = dlr_enum_code(analysis.judgment);
    chart["judgmentLabel"] = dlr_enum_label(analysis.judgment);
    chart["conclusion"] = analysis.conclusion;
    chart["reviewPoints"] = analysis.review_points;

    // 上游排盘的神煞速查：DaLiuRenResult::to_json() 没有调用 ShenShaResult::to_json()，
    // 这里原样内嵌上游整块中文 JSON（键名为上游中文，不改写）。
    chart["shenSha"] = pan.shen_sha.to_json();
    return chart;
}

} // namespace cnetnexus::chart
