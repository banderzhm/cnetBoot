module cnetnexus.chart.service.xuanxue.bazi_adapter;

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
import cnetnexus.chart.service.xuanxue.bazi_liu_nian_projection;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

auto bazi_adapter::pai_pan(const cnetnexus::chart::bazi::birth_input& input)
    -> std::expected<cnetnexus::chart::bazi::chart, cnetnexus::framework::error::error_code>
{
    // 上游 ZhouYiLab 的八字链路不用 std::expected：参数非法抛
    // std::invalid_argument， 其它算法失败抛
    // std::exception，统一在本适配器边界翻译成已注册错误码。
    try
    {
        const auto result = bazi_pai_pan_result(input);

        cnetnexus::chart::bazi::chart chart{};
        chart.birth = input;
        chart.pillars.year = result.ba_zi.year.to_string();
        chart.pillars.month = result.ba_zi.month.to_string();
        chart.pillars.day = result.ba_zi.day.to_string();
        chart.pillars.hour = result.ba_zi.hour.to_string();
        chart.xun_kong_1 = result.ba_zi.xun_kong_1;
        chart.xun_kong_2 = result.ba_zi.xun_kong_2;

        if (chart.pillars.year.empty() || chart.pillars.day.empty())
            return std::unexpected{cnetnexus::framework::error::error_code::bazi_chart_unavailable};

        // 四路输出同源：四柱/旬空给接口，结构化盘面 JSON 给前端，中文排盘原局 + 中文分析报告
        // 给大模型提示词投影（两者都取上游 presenter，不在适配层另写中文文案）。
        const auto analysis = ZhouYi::BaZiController::analyze_ba_zi(result, bazi_analysis_request(input));
        auto chart_projection =
            parse_provider_object(bazi_chart_projection_json(input, result, analysis));
        auto analysis_projection =
            parse_provider_object(ZhouYi::BaZiAnalysis::to_json(analysis).dump());
        if (!chart_projection || !analysis_projection)
            return std::unexpected{
                cnetnexus::framework::error::error_code::algorithm_failed};
        chart.chart_projection = std::move(*chart_projection);
        chart.analysis_projection = std::move(*analysis_projection);
        chart.chart_zh = ZhouYi::BaZiPresenter::to_zh(result);
        chart.analysis_zh = ZhouYi::BaZiAnalysis::to_zh(analysis);

        if (chart.chart_zh.empty() || chart.analysis_zh.empty())
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        return chart;
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


auto bazi_adapter::liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input)
    -> std::expected<cnetnexus::chart::bazi::liu_nian_result, cnetnexus::framework::error::error_code>
{
    try
    {
        const auto result = bazi_pai_pan_result(input.birth);
        if (result.ba_zi.year.to_string().empty() || result.ba_zi.day.to_string().empty())
            return std::unexpected{cnetnexus::framework::error::error_code::bazi_chart_unavailable};

        cnetnexus::chart::bazi::liu_nian_result output{};
        output.birth = input.birth;
        output.target_year = input.target_year;
        auto projection =
            parse_provider_object(bazi_liu_nian_projection_json(input, result));
        if (!projection)
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        output.chart_projection = std::move(*projection);
        // 运限详情是断语提示词的附加载荷：表格渲染失败（空串）不算盘面失败，但记在返回值里由上层决定。
        output.period_zh = bazi_qi_yun_zh(input, result);
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
