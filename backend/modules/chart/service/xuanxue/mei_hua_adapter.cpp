module cnetnexus.chart.service.xuanxue.mei_hua_adapter;

import std;
import magic_enum;
import nlohmann.json;
import ZhouYi.GanZhi;
import ZhouYi.MeiHua;
import ZhouYi.MeiHua.Analysis.Contract;
import ZhouYi.MeiHua.Analysis.Presenter;
import ZhouYi.MeiHua.Controller;
import ZhouYi.ZhMapper;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.framework.error;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

namespace {

    using ZhouYi::MeiHua::HexagramSnapshot;
    using ZhouYi::MeiHua::MeiHuaPan;
    using ZhouYi::MeiHua::Trigram;
    using ZhouYi::MeiHuaAnalysis::AnalysisResult;
    using ZhouYi::MeiHuaAnalysis::GuaStage;
    using ZhouYi::MeiHuaAnalysis::JudgmentComponent;
    using ZhouYi::MeiHuaAnalysis::MeiHuaBasis;
    using ZhouYi::MeiHuaAnalysis::OmenReading;
    using ZhouYi::MeiHuaAnalysis::PartyBalance;
    using ZhouYi::MeiHuaAnalysis::PartyMember;
    using ZhouYi::MeiHuaAnalysis::TimingCandidate;
    using ZhouYi::MeiHuaAnalysis::TrigramReading;
    using ZhouYi::MeiHuaAnalysis::TrigramRole;

