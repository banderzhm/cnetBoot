module cnetnexus.chart.service.xuanxue.qi_men_adapter;

import std;
import magic_enum;
import nlohmann.json;
import ZhouYi.BaZiBase;
import ZhouYi.GanZhi;
import ZhouYi.QiMen;
import ZhouYi.QiMen.Analysis;
import ZhouYi.QiMen.Analysis.Presenter;
import ZhouYi.QiMen.Controller;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.framework.error;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

namespace {

    using ZhouYi::BaZiBase::BaZi;
    using ZhouYi::GanZhi::DiZhi;
    using ZhouYi::GanZhi::TianGan;
    using ZhouYi::QiMen::Gate;
    using ZhouYi::QiMen::Palace;
    using ZhouYi::QiMen::PalaceInfo;
    using ZhouYi::QiMen::PatternFinding;
    using ZhouYi::QiMen::QiMenPan;
    using ZhouYi::QiMen::Spirit;
    using ZhouYi::QiMen::Star;
    using ZhouYi::QiMen::Strength;
    using ZhouYi::QiMenAnalysis::AnalysisResult;
    using ZhouYi::QiMenAnalysis::MainGuestReading;
    using ZhouYi::QiMenAnalysis::Manifestation;
    using ZhouYi::QiMenAnalysis::PalaceReading;
    using ZhouYi::QiMenAnalysis::PanJuBasis;
    using ZhouYi::QiMenAnalysis::TimingDirection;
    using ZhouYi::QiMenAnalysis::YongShenRole;
    using ZhouYi::QiMenAnalysis::YongShenSelection;

