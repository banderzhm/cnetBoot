module cnetnexus.chart.service.xuanxue.liu_yao_adapter;

import std;
import magic_enum;
import nlohmann.json;
import ZhouYi.BaZiBase;
import ZhouYi.GanZhi;
import ZhouYi.LiuYaoAnalysis.Contract;
import ZhouYi.LiuYaoAnalysis.Presenter;
import ZhouYi.LiuYaoController;
import ZhouYi.YiJing.HexagramCatalog;
import ZhouYi.ZhMapper;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.framework.error;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

namespace {

    using ZhouYi::LiuYaoAnalysis::AnalysisResult;
    using ZhouYi::LiuYaoAnalysis::GuaYaoBasis;
    using ZhouYi::LiuYaoAnalysis::YaoAnalysis;
    using ZhouYi::LiuYaoController::LiuYaoPaiPanResult;
    using ZhouYi::LiuYaoController::YaoDetails;

    /**
     * @brief 上游枚举名转前端契约的 lowerCamel 取值（如 WangXiang -> wangXiang）。
     *
     * 只做首字母降格，不维护第二份枚举名对照表；中文一律另给 *Label 字段，
     * 由上游 ZhouYi.Mapper 的 ZhMap 提供。
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
     * 上游只给中文五行，此处按文档口径转换，不做任何生克推算；认不出的值给 unknown。
     */
    auto element_tone(const std::string& element_zh) -> std::string
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

    /// @brief 爻位中文名；口径与上游样例 example_liu_yao_output.txt:24-29 一致，第 6 爻称「上爻」。
    constexpr std::array<std::string_view, 6> k_position_labels{"初爻", "二爻", "三爻", "四爻", "五爻", "上爻"};

    /// @brief 爻画类型：由上游 mainYaoType（'0'/'1'）与 changeMark（'O'/'X'/' '）组装。
    auto yao_symbol(char main_yao_type, const std::string& change_mark) -> std::string
    {
        const bool yang = main_yao_type == '1';
        const bool moving = change_mark == "O" || change_mark == "X";
        if (yang)
            return moving ? "laoYang" : "shaoYang";
        return moving ? "laoYin" : "shaoYin";
    }

