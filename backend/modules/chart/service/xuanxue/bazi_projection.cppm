export module cnetnexus.chart.service.xuanxue.bazi_projection;

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


export namespace cnetnexus::chart {

using ZhouYi::BaZiBase::Pillar;
using ZhouYi::GanZhi::DiZhi;
using ZhouYi::GanZhi::ShiShen;
using ZhouYi::GanZhi::TianGan;
using ZhouYi::GanZhi::WuXing;
using ZhouYi::GanZhi::YinYang;

[[nodiscard]] auto bazi_element_key(WuXing element) -> std::string_view;
[[nodiscard]] auto bazi_shi_shen_tone(ShiShen value) -> std::string_view;
[[nodiscard]] auto bazi_hidden_stems_json(TianGan day_gan, DiZhi zhi) -> nlohmann::ordered_json;
[[nodiscard]] auto bazi_element_json(const Pillar& pillar) -> nlohmann::ordered_json;
[[nodiscard]] auto bazi_lunar_json(const cnetnexus::chart::bazi::birth_input& input) -> nlohmann::ordered_json;
[[nodiscard]] auto bazi_mang_pai_json(const ZhouYi::BaZiAnalysis::BlindAnalysisDetail& blind) -> nlohmann::ordered_json;
[[nodiscard]] auto bazi_chart_projection_json(const cnetnexus::chart::bazi::birth_input& input, const ZhouYi::BaZi::BaZiResult& result, const ZhouYi::BaZiAnalysis::AnalysisResult& analysis) -> std::string;
[[nodiscard]] auto bazi_pai_pan_result(const cnetnexus::chart::bazi::birth_input& input) -> ZhouYi::BaZi::BaZiResult;
[[nodiscard]] auto bazi_analysis_request(const cnetnexus::chart::bazi::birth_input& input) -> ZhouYi::BaZiAnalysis::AnalysisRequest;

} // namespace cnetnexus::chart
