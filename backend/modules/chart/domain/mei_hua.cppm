export module cnetnexus.chart.domain.mei_hua;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::mei_hua {

/** @brief 时间起卦采用的历法。 */
enum class calendar_kind
{
    solar, ///< 公历输入
    lunar, ///< 农历输入
};

/** @brief 占问门类沿用上游 ZhouYiLab 的 ZhMap
 * 中文口径，本仓库不另建枚举与映射表。 */

/** @brief 时间起卦输入。 */
struct cast_input
{
    calendar_kind calendar{calendar_kind::solar}; ///< 输入历法
    int year{};                                   ///< 年
    int month{};                                  ///< 月
    int day{};                                    ///< 日
    int hour{};                                   ///< 时，0-23
    int minute{};                                 ///< 分，0-59
    std::string kind{"泛占"};                     ///< 占问门类名称；取值由上游 ZhMap 定义
    std::string question;                         ///< 占问原文，可为空
};

/** @brief 起卦与占断结果投影。
 *  上游 analyze_by_time 返回的 MeiHuaPan 此前只投影了中文报告，以下两个 JSON 字段
 *  把上游算好的盘面交给接口调用方：
 *  - pan_json：上游 ZhouYi::MeiHuaAnalysis::to_zh_json(pan, analysis) 的完整中文 JSON
 *    （起卦、本互变卦盘、体用、互变推演与占断），与 da_liu_ren/qi_men 的 pan 同源；
 *  - chart_json：前端设计文档 §6.1/§8 的 camelCase 结构化盘面
 *    （method / sourceNumbers / upperTotal / movingLine / benGua / huGua / bianGua /
 *    tiYong / benYong / huInfluences / bianInfluence / partyBalance /
 *    movingLineReading / images / questionReading / omenReadings / basis / judgment /
 *    judgmentDetail / timing / conclusion / reviewPoints / schemaVersion）。字段值全部
 *    取自上游 HexagramSnapshot、TiYong、AnalysisResult 与上游 ZhMap，适配层只做枚举名
 *    转换与中文标签投影，不重算起卦、互变、体用或卦力；transport 把 chart_json 的
 *    顶层键直接并入响应根。
 */
struct cast_result
{
    cast_input input;        ///< 回显输入
    cnetmod::json::document pan_projection;   ///< 已解析的上游排盘与占断中文投影
    cnetmod::json::document chart_projection; ///< 已解析的结构化盘面投影
    std::string analysis_zh; ///< 中文占断报告（上游 presenter 输出）
};

} // namespace cnetnexus::chart::mei_hua