    /**
     * @brief 上游枚举名转前端契约的 lowerCamel 取值（如 GongMing -> gongMing）。
     *
     * 只做首字母降格，不维护第二份枚举名对照表。奇门上游没有 ZhMap 特化，
     * 因此凡上游未导出中文取值函数的枚举（Dun / PatternNature / QuestionKind /
     * YongShenRole / PalaceRelation / EffectNature / Judgment）一律只给本码，
     * 其中文仍可从同源的 pan_json 读取，本仓不复制上游 presenter 的私有中文表。
     */
    template <typename enum_type>
    auto enum_code(enum_type value) -> std::string
    {
        std::string name{magic_enum::enum_name(value)};
        if (!name.empty())
            name.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(name.front())));
        return name;
    }

    /// @brief 天干中文（上游 ZhouYi.GanZhi 映射），奇门盘面记号一律用汉字。
    auto gan_label(TianGan value) -> std::string
    {
        return std::string{ZhouYi::GanZhi::Mapper::to_zh(value)};
    }

    /// @brief 地支中文（上游 ZhouYi.GanZhi 映射）。
    auto zhi_label(DiZhi value) -> std::string
    {
        return std::string{ZhouYi::GanZhi::Mapper::to_zh(value)};
    }

    /// @brief 宫位数（1..9，即洛书数）：上游 get_number_from_palace，落宫唯一依据。
    auto palace_number(Palace value) -> int
    {
        return static_cast<int>(ZhouYi::QiMen::get_number_from_palace(value));
    }

    /// @brief 宫名「巽四宫」：上游 palace_name。
    auto palace_label(Palace value) -> std::string
    {
        return std::string{ZhouYi::QiMen::palace_name(value)};
    }

    /// @brief 卦名「巽」：上游 trigram_name（设计文档 §6-12 的 palaces[].trigram）。
    auto trigram_label(Palace value) -> std::string
    {
        return std::string{ZhouYi::QiMen::trigram_name(value)};
    }

    /// @brief 宫位五行「木火土金水」：上游 palace_element + 上游五行中文映射。
    ///  只取上游固定后天五行常量，不做任何生克推演（设计文档 §3.4 的可选增强）。
    auto element_label(Palace value) -> std::string
    {
        return std::string{ZhouYi::GanZhi::Mapper::to_zh(ZhouYi::QiMen::palace_element(value))};
    }

    /// @brief 九星名「天冲」：上游 star_name。
    auto star_label(Star value) -> std::string
    {
        return std::string{ZhouYi::QiMen::star_name(value)};
    }

    /// @brief 八门名「伤门」：上游 gate_name。
    auto gate_label(Gate value) -> std::string
    {
        return std::string{ZhouYi::QiMen::gate_name(value)};
    }

    /// @brief 八神名「六合」：上游 spirit_name。
    auto spirit_label(Spirit value) -> std::string
    {
        return std::string{ZhouYi::QiMen::spirit_name(value)};
    }

    /// @brief 旺衰「旺/相/休/囚/废/死/未判」：上游 strength_name。
    auto strength_label(Strength value) -> std::string
    {
        return std::string{ZhouYi::QiMen::strength_name(value)};
    }

    /// @brief 空串按契约的「string|null」口径输出 null，不落空占位（设计文档 C2）。
    auto text_or_null(const std::string& value) -> nlohmann::ordered_json
    {
        return value.empty() ? nlohmann::ordered_json(nullptr) : nlohmann::ordered_json(value);
    }

    /// @brief 单条已成格局投影；nature 给上游 PatternNature 的 lowerCamel 码。
    auto pattern_json(const PatternFinding& finding) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["ruleId"] = finding.rule_id;
        item["name"] = finding.name;
        item["nature"] = enum_code(finding.nature);
        item["palace"] = palace_number(finding.palace);
        item["palaceName"] = palace_label(finding.palace);
        item["panJuBasis"] = finding.pan_ju_basis;
        return item;
    }

    /// @brief 格局数组投影（本宫格局 QiMenPan::palaces[].patterns 与全局格局 global_patterns 同构）。
    auto patterns_json(const std::vector<PatternFinding>& findings) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& finding : findings)
            array.push_back(pattern_json(finding));
        return array;
    }

    /**
     * @brief 一条可追溯盘局依据投影（上游 PanJuBasis）。
     *
     * 落宫上游为 optional<Palace>：全局断法为空，按 C2 给 null，
     * 不用上游中文 JSON 里的「全局」占位文本顶替（那是展示口径，不是结构化值）。
     */
    auto basis_json(const PanJuBasis& basis) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["rule"] = basis.rule;
        item["detail"] = basis.detail;
        item["nature"] = enum_code(basis.nature);
        item["palace"] = basis.palace ? nlohmann::ordered_json(palace_number(*basis.palace))
                                      : nlohmann::ordered_json(nullptr);
        item["palaceName"] = basis.palace ? nlohmann::ordered_json(palace_label(*basis.palace))
                                          : nlohmann::ordered_json(nullptr);
        return item;
    }

    /// @brief 依据数组投影（生扶救应 / 克制闭塞 / 主客合参 / 全局格局与时势共用）。
    auto bases_json(const std::vector<PanJuBasis>& items) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : items)
            array.push_back(basis_json(item));
        return array;
    }

    /**
     * @brief 单宫完整盘面投影（设计文档 §4.1 QiMenPalaceView + §6 缺口 4/5/6/7/8/12）。
     *
     * 宫名、卦名、九星、八门、八神、旺衰、奇仪全部取上游中文名函数与 GanZhi 映射，
     * 与 pan_json 的 palaces[] 同源项逐字一致；门迫 / 击刑 / 入墓 / 承接中五寄宫
     * 四个布尔与宫位地支取上游 PalaceInfo 成员，不重算任何规则。
     */
    auto palace_json(const PalaceInfo& palace) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["palaceNumber"] = palace_number(palace.palace);
        item["name"] = palace_label(palace.palace);
        item["trigram"] = trigram_label(palace.palace);
        item["element"] = element_label(palace.palace);
        item["star"] = star_label(palace.star);
        item["extraStar"] = palace.extra_star ? nlohmann::ordered_json(star_label(*palace.extra_star))
                                              : nlohmann::ordered_json(nullptr);
        item["gate"] = gate_label(palace.gate);
        item["spirit"] = spirit_label(palace.spirit);
        item["starStrength"] = strength_label(palace.star_strength);
        item["gateStrength"] = strength_label(palace.gate_strength);
        item["heavenStem"] = gan_label(palace.tian_gan);
        item["extraHeavenStem"] = palace.extra_tian_gan ? nlohmann::ordered_json(gan_label(*palace.extra_tian_gan))
                                                        : nlohmann::ordered_json(nullptr);
        item["earthStem"] = gan_label(palace.di_gan);
        item["extraEarthStem"] = palace.extra_di_gan ? nlohmann::ordered_json(gan_label(*palace.extra_di_gan))
                                                     : nlohmann::ordered_json(nullptr);
        item["hiddenStem"] = palace.hidden_gan ? nlohmann::ordered_json(gan_label(*palace.hidden_gan))
                                               : nlohmann::ordered_json(nullptr);

        // 宫位地支：branch_count 是中五宫 0 支、四隅宫 2 支的真实有效数，按它截断。
        nlohmann::ordered_json branches = nlohmann::ordered_json::array();
        for (std::size_t index = 0; index < palace.branch_count && index < palace.branches.size(); ++index)
            branches.push_back(zhi_label(palace.branches[index]));
        item["branches"] = std::move(branches);
        item["branchCount"] = palace.branch_count;

        item["isZhiFu"] = palace.is_zhi_fu;
        item["isZhiShi"] = palace.is_zhi_shi;
        item["isVoid"] = palace.is_void;
        item["isPostHorse"] = palace.is_post_horse;
        item["isJiGong"] = palace.is_ji_gong;
        item["hasGatePressure"] = palace.has_gate_pressure;
        item["hasLiuYiJiXing"] = palace.has_liu_yi_ji_xing;
        item["hasSanQiTomb"] = palace.has_san_qi_tomb;
        item["patterns"] = patterns_json(palace.patterns);
        return item;
    }

    /// @brief 起局四柱投影（设计文档 §6-1）；上游 QiMenPan::ba_zi 为空时给 null。
    auto pillars_json(const std::optional<BaZi>& bazi) -> nlohmann::ordered_json
    {
        if (!bazi)
            return nlohmann::ordered_json(nullptr);
        nlohmann::ordered_json item;
        item["year"] = bazi->year.to_string();
        item["month"] = bazi->month.to_string();
        item["day"] = bazi->day.to_string();
        item["hour"] = bazi->hour.to_string();
        return item;
    }

    /// @brief 起局四柱旬空二支（设计文档 §6-1）；上游为空或该支缺失时给空数组。
    auto pillars_xun_kong_json(const std::optional<BaZi>& bazi) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        if (!bazi)
            return array;
        for (const auto& branch : {bazi->xun_kong_1, bazi->xun_kong_2})
        {
            if (!branch.empty())
                array.push_back(branch);
        }
        return array;
    }

    /// @brief 用神落宫投影（上游 YongShenSelection）。
    auto yong_shen_json(const std::vector<YongShenSelection>& selections) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : selections)
        {
            array.push_back(nlohmann::ordered_json{{"role", enum_code(item.role)},
                {"palace", palace_number(item.palace)},
                {"palaceName", palace_label(item.palace)},
                {"name", item.name},
                {"basis", item.basis}});
        }
        return array;
    }

    /// @brief 日时主客投影（上游 MainGuestReading），basis 为设计文档 §6-11 的合参依据。
    auto main_guest_json(const MainGuestReading& reading) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["hostPalace"] = palace_number(reading.host_palace);
        item["hostPalaceName"] = palace_label(reading.host_palace);
        item["guestPalace"] = palace_number(reading.guest_palace);
        item["guestPalaceName"] = palace_label(reading.guest_palace);
        item["relation"] = enum_code(reading.relation);
        item["interpretation"] = reading.interpretation;
        item["basis"] = bases_json(reading.pan_ju_basis);
        return item;
    }

    /// @brief 用神职责数组投影（上游 PalaceReading::roles 是 YongShenRole 枚举，给码）。
    auto roles_json(const std::vector<YongShenRole>& roles) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& role : roles)
            array.push_back(enum_code(role));
        return array;
    }

    /**
     * @brief 主用诸宫合参投影（上游 PalaceReading）。
     *
     * patterns 是设计文档 §6-9 的缺口：上游 PalaceReading 已带该宫已成格局，
     * presenter 的中文 JSON 只输出到「宫象」为止，此处按结构体补齐。
     */
    auto palace_reading_json(const PalaceReading& reading) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["palace"] = palace_number(reading.palace);
        item["palaceName"] = palace_label(reading.palace);
        item["roles"] = roles_json(reading.roles);
        item["star"] = star_label(reading.star);
        item["gate"] = gate_label(reading.gate);
        item["spirit"] = spirit_label(reading.spirit);
        item["heavenStem"] = gan_label(reading.heaven_stem);
        item["earthStem"] = gan_label(reading.earth_stem);
        item["starStrength"] = strength_label(reading.star_strength);
        item["gateStrength"] = strength_label(reading.gate_strength);
        item["patterns"] = patterns_json(reading.patterns);
        item["favorable"] = bases_json(reading.favorable);
        item["unfavorable"] = bases_json(reading.unfavorable);
        item["image"] = reading.image;
        return item;
    }

    /**
     * @brief 分门应象投影（设计文档 §6-10；上游 AnalysisResult::manifestations）。
     *
     * presenter 的中文报告有【分门应象】段，但 to_zh_json 未输出，故此处按结构体补齐；
     * 每条自带盘局依据，前端不必从中文报告切句子。
     */
    auto manifestations_json(const std::vector<Manifestation>& items) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : items)
        {
            array.push_back(nlohmann::ordered_json{{"kind", enum_code(item.kind)},
                {"subject", item.subject},
                {"indication", item.indication},
                {"basis", bases_json(item.pan_ju_basis)}});
        }
        return array;
    }

    /**
     * @brief 应期与方位投影（上游 TimingDirection）。
     *
     * 候支给中文支数组：上游为空表示候应随寄宫，presenter 在中文报告里写成
     * 「中宫随寄宫」占位文本，结构化块按 C2 给空数组，不代填。
     */
    auto timing_json(const std::vector<TimingDirection>& items) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : items)
        {
            nlohmann::ordered_json branches = nlohmann::ordered_json::array();
            for (const auto branch : item.trigger_branches)
                branches.push_back(zhi_label(branch));
            array.push_back(nlohmann::ordered_json{{"priority", item.priority},
                {"palace", palace_number(item.palace)},
                {"palaceName", palace_label(item.palace)},
                {"direction", item.direction},
                {"triggerBranches", std::move(branches)},
                {"pace", item.pace},
                {"condition", item.condition},
                {"basis", item.basis}});
        }
        return array;
    }

    /// @brief 九宫数组投影，按 array<PalaceInfo, 9> 的宫序（宫 1..9）输出。
    auto palaces_json(const std::array<PalaceInfo, 9>& palaces) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& palace : palaces)
            array.push_back(palace_json(palace));
        return array;
    }

    /**
     * @brief 排盘与占断的结构化投影（前端设计文档 §5 字段映射表 + §6 待补字段清单）。
     *
     * 字段值全部来自上游 QiMenPan、AnalysisResult 与上游中文名函数，适配层只做重排、
     * 枚举名转换与 null 归一，不重算门迫 / 击刑 / 入墓 / 遁干 / 用神 / 主客 / 应期。
     * 设计文档 §6 的缺口里，第 1/2/3/4/5/6/7/8/9/10/11 项与第 12 项的卦名在此补齐；
     * 第 12 项的宫位固定方位（direction）上游没有宫位方位函数，故不输出。
     */
    auto chart_json(const QiMenPan& pan, const AnalysisResult& analysis) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json chart;
        // 契约版本置于结构化块首位，与六爻/梅花接口保持同一键序。
        chart["schemaVersion"] = analysis.schema_version;

        chart["solarDate"] = nlohmann::ordered_json{{"year", pan.solar_year},
            {"month", pan.solar_month},
            {"day", pan.solar_day},
            {"hour", pan.hour},
            {"minute", pan.minute}};
        chart["lunarDate"] = nlohmann::ordered_json{{"year", pan.lunar_year},
            {"month", pan.lunar_month},
            {"day", pan.lunar_day},
            {"isLeapMonth", pan.is_leap_month}};

        chart["dun"] = enum_code(pan.dun);
        chart["yuan"] = std::string{ZhouYi::QiMen::yuan_name(pan.yuan)};
        chart["ju"] = pan.ju;
        chart["solarTerm"] = std::string{ZhouYi::QiMen::solar_term_name(pan.solar_term)};
        chart["xunShou"] = std::string{ZhouYi::QiMen::jia_xun_name(pan.xun_shou)};
        // §6-2：旬首所遁六仪，上游 QiMenPan::xun_hidden_gan，中文报告写「甲申旬遁庚」。
        chart["xunHiddenGan"] = gan_label(pan.xun_hidden_gan);
        // §6-3：时柱旬首之旬空二支，上游 QiMenPan::void_branches；它与逐宫 is_void
        // 同源（本盘坤二宫带未、离九宫带午即为命中），也解释了哪些宫因空亡而降级。
        chart["voidBranches"] =
            nlohmann::ordered_json::array({zhi_label(pan.void_branches[0]), zhi_label(pan.void_branches[1])});
        // §6-1：四柱与旬空，上游 QiMenPan::ba_zi（上游 to_json 未序列化）。
        // 注意 castingXunKong 是四柱的旬空（上游 BaZi::xun_kong_1/2，随日柱），
        // 与上面随旬首（时柱）的 voidBranches 含义不同，两者都按上游原样输出、不互相顶替。
        chart["castingPillars"] = pillars_json(pan.ba_zi);
        chart["castingXunKong"] = pillars_xun_kong_json(pan.ba_zi);

        chart["zhiFu"] = nlohmann::ordered_json{{"star", star_label(pan.zhi_fu_star)},
            {"palace", palace_number(pan.zhi_fu_palace)},
            {"palaceName", palace_label(pan.zhi_fu_palace)}};
        chart["zhiShi"] = nlohmann::ordered_json{{"gate", gate_label(pan.zhi_shi_gate)},
            {"palace", palace_number(pan.zhi_shi_palace)},
            {"palaceName", palace_label(pan.zhi_shi_palace)}};

        chart["palaces"] = palaces_json(pan.palaces);
        chart["globalPatterns"] = patterns_json(pan.global_patterns);

        chart["yongShen"] = yong_shen_json(analysis.yong_shen);
        // §6-11：日时主客的合参依据（presenter 只输出主宫/客宫/生克/断意）。
        chart["mainGuest"] = main_guest_json(analysis.main_guest);
        // §6-9：主用诸宫合参，含该宫已成格局（presenter 只输出到宫象）。
        chart["palaceReadings"] = nlohmann::ordered_json::array();
        for (const auto& reading : analysis.palace_readings)
            chart["palaceReadings"].push_back(palace_reading_json(reading));

        // 全盘三组依据：presenter 的中文报告有，to_zh_json 未输出，按结构体补齐。
        chart["globalConfiguration"] = bases_json(analysis.global_configuration);
        chart["favorable"] = bases_json(analysis.favorable);
        chart["unfavorable"] = bases_json(analysis.unfavorable);
        // §6-10：分门应象（上游中文报告有，to_zh_json 未输出）。
        chart["manifestations"] = manifestations_json(analysis.manifestations);
        chart["timing"] = timing_json(analysis.timing);

        chart["judgment"] = enum_code(analysis.judgment);
        chart["conclusion"] = text_or_null(analysis.conclusion);
        chart["reviewPoints"] = analysis.review_points;
        return chart;
    }

} // namespace