    /// @brief 本卦/变卦纳甲干支投影：干支、五行与六亲。
    auto gan_zhi_json(const std::string& gan_zhi, const std::string& element_zh, const std::string& relative)
        -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["ganZhi"] = gan_zhi;
        item["element"] = element_tone(element_zh);
        item["relative"] = relative;
        return item;
    }

    /// @brief 单爻卦爻依据投影；nature 用前端契约的 lowerCamel 取值。
    auto basis_json(const std::vector<GuaYaoBasis>& basis) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : basis)
        {
            array.push_back(nlohmann::ordered_json{
                {"rule", item.rule}, {"detail", item.detail}, {"nature", enum_code(item.nature)}});
        }
        return array;
    }

    /// @brief 「日月作用」列标签：旬空/月破/日破布尔 + 逐爻依据里的日月爻际规则名。
    ///
    /// 「月建旺衰」已由 monthOrder 单独给出、动化已由 transformation 单独给出，
    /// 二者不重复进本列（见 domain 契约 cast_result 的口径说明）；组装只用上游已有字段，
    /// 不新增判断。
    auto day_effects(const YaoAnalysis& line) -> nlohmann::ordered_json
    {
        std::vector<std::string> effects;
        const auto push = [&effects](std::string value)
        {
            if (std::ranges::find(effects, value) == effects.end())
                effects.push_back(std::move(value));
        };
        if (line.is_xun_kong)
            push("旬空");
        if (line.is_month_broken)
            push("月破");
        if (line.is_day_broken)
            push("日破");
        for (const auto& basis : line.gua_yao_basis)
        {
            if (basis.rule == "月建旺衰" || (!line.transformation.empty() && basis.rule == line.transformation))
                continue;
            push(basis.rule);
        }
        return effects;
    }

    /// @brief 六十四卦资料投影；卦名、卦宫、内外卦与世应全部来自上游卦表。
    auto hexagram_json(const std::string& code, const ZhouYi::YiJing::HexagramInfo& info) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["name"] = info.name;
        item["palace"] = info.palaceType + "宫";
        item["palaceElement"] = element_tone(info.fiveElement);
        item["code"] = code;
        item["upperTrigram"] = info.outerHexagram;
        item["lowerTrigram"] = info.innerHexagram;
        item["shiPosition"] = info.shiYaoPosition;
        item["yingPosition"] = info.yingYaoPosition;
        return item;
    }

    /// @brief 变卦卦码：本卦码按动爻逐位取反。
    ///  与上游 analyze_liu_yao 内部生成变卦卦码的方式一致（见上游
    ///  liu_yao_controller.cpp 的 changed_code 计算），只做爻码变换，不涉及纳甲、
    ///  六亲、六神等装卦规则；变卦的卦名与卦宫仍取自上游六十四卦目录。
    auto changed_hexagram_code(const std::string& code, const std::vector<int>& changing_lines) -> std::string
    {
        auto changed = code;
        for (const int position : changing_lines)
        {
            if (position >= 1 && position <= 6)
            {
                auto& bit = changed[static_cast<std::size_t>(position - 1)];
                bit = bit == '0' ? '1' : '0';
            }
        }
        return changed;
    }

    /// @brief 单爻投影：装卦（YaoDetails）与断卦（YaoAnalysis）同爻位合并。
    auto yao_json(const YaoDetails& details, const YaoAnalysis& line) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json item;
        item["position"] = line.position;
        item["positionLabel"] = std::string{k_position_labels[static_cast<std::size_t>(line.position - 1)]};
        item["symbol"] = yao_symbol(details.mainYaoType, details.changeMark);
        item["spirit"] = details.spirit;
        if (details.hiddenPillar.to_string().empty())
        {
            item["fuShen"] = nullptr;
        }
        else
        {
            item["fuShen"] = nlohmann::ordered_json{{"relative", details.hiddenRelative},
                {"ganZhi", details.hiddenPillar.to_string()},
                {"element", element_tone(details.hiddenElement)}};
        }
        item["main"] = gan_zhi_json(details.mainPillar.to_string(), details.mainElement, details.mainRelative);
        item["isShi"] = line.is_shi;
        item["isYing"] = line.is_ying;
        item["isChanging"] = line.is_changing;
        item["monthOrder"] = details.wangShuai;
        item["strength"] = enum_code(line.strength);
        item["dayEffects"] = day_effects(line);
        item["isXunKong"] = line.is_xun_kong;
        item["isMonthBroken"] = line.is_month_broken;
        item["isDayBroken"] = line.is_day_broken;
        // 变卦只落在动爻行；静爻不给 changed，避免前端把本卦复制到变卦列。
        if (line.is_changing)
        {
            item["changed"] = nlohmann::ordered_json{{"ganZhi", details.changedPillar.to_string()},
                {"element", element_tone(details.changedElement)},
                {"relative", details.changedRelative}};
        }
        else
        {
            item["changed"] = nullptr;
        }
        item["transformation"] = line.transformation.empty() ? nlohmann::ordered_json(nullptr)
                                                             : nlohmann::ordered_json(line.transformation);
        item["basis"] = basis_json(line.gua_yao_basis);
        return item;
    }

    /// @brief 用神投影；未落爻时 position/branch/strengthLabel 给 null。
    auto yong_shen_json(const AnalysisResult& analysis) -> nlohmann::ordered_json
    {
        const auto& yong = analysis.yong_shen;
        nlohmann::ordered_json item;
        item["target"] = yong.target;
        item["state"] = enum_code(yong.state);
        item["stateLabel"] = std::string{ZhouYi::Mapper::to_zh(yong.state)};
        item["position"] = yong.position ? nlohmann::ordered_json(*yong.position) : nlohmann::ordered_json(nullptr);
        item["branch"] = yong.branch.empty() ? nlohmann::ordered_json(nullptr) : nlohmann::ordered_json(yong.branch);
        item["strengthLabel"] = yong.strength ? nlohmann::ordered_json(std::string{ZhouYi::Mapper::to_zh(*yong.strength)})
                                              : nlohmann::ordered_json(nullptr);
        item["manuallySpecified"] = yong.manually_specified;
        item["reason"] = yong.reason;
        return item;
    }

    /// @brief 关系汇总投影；kind 直接取上游 GuaYaoBasis.rule（月建与爻/日辰与爻/六合/相刑/世应作用…）。
    auto relations_json(const std::vector<GuaYaoBasis>& relations) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (const auto& item : relations)
        {
            array.push_back(nlohmann::ordered_json{{"kind", item.rule},
                {"detail", item.detail},
                {"positions", item.positions},
                {"nature", enum_code(item.nature)}});
        }
        return array;
    }

    /**
     * @brief 装卦与断卦的结构化投影（前端设计文档 §6.1 / §8 的字段口径）。
     *
     * 字段值全部来自上游 analyze_liu_yao 的 pai_pan 与 analysis，以及上游六十四卦目录；
     * 适配层只做重排、枚举名转换与中文标签投影，不重算纳甲、六亲、六神、世应或旺衰。
     * yao[] 按前端文档 §8.4 建议直接给 position 降序（上爻→初爻）。
     */
    auto chart_json(const ZhouYi::BaZiBase::BaZi& bazi, const LiuYaoPaiPanResult& pai_pan,
        const AnalysisResult& analysis, const std::string& main_code, const std::vector<int>& changing_lines)
        -> nlohmann::ordered_json
    {
        const auto& main_info = ZhouYi::YiJing::hexagram_info(main_code);

        nlohmann::ordered_json chart;
        // 契约版本置于结构化块首位，与梅花接口保持同一键序。
        chart["schemaVersion"] = analysis.schema_version;
        chart["castingPillars"] = nlohmann::ordered_json{{"year", bazi.year.to_string()},
            {"month", bazi.month.to_string()},
            {"day", bazi.day.to_string()},
            {"hour", bazi.hour.to_string()}};
        chart["monthBuild"] = bazi.month.branch();
        chart["dayBranch"] = bazi.day.branch();
        chart["xunKong"] = nlohmann::ordered_json::array();
        for (const auto& kong : {bazi.xun_kong_1, bazi.xun_kong_2})
        {
            if (!kong.empty())
                chart["xunKong"].push_back(kong);
        }
        chart["mainHexagram"] = hexagram_json(main_code, main_info);
        if (changing_lines.empty())
        {
            chart["changedHexagram"] = nullptr;
        }
        else
        {
            const auto bian_code = changed_hexagram_code(main_code, changing_lines);
            chart["changedHexagram"] = hexagram_json(bian_code, ZhouYi::YiJing::hexagram_info(bian_code));
        }

        // 盘式固定：上爻→初爻；上游两边（装卦与断卦）都是初爻→上爻，按 position 对齐后倒序。
        chart["yao"] = nlohmann::ordered_json::array();
        for (int position = 6; position >= 1; --position)
        {
            const auto index = static_cast<std::size_t>(position - 1);
            chart["yao"].push_back(yao_json(pai_pan.yao_list[index], analysis.yao[index]));
        }

        chart["yongShen"] = yong_shen_json(analysis);
        chart["relations"] = relations_json(analysis.relations);
        chart["judgment"] = enum_code(analysis.judgment);
        chart["judgmentLabel"] = std::string{ZhouYi::Mapper::to_zh(analysis.judgment)};
        chart["conclusion"] = analysis.conclusion;
        chart["reviewPoints"] = analysis.review_points;
        return chart;
    }

} // namespace

