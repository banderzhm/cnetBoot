export module cnetnexus.chart.service.xuanxue.da_liu_ren_projection;

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


export namespace cnetnexus::chart {

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

[[nodiscard]] auto dlr_zhi_zh(DiZhi value) -> std::string;
[[nodiscard]] auto dlr_gan_zh(TianGan value) -> std::string;
[[nodiscard]] auto dlr_liu_qin_zh(LiuQin value) -> std::string;
[[nodiscard]] auto dlr_basis_json(const std::vector<KeChuanBasis>& items) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_missing_context_json(const MissingBranchContext& context) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_relation_json(const BranchRelationAnalysis& relation) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_lesson_json(const LessonAnalysis& lesson) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_transmission_json(const TransmissionAnalysis& transmission) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_palace_json(const PalaceAnalysis& palace) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_personal_marker_json(const PersonalMarkerAnalysis& marker) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_eight_door_json(const BaMenAnalysis& door) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_lei_shen_json(const LeiShenSelection& selection) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_pattern_json(const PatternAnalysis& pattern) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_timing_json(const TimingCandidate& candidate) -> nlohmann::ordered_json;
[[nodiscard]] auto dlr_is_landing_relation(const BranchRelationAnalysis& relation) -> bool;
[[nodiscard]] auto dlr_is_group_relation(const BranchRelationAnalysis& relation) -> bool;
[[nodiscard]] auto dlr_chart_json(const DaLiuRenResult& pan, const AnalysisResult& analysis) -> nlohmann::ordered_json;

} // namespace cnetnexus::chart
