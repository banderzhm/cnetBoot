export module cnetnexus.chart.domain.qi_men;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::qi_men {

/** @brief 输入历法。 */
enum class calendar_kind
{
    solar, ///< 公历
    lunar, ///< 农历；闰月以负数月份表示
};

/** @brief 奇门排盘输入。
 *  占类沿用上游枚举名（如 "GongMing"）：奇门上游目前没有 ZhMap 中文映射，
 *  本仓库不复制其内部中文表，因此按枚举名传参；年命为出生年干汉字（如 "庚"）。
 */
struct chart_input
{
    calendar_kind calendar{calendar_kind::solar}; ///< 输入历法
    int year{};                                   ///< 年
    int month{};                                  ///< 月；农历闰月为负数
    int day{};                                    ///< 日
    int hour{};                                   ///< 时，0-23
    int minute{};                                 ///< 分，0-59
    std::string kind{"FanZhan"};                  ///< 占类：上游 QuestionKind 枚举名
    std::string question;                         ///< 占问原文，可为空
    std::optional<std::string> nian_ming;         ///< 求测者出生年干（单个汉字）；空表示未提供
};

/** @brief 排盘与占断结果投影。
 *
 *  上游 QiMenPan 与 AnalysisResult 里已经算好的盘面，此前只投影了中文报告，
 *  以下三个 JSON 字段把上游算好的数据交给接口调用方：
 *  - pan_json：上游 ZhouYi::QiMenAnalysis::to_zh_json(pan, analysis) 的完整中文 JSON
 *    （占问 / 排盘 / 用神落宫 / 日时主客 / 主用诸宫 / 应期方位 / 总断），键名保持
 *    上游 snake_case 与中文原样，不重命名、不删减；
 *  - chart_json：前端设计文档 §5/§6 的 camelCase 结构化盘面，字段值全部取自上游
 *    QiMenPan、PalaceInfo、PatternFinding、AnalysisResult 与上游中文名函数
 *    （palace_name / trigram_name / star_name / gate_name / spirit_name /
 *    strength_name / solar_term_name / yuan_name / jia_xun_name）及
 *    ZhouYi::GanZhi::Mapper::to_zh，适配层只做重排与枚举名转换，不重算任何
 *    门迫 / 击刑 / 入墓 / 遁干 / 用神 / 主客 / 应期规则；
 *  - analysis_zh：中文排盘与占断报告（上游 write_zh），供大模型提示词投影。
 *
 *  chart_json 的顶层键（transport 把它们直接并入响应根，既有键一律保持不变）：
 *  schemaVersion / solarDate / lunarDate / dun / yuan / ju / solarTerm / xunShou /
 *  xunHiddenGan / voidBranches / castingPillars / castingXunKong / zhiFu / zhiShi /
 *  palaces / globalPatterns / yongShen / mainGuest / palaceReadings /
 *  globalConfiguration / favorable / unfavorable / manifestations / timing /
 *  judgment / conclusion / reviewPoints。
 *
 *  palaces[] 每宫给：palaceNumber / name / trigram / element / star / extraStar / gate /
 *  spirit / starStrength / gateStrength / heavenStem / extraHeavenStem / earthStem /
 *  extraEarthStem / hiddenStem / branches / branchCount / isZhiFu / isZhiShi / isVoid /
 *  isPostHorse / isJiGong / hasGatePressure / hasLiuYiJiXing / hasSanQiTomb / patterns。
 *
 *  取值口径（避免前端误读，也避免本仓自建对照表）：
 *  - 上游导出中文取值函数的枚举（宫、卦、九星、八门、八神、旺衰、节气、元、旬首）
 *    直接给中文，与 pan_json 中同源项逐字一致；
 *  - voidBranches 取上游 QiMenPan::void_branches（随旬首/时柱的旬空），
 *    castingXunKong 取上游 BaZi::xun_kong_1/2（随日柱的四柱旬空），二者含义不同，
 *    都按上游原样输出，不互相顶替、也不代填；
 *  - 上游未导出中文取值的枚举（Dun / PatternNature / QuestionKind / YongShenRole /
 *    PalaceRelation / EffectNature / Judgment）给 magic_enum::enum_name 首字母降格码；
 *    这些枚举的中文只在 qi_men_analysis_presenter.cpp 的匿名命名空间内，未导出且无
 *    ZhMap 特化，本仓不复制其对照表，中文仍可从 pan_json 的同源项读取。
 */
struct chart_result
{
    chart_input input;       ///< 回显输入
    cnetmod::json::document pan_projection;   ///< 已解析的九宫盘与占断中文投影
    cnetmod::json::document chart_projection; ///< 已解析的 camelCase 结构化盘面
    std::string analysis_zh; ///< 中文排盘与占断报告（上游 write_zh），供大模型提示词投影
};

} // namespace cnetnexus::chart::qi_men