    /**
     * @brief 上游枚举名转前端契约的 lowerCamel 取值（如 YongShengTi -> yongShengTi）。
     *
     * 只做首字母降格；中文标签一律另给 *Label 字段，由上游 ZhouYi.Mapper 的 ZhMap 提供。
     */
    template <typename enum_type>
    auto enum_code(enum_type value) -> std::string
    {
        std::string name{magic_enum::enum_name(value)};
        if (!name.empty())
            name.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(name.front())));
        return name;
    }

    /**
     * @brief 五行中文名转前端设计文档 §6.1 的 `ElementTone` 词汇。
     *
     * ElementTone（earth/fire/metal/unknown/water/wood）是前端文档定义的展示词汇，
     * 上游只给五行枚举与其中文名，此处按文档口径转换，不做任何体用生克推算。
     */
    auto element_tone(std::string_view element_zh) -> std::string
    {
        if (element_zh == "木")
            return "wood";
        if (element_zh == "火")
            return "fire";
        if (element_zh == "土")
            return "earth";
        if (element_zh == "金")
            return "metal";
        if (element_zh == "水")
            return "water";
        return "unknown";
    }

    /**
     * @brief 经卦职责转前端契约取值。
     *
     * 前端文档 §6.1 的 TrigramRole 把「本卦之用」记作 `benYong`，而上游枚举名为
     * `TrigramRole::YongGua`；此处按文档口径输出，差异已在交付报告中列出。
     */
    auto role_code(TrigramRole role) -> std::string
    {
        if (role == TrigramRole::YongGua)
            return "benYong";
        return enum_code(role);
    }

    /// @brief 经卦是否为动爻所在：动位之卦（本卦之用 / 变卦之用）才算。
    auto role_is_moving(TrigramRole role) -> bool
    {
        return role == TrigramRole::YongGua || role == TrigramRole::BianYong;
    }

    /// @brief 空串按前端契约的 `string|null` 口径输出 null，不落空占位。
    auto text_or_null(const std::string& value) -> nlohmann::ordered_json
    {
        return value.empty() ? nlohmann::ordered_json(nullptr) : nlohmann::ordered_json(value);
    }

    /// @brief 经卦引用投影：卦名、卦符、先天卦数、五行与角色徽标；数据取自上游 trigram_info。
    auto trigram_ref_json(const Trigram trigram, const std::optional<TrigramRole> role, const bool moving)
        -> nlohmann::ordered_json
    {
        const auto& info = ZhouYi::MeiHua::trigram_info(trigram);
        nlohmann::ordered_json item;
        item["name"] = info.name;
        item["symbol"] = text_or_null(info.symbol);
        item["number"] = info.number;
        item["element"] = element_tone(ZhouYi::GanZhi::Mapper::to_zh(info.element));
        item["role"] = role ? nlohmann::ordered_json(role_code(*role)) : nlohmann::ordered_json(nullptr);
        item["isMoving"] = moving;
        return item;
    }

    /// @brief 本卦/变卦快照投影：卦名、卦义、卦码与上下经卦角色。
    ///  上下经卦的角色由上游 TiYong 的体用分判与动爻位置决定，适配层不重判体用。
    auto gua_json(const HexagramSnapshot& gua, const GuaStage stage, const bool moving_in_lower,
        const bool changed_stage) -> nlohmann::ordered_json
    {
        const auto yong_role = changed_stage ? TrigramRole::BianYong : TrigramRole::YongGua;
        nlohmann::ordered_json item;
        item["stage"] = enum_code(stage);
        item["stageLabel"] = std::string{ZhouYi::Mapper::to_zh(stage)};
        item["name"] = gua.name;
        item["meaning"] = text_or_null(gua.meaning);
        item["code"] = gua.code;
        item["upper"] = trigram_ref_json(gua.upper, moving_in_lower ? TrigramRole::TiGenBen : yong_role, !moving_in_lower);
        item["lower"] = trigram_ref_json(gua.lower, moving_in_lower ? yong_role : TrigramRole::TiGenBen, moving_in_lower);
        return item;
    }

    /// @brief 互卦快照投影；上下互卦都不含动爻。
    auto hu_gua_json(const HexagramSnapshot& gua) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["stage"] = enum_code(GuaStage::Hu);
        item["stageLabel"] = std::string{ZhouYi::Mapper::to_zh(GuaStage::Hu)};
        item["name"] = gua.name;
        item["meaning"] = text_or_null(gua.meaning);
        item["code"] = gua.code;
        item["upper"] = trigram_ref_json(gua.upper, TrigramRole::HuShang, false);
        item["lower"] = trigram_ref_json(gua.lower, TrigramRole::HuXia, false);
        return item;
    }

    /// @brief 互变推演条投影。
    auto reading_json(const TrigramReading& reading) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["stage"] = enum_code(reading.stage);
        item["role"] = role_code(reading.role);
        item["roleLabel"] = std::string{ZhouYi::Mapper::to_zh(reading.role)};
        item["trigram"] = trigram_ref_json(reading.trigram, reading.role, role_is_moving(reading.role));
        item["relation"] = enum_code(reading.relation);
        item["relationLabel"] = std::string{ZhouYi::Mapper::to_zh(reading.relation)};
        item["seasonalState"] = enum_code(reading.seasonal_state);
        item["seasonalLabel"] = std::string{ZhouYi::Mapper::to_zh(reading.seasonal_state)};
        item["interpretation"] = reading.interpretation;
        return item;
    }

    /// @brief 体党/用党成员投影。
    auto party_member_json(const PartyMember& member) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["stage"] = enum_code(member.stage);
        item["role"] = role_code(member.role);
        item["roleLabel"] = std::string{ZhouYi::Mapper::to_zh(member.role)};
        item["trigram"] = trigram_ref_json(member.trigram, member.role, role_is_moving(member.role));
        item["side"] = enum_code(member.side);
        item["sideLabel"] = std::string{ZhouYi::Mapper::to_zh(member.side)};
        item["relation"] = enum_code(member.relation);
        item["relationLabel"] = std::string{ZhouYi::Mapper::to_zh(member.relation)};
        item["seasonalState"] = enum_code(member.strength);
        item["seasonalLabel"] = std::string{ZhouYi::Mapper::to_zh(member.strength)};
        item["effectiveForce"] = member.effective_force;
        item["basis"] = member.basis;
        return item;
    }

    /// @brief 体党用党汇总投影。
    auto party_balance_json(const PartyBalance& balance) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json members = nlohmann::ordered_json::array();
        for (const auto& member : balance.members)
            members.push_back(party_member_json(member));

        nlohmann::ordered_json item;
        item["members"] = std::move(members);
        item["tiForce"] = balance.ti_force;
        item["yongForce"] = balance.yong_force;
        item["dominant"] = enum_code(balance.dominant);
        item["dominantLabel"] = std::string{ZhouYi::Mapper::to_zh(balance.dominant)};
        item["interpretation"] = text_or_null(balance.interpretation);
        return item;
    }

    /// @brief 外应参断条投影；外应卦象没有体用职责，role 给 null。
    auto omen_json(const OmenReading& omen) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["kind"] = enum_code(omen.kind);
        item["kindLabel"] = std::string{ZhouYi::Mapper::to_zh(omen.kind)};
        item["observation"] = omen.observation;
        item["trigram"] =
            omen.trigram ? trigram_ref_json(*omen.trigram, std::nullopt, false) : nlohmann::ordered_json(nullptr);
        item["relation"] = omen.relation ? nlohmann::ordered_json(enum_code(*omen.relation))
                                         : nlohmann::ordered_json(nullptr);
        item["relationLabel"] = omen.relation ? nlohmann::ordered_json(std::string{ZhouYi::Mapper::to_zh(*omen.relation)})
                                              : nlohmann::ordered_json(nullptr);
        item["number"] = omen.number ? nlohmann::ordered_json(*omen.number) : nlohmann::ordered_json(nullptr);
        item["favorable"] = omen.favorable;
        item["interpretation"] = omen.interpretation;
        return item;
    }

    /// @brief 占断依据条投影。
    auto basis_json(const MeiHuaBasis& basis) -> nlohmann::ordered_json
    {
        return nlohmann::ordered_json{{"rule", enum_code(basis.rule)},
            {"ruleLabel", std::string{ZhouYi::Mapper::to_zh(basis.rule)}},
            {"detail", basis.detail},
            {"stage", enum_code(basis.stage)},
            {"disposition", enum_code(basis.disposition)}};
    }

    /// @brief 卦势判定分项投影；四个数值全部由上游给出，适配层不重算。
    auto judgment_component_json(const JudgmentComponent& component) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["stage"] = enum_code(component.stage);
        item["role"] = role_code(component.role);
        item["roleLabel"] = std::string{ZhouYi::Mapper::to_zh(component.role)};
        item["relation"] = enum_code(component.relation);
        item["relationLabel"] = std::string{ZhouYi::Mapper::to_zh(component.relation)};
        item["seasonalState"] = enum_code(component.strength);
        item["seasonalLabel"] = std::string{ZhouYi::Mapper::to_zh(component.strength)};
        item["relationValue"] = component.relation_value;
        item["seasonalFactor"] = component.seasonal_factor;
        item["stageFactor"] = component.stage_factor;
        item["contribution"] = component.contribution;
        item["basis"] = component.basis;
        return item;
    }

    /// @brief 应期候选投影。
    auto timing_json(const TimingCandidate& candidate) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["priority"] = candidate.priority;
        item["kind"] = enum_code(candidate.kind);
        item["kindLabel"] = std::string{ZhouYi::Mapper::to_zh(candidate.kind)};
        item["number"] = candidate.number;
        item["unit"] = enum_code(candidate.unit);
        item["unitLabel"] = std::string{ZhouYi::Mapper::to_zh(candidate.unit)};
        item["direction"] = text_or_null(candidate.direction);
        item["trigger"] = candidate.trigger;
        item["basis"] = candidate.basis;
        return item;
    }

    /**
     * @brief 排盘与占断的结构化投影（前端设计文档 §6.1 / §8 的字段口径）。
     *
     * 字段值全部来自上游 analyze_by_time 返回的 MeiHuaPan 与 AnalysisResult，
     * 适配层只做枚举名转换与中文标签投影，不重算起卦、互变、体用或卦力。
     */
    auto chart_json(const MeiHuaPan& pan, const AnalysisResult& analysis) -> nlohmann::ordered_json
    {
        const bool moving_in_lower = pan.ti_yong.moving_line_in_lower;

        nlohmann::ordered_json chart;
        chart["schemaVersion"] = analysis.schema_version;
        chart["method"] = enum_code(pan.method);
        chart["methodLabel"] = std::string{ZhouYi::Mapper::to_zh(pan.method)};
        chart["sourceNumbers"] = pan.source_numbers;
        chart["upperTotal"] = pan.upper_total;
        chart["lowerTotal"] = pan.lower_total;
        chart["movingTotal"] = pan.moving_total;
        chart["movingLine"] = pan.moving_line;
        chart["civilTime"] = text_or_null(pan.civil_time);
        chart["castingTime"] = text_or_null(pan.casting_time);
        chart["lunarDate"] = text_or_null(pan.lunar_date);
        chart["lunarMonth"] = pan.lunar_month;
        chart["lunarLeapMonth"] = pan.lunar_leap_month;
        chart["yearBranchNumber"] = pan.year_branch_number;
        chart["hourBranchNumber"] = pan.hour_branch_number;
        chart["trueSolarTimeApplied"] = pan.true_solar_time_applied;
        chart["timeOffsetMinutes"] = pan.time_offset_minutes;

        chart["benGua"] = gua_json(pan.ben_gua, GuaStage::Ben, moving_in_lower, false);
        chart["huGua"] = hu_gua_json(pan.hu_gua);
        chart["bianGua"] = gua_json(pan.bian_gua, GuaStage::Bian, moving_in_lower, true);

        chart["tiYong"] = nlohmann::ordered_json{
            {"ti", trigram_ref_json(pan.ti_yong.ti, TrigramRole::TiGenBen, !moving_in_lower)},
            {"yong", trigram_ref_json(pan.ti_yong.yong, TrigramRole::YongGua, true)},
            {"movingLineInLower", moving_in_lower},
            {"basis", text_or_null(pan.ti_yong.basis)}};

        chart["benYong"] = reading_json(analysis.ben_yong);
        chart["huInfluences"] = nlohmann::ordered_json::array();
        for (const auto& reading : analysis.hu_influences)
            chart["huInfluences"].push_back(reading_json(reading));
        chart["bianInfluence"] = reading_json(analysis.bian_influence);
        chart["partyBalance"] = party_balance_json(analysis.party_balance);

        chart["movingLineReading"] = nlohmann::ordered_json{{"position", analysis.moving_line.position},
            {"place", text_or_null(analysis.moving_line.place)},
            {"domain", text_or_null(analysis.moving_line.domain)},
            {"interpretation", analysis.moving_line.interpretation}};
        chart["images"] = analysis.images;
        chart["questionReading"] = nlohmann::ordered_json{{"focus", analysis.question_reading.focus},
            {"favorableSigns", analysis.question_reading.favorable_signs},
            {"obstructiveSigns", analysis.question_reading.obstructive_signs},
            {"interpretation", analysis.question_reading.interpretation}};

        chart["omenReadings"] = nlohmann::ordered_json::array();
        for (const auto& omen : analysis.omen_readings)
            chart["omenReadings"].push_back(omen_json(omen));

        chart["basis"] = nlohmann::ordered_json::array();
        for (const auto& basis : analysis.mei_hua_basis)
            chart["basis"].push_back(basis_json(basis));

        chart["judgment"] = nlohmann::ordered_json{
            {"value", enum_code(analysis.judgment)}, {"label", std::string{ZhouYi::Mapper::to_zh(analysis.judgment)}}};

        nlohmann::ordered_json components = nlohmann::ordered_json::array();
        for (const auto& component : analysis.judgment_detail.components)
            components.push_back(judgment_component_json(component));
        chart["judgmentDetail"] = nlohmann::ordered_json{{"ruleVersion", analysis.judgment_detail.rule_version},
            {"formula", analysis.judgment_detail.formula},
            {"components", std::move(components)},
            {"partyAdjustment", analysis.judgment_detail.party_adjustment},
            {"total", analysis.judgment_detail.total},
            {"interpretation", text_or_null(analysis.judgment_detail.interpretation)}};

        chart["timing"] = nlohmann::ordered_json::array();
        for (const auto& candidate : analysis.timing)
            chart["timing"].push_back(timing_json(candidate));

        chart["conclusion"] = text_or_null(analysis.conclusion);
        chart["reviewPoints"] = analysis.review_points;
        return chart;
    }

} // namespace