auto qi_men_adapter::pai_pan(const cnetnexus::chart::qi_men::chart_input& input)
    -> std::expected<cnetnexus::chart::qi_men::chart_result, cnetnexus::framework::error::error_code>
{
    // 占类走上游枚举反射（奇门暂无
    // ZhMap），年命走上游天干中文映射，均不自建对照表。
    const auto question_kind = magic_enum::enum_cast<ZhouYi::QiMenAnalysis::QuestionKind>(input.kind);
    if (!question_kind)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    std::optional<ZhouYi::GanZhi::TianGan> nian_ming;
    if (input.nian_ming)
    {
        nian_ming = ZhouYi::GanZhi::Mapper::from_zh_gan(*input.nian_ming);
        if (!nian_ming)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }

    // 上游奇门排盘用 std::expected<QiMenPan, std::string>：失败即入参/历法非法。
    const auto pan = input.calendar == cnetnexus::chart::qi_men::calendar_kind::lunar
        ? ZhouYi::QiMen::QiMenController::pai_pan_lunar(input.year, input.month, input.day, input.hour, input.minute)
        : ZhouYi::QiMen::QiMenController::pai_pan_solar(input.year, input.month, input.day, input.hour, input.minute);
    if (!pan)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    cnetnexus::chart::qi_men::chart_result result{};
    result.input = input;
    try
    {
        ZhouYi::QiMenAnalysis::AnalysisRequest request;
        request.question_kind = *question_kind;
        request.question = input.question;
        request.nian_ming = nian_ming;

        const auto analysis = ZhouYi::QiMenAnalysis::analyze(*pan, request);

        std::ostringstream report;
        ZhouYi::QiMenAnalysis::write_zh(report, *pan, analysis);
        result.analysis_zh = report.str();
        auto pan_projection = parse_provider_object(
            ZhouYi::QiMenAnalysis::to_zh_json(*pan, analysis).dump());
        // 盘面同源两路：pan 保留上游中文 JSON 原样（既有键，不动），chart 按设计文档
        // §5/§6 投影成 camelCase 结构化字段供前端直接消费。
        auto chart_projection =
            parse_provider_object(chart_json(*pan, analysis).dump());
        if (!pan_projection || !chart_projection)
            return std::unexpected{
                cnetnexus::framework::error::error_code::algorithm_failed};
        result.pan_projection = std::move(*pan_projection);
        result.chart_projection = std::move(*chart_projection);
    }
    catch (const std::invalid_argument&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }
    catch (const std::exception&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
    }

    if (result.analysis_zh.empty())
        return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
    return result;
}

} // namespace cnetnexus::chart
