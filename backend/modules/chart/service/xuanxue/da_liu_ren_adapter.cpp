module cnetnexus.chart.service.xuanxue.da_liu_ren_adapter;

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
import cnetnexus.chart.service.xuanxue.da_liu_ren_projection;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

auto da_liu_ren_adapter::cast(const cnetnexus::chart::da_liu_ren::cast_input& input)
    -> std::expected<cnetnexus::chart::da_liu_ren::cast_result, cnetnexus::framework::error::error_code>
{
    // 占类、本命、行年、性别一律交给上游既有映射解析，本仓库不维护第二份对照表：
    //  - 占类 / 性别：ZhouYi.Mapper 的 ZhMap（from_zh）
    //  - 地支：ZhouYi.GanZhi::from_zh_zhi
    using ZhouYi::DaLiuRenAnalysis::AnalysisRequest;
    using ZhouYi::DaLiuRenAnalysis::QuestionKind;
    using ZhouYi::DaLiuRenAnalysis::SeekerGender;

    const auto question_kind = ZhouYi::Mapper::from_zh<QuestionKind>(input.kind);
    if (!question_kind)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    std::optional<ZhouYi::GanZhi::DiZhi> ben_ming;
    if (input.ben_ming)
    {
        ben_ming = ZhouYi::GanZhi::Mapper::from_zh_zhi(*input.ben_ming);
        if (!ben_ming)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }
    std::optional<ZhouYi::GanZhi::DiZhi> xing_nian;
    if (input.xing_nian)
    {
        xing_nian = ZhouYi::GanZhi::Mapper::from_zh_zhi(*input.xing_nian);
        if (!xing_nian)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }

    // 出生时刻齐备时自动推本命/行年（行年是虚岁+性别决定的年宫，让用户手查是多余的）；
    // 出生信息不全就维持「未提供不推测」，不猜。手工给了值就以手工值为准（可覆盖）。
    std::optional<cnetnexus::chart::da_liu_ren::derived_year_fate> derived;
    const bool has_birth = input.birth_year && input.birth_month && input.birth_day && input.birth_hour;
    if (has_birth)
    {
        if (!input.gender)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
        const auto seeker_gender = ZhouYi::Mapper::from_zh<SeekerGender>(*input.gender);
        if (!seeker_gender)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

        using ZhouYi::DaLiuRen::XingNianInput;
        using ZhouYi::DaLiuRen::XingNianRule;
        XingNianRule rule = XingNianRule::LiuJiaClassic;
        if (input.xing_nian_rule)
        {
            if (*input.xing_nian_rule == "modern")
                rule = XingNianRule::Modern;
            else if (*input.xing_nian_rule != "liu_jia_classic")
                return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
        }

        XingNianInput birth{};
        birth.birth_year = *input.birth_year;
        birth.birth_month = *input.birth_month;
        birth.birth_day = *input.birth_day;
        birth.birth_hour = *input.birth_hour;
        birth.birth_minute = 0;
        birth.is_male = *seeker_gender == SeekerGender::Nan;
        birth.casting_year = input.year;
        birth.casting_month = input.month;
        birth.casting_day = input.day;
        birth.casting_hour = input.hour;
        birth.casting_minute = input.minute;

        const auto computed = ZhouYi::DaLiuRen::calculate_xing_nian(birth, rule);
        cnetnexus::chart::da_liu_ren::derived_year_fate value;
        value.ben_ming = std::string{ZhouYi::GanZhi::Mapper::to_zh(computed.ben_ming)};
        value.xing_nian = std::string{ZhouYi::GanZhi::Mapper::to_zh(computed.xing_nian)};
        value.xing_nian_gan_zhi = computed.xing_nian_gan_zhi;
        value.nominal_age = computed.nominal_age;
        value.birth_cycle_year = computed.birth_cycle_year;
        value.rule = rule == XingNianRule::Modern ? "modern" : "liu_jia_classic";
        derived = value;
        // 未手工指定时，用推算值参与占断。
        if (!ben_ming)
            ben_ming = computed.ben_ming;
        if (!xing_nian)
            xing_nian = computed.xing_nian;
    }
    std::optional<SeekerGender> gender;
    if (input.gender)
    {
        gender = ZhouYi::Mapper::from_zh<SeekerGender>(*input.gender);
        if (!gender)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }

    // 上游大六壬链路不用 std::expected：历法与入参非法抛异常，统一在此边界翻译。
    try
    {
        const auto pan = ZhouYi::DaLiuRen::Controller::DaLiuRenController::pai_pan_solar(input.year, input.month,
            input.day, input.hour, input.minute);

        AnalysisRequest request;
        request.question_kind = *question_kind;
        request.question = input.question;
        request.ben_ming = ben_ming;
        request.xing_nian = xing_nian;
        request.gender = gender;

        const auto analysis = ZhouYi::DaLiuRenAnalysis::analyze(pan, request);

        std::ostringstream report;
        ZhouYi::DaLiuRenAnalysis::write_zh(report, pan, analysis);

        cnetnexus::chart::da_liu_ren::cast_result result{};
        result.input = input;
        result.derived = derived;
        result.analysis_zh = report.str();
        // 盘面同源两路：pan 保留上游 to_zh_json 的中文 JSON 原样（既有顶层键不动），
        // chart 按前端设计文档 §5 投影成 camelCase 结构化字段。
        auto pan_projection = parse_provider_object(
            ZhouYi::DaLiuRenAnalysis::to_zh_json(pan, analysis).dump());
        auto chart_projection =
            parse_provider_object(dlr_chart_json(pan, analysis).dump());
        if (!pan_projection || !chart_projection || result.analysis_zh.empty())
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        result.pan_projection = std::move(*pan_projection);
        result.chart_projection = std::move(*chart_projection);
        return result;
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