auto mei_hua_adapter::cast(const cnetnexus::chart::mei_hua::cast_input& input)
    -> std::expected<cnetnexus::chart::mei_hua::cast_result, cnetnexus::framework::error::error_code>
{
    // 占类交给上游 ZhMap 解析，本仓库不维护第二份对照表。
    const auto question_kind = ZhouYi::Mapper::from_zh<ZhouYi::MeiHuaAnalysis::QuestionKind>(input.kind);
    if (!question_kind)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    // 上游梅花链路不用 std::expected：无效时刻抛
    // std::invalid_argument，和数溢出抛
    // std::overflow_error，统一在此边界翻译成已注册错误码。
    try
    {
        const ZhouYi::MeiHuaController::TimeCastingRequest time{.calendar = input.calendar ==
                    cnetnexus::chart::mei_hua::calendar_kind::lunar
                ? ZhouYi::MeiHuaController::CalendarKind::Lunar
                : ZhouYi::MeiHuaController::CalendarKind::Solar,
            .year = input.year,
            .month = input.month,
            .day = input.day,
            .hour = input.hour,
            .minute = input.minute,
            .location = std::nullopt};

        ZhouYi::MeiHuaAnalysis::AnalysisRequest request;
        request.question_kind = *question_kind;
        request.question = input.question;

        const auto result = ZhouYi::MeiHuaController::analyze_by_time(time, request);

        std::ostringstream report;
        ZhouYi::MeiHuaAnalysis::write_zh(report, result.pan, result.analysis);

        cnetnexus::chart::mei_hua::cast_result output{};
        output.input = input;
        output.analysis_zh = report.str();
        // 盘面同源两路：pan 保留上游 to_zh_json 的中文 JSON 原样（与 da_liu_ren/qi_men 的
        // pan 一致），chart 按前端设计文档 §8 投影成 camelCase 结构化字段。
        auto pan_projection = parse_provider_object(
            ZhouYi::MeiHuaAnalysis::to_zh_json(result.pan, result.analysis).dump());
        auto chart_projection =
            parse_provider_object(chart_json(result.pan, result.analysis).dump());
        if (!pan_projection || !chart_projection || output.analysis_zh.empty())
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        output.pan_projection = std::move(*pan_projection);
        output.chart_projection = std::move(*chart_projection);
        return output;
    }
    catch (const std::invalid_argument&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }
    catch (const std::overflow_error&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }
    catch (const std::exception&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
    }
}

} // namespace cnetnexus::chart