auto liu_yao_adapter::cast(const cnetnexus::chart::liu_yao::cast_input& input)
    -> std::expected<cnetnexus::chart::liu_yao::cast_result, cnetnexus::framework::error::error_code>
{
    // 占类交给上游 ZhMap 解析，本仓库不维护第二份对照表。
    const auto question_kind = ZhouYi::Mapper::from_zh<ZhouYi::LiuYaoAnalysis::QuestionKind>(input.kind);
    if (!question_kind)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    // 上游六爻链路不用
    // std::expected：无效爻码、爻位或时间抛异常，统一在此边界翻译。
    try
    {
        const auto casting_time =
            ZhouYi::BaZiBase::BaZi::from_solar(input.year, input.month, input.day, input.hour, input.minute);

        ZhouYi::LiuYaoAnalysis::AnalysisRequest request;
        request.question_kind = *question_kind;
        request.question = input.question;
        // 人工指定用神爻位直接透传上游字段；越界由上游抛 std::invalid_argument，
        // 在本函数边界翻译成 REQUEST.INVALID_ARGUMENT（transport 已先做一次同样校验）。
        request.specified_yong_shen_position = input.yong_shen_position;

        // generate_ai_json=true：取回上游算好的装卦中文 JSON（ai_read_json_data）。
        const auto result = ZhouYi::LiuYaoController::analyze_liu_yao(input.hexagram_code, casting_time,
            input.changing_lines, request, true);

        std::ostringstream report;
        ZhouYi::LiuYaoAnalysis::write_zh(report, result.analysis);

        cnetnexus::chart::liu_yao::cast_result output{};
        output.input = input;
        output.analysis_zh = report.str();

        // 盘面同源两路：pan 保留上游中文 JSON 原样（与 da_liu_ren/qi_men 的 pan 一致），
        // chart 按前端设计文档 §8 投影成 camelCase 结构化字段供前端直接消费。
        auto pan = result.pai_pan.ai_read_json_data;
        if (!pan.is_object())
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        pan["断卦"] = ZhouYi::LiuYaoAnalysis::to_zh_json(result.analysis);
        auto pan_projection = parse_provider_object(pan.dump());
        auto chart_projection = parse_provider_object(
            chart_json(casting_time, result.pai_pan, result.analysis,
                input.hexagram_code, input.changing_lines).dump());
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
    catch (const std::exception&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
    }
}

} // namespace cnetnexus::chart
