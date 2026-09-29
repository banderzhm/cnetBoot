export module cnetnexus.chart.service.xuanxue.bazi_liu_nian_projection;

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


export namespace cnetnexus::chart {

using ZhouYi::BaZiBase::Pillar;
using ZhouYi::GanZhi::DiZhi;
using ZhouYi::GanZhi::ShiShen;
using ZhouYi::GanZhi::TianGan;
using ZhouYi::GanZhi::WuXing;
using ZhouYi::GanZhi::YinYang;

[[nodiscard]] auto bazi_liu_nian_json(const ZhouYi::BaZi::LiuNian& value, int current_year) -> nlohmann::ordered_json;
[[nodiscard]] auto bazi_current_solar_year() -> int;
[[nodiscard]] auto bazi_liu_nian_projection_json(const cnetnexus::chart::bazi::liu_nian_input& input, const ZhouYi::BaZi::BaZiResult& result) -> std::string;
[[nodiscard]] auto bazi_qi_yun_zh(const cnetnexus::chart::bazi::liu_nian_input& input, const ZhouYi::BaZi::BaZiResult& result) -> std::string;

} // namespace cnetnexus::chart
