export module cnetnexus.chart.domain.liu_yao;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::liu_yao {

/** @brief 起卦输入。
 *  占类沿用 ZhouYiLab 的 ZhMap 中文口径（如
 * "财运"、"功名事业"），本仓库不另建枚举与映射表。
 */
struct cast_input
{
    int year{};                      ///< 起卦年
    int month{};                     ///< 起卦月
    int day{};                       ///< 起卦日
    int hour{};                      ///< 起卦时，0-23
    int minute{};                    ///< 起卦分，0-59
    std::string hexagram_code;       ///< 主卦码，六位 0/1，初爻到上爻
    std::vector<int> changing_lines; ///< 动爻爻位 1—6；已去重并升序
    std::string kind{"自身泛占"};    ///< 占类名称；取值由上游 ZhMap 定义
    std::string question;            ///< 占问原文，可为空

    /** @brief 人工指定用神爻位，取值 1—6；未提供时留空。
     *
     *  对应 GET /api/v1/liu_yao/cast 的可选查询参数 `yong_shen`，直接映射到上游
     *  ZhouYi::LiuYaoAnalysis::AnalysisRequest::specified_yong_shen_position
     *  （上游该字段的类型就是爻位 int，不接受六亲名），因此本参数只收爻位数字。
     *  取值越界、非数字或空值一律视为非法请求，transport 返回
     *  400 REQUEST.INVALID_ARGUMENT，**不静默忽略、不回退为自动取用**；
     *  完全不传该参数时行为与不指定用神完全一致（上游自行择取）。
     *  指定成功时结果里的 yongShen.manuallySpecified 为 true、position 等于传入爻位。
     */
    std::optional<int> yong_shen_position; ///< 人工指定用神爻位 1—6；空表示未指定
};

/** @brief 装卦与断卦结果投影。
 *  上游 analyze_liu_yao 返回的结构化盘面（LiuYaoPaiPanResult）此前只投影了中文报告，
 *  以下两个 JSON 字段把上游算好的装卦与断卦一并交给接口调用方：
 *  - pan_json：上游装卦中文 JSON（analyze_liu_yao 的 generate_ai_json=true，即
 *    ai_read_json_data，含八字、本卦/变卦名称、神煞与六爻装卦），并挂上游断卦中文
 *    JSON ZhouYi::LiuYaoAnalysis::to_zh_json(analysis) 于「断卦」键下；
 *  - chart_json：前端设计文档 §6.1/§8 的 camelCase 结构化盘面
 *    （schemaVersion / castingPillars / monthBuild / dayBranch / xunKong /
 *    mainHexagram / changedHexagram / yao[] / yongShen / relations[] / judgment /
 *    judgmentLabel / conclusion / reviewPoints）。契约版本置于结构化块首位，
 *    与梅花接口键序一致。字段值全部取自上游 YaoDetails、
 *    YaoAnalysis、HexagramInfo 与上游 ZhMap，适配层只做重排、枚举名转换与中文标签
 *    投影，不重算纳甲、六亲、六神、世应或旺衰；transport 把 chart_json 的顶层键
 *    直接并入响应根。
 *
 *  盘面的三处「不重复承载」口径（适配层有意为之，不是漏投影）：
 *  - yao[].dayEffects 只收旬空/月破/日破旗标与逐爻依据里的日月、爻际规则名；
 *    其中的「月建旺衰」规则已由 yao[].monthOrder（YaoDetails.wangShuai）单独承载，
 *    动化规则已由 yao[].transformation 单独承载，故二者都从 dayEffects 中剔除，
 *    不在一列里重复出现。
 *  - 变卦只存在于 changedHexagram 与动爻行的 yao[].changed；静爻行的 changed 为
 *    null，禁止用本卦纳甲/六亲顶替。
 */
struct cast_result
{
    cast_input input;        ///< 回显输入
    cnetmod::json::document pan_projection;   ///< 已解析的上游装卦与断卦中文投影
    cnetmod::json::document chart_projection; ///< 已解析的 camelCase 结构化盘面
    std::string analysis_zh; ///< 中文断卦报告（上游 presenter 输出）
};

} // namespace cnetnexus::chart::liu_yao
