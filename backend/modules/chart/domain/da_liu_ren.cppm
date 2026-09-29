export module cnetnexus.chart.domain.da_liu_ren;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::da_liu_ren {

/** @brief 起课输入。
 *  占类、本命、行年一律使用上游 ZhouYiLab
 * 已有的名称口径，本仓库不另建枚举与映射表： 占类取 ZhouYi.Mapper 的 ZhMap
 * 中文名（如 "求财"、"功名事业"），本命/行年取十二支汉字。
 *  未提供本命/行年时必须留空，不得由起课日期倒推。
 */
struct cast_input
{
    int year{};                           ///< 起课年
    int month{};                          ///< 起课月
    int day{};                            ///< 起课日
    int hour{};                           ///< 起课时，0-23
    int minute{};                         ///< 起课分，0-59
    std::string kind{"泛占"};             ///< 占类名称；默认泛占，取值由上游 ZhMap 定义
    std::string question;                 ///< 占问原文，可为空
    std::optional<std::string> ben_ming;  ///< 本命地支（单个汉字，如 "申"）；空表示未提供
    std::optional<std::string> xing_nian; ///< 行年地支（单个汉字，如 "子"）；空表示未提供
    std::optional<std::string> gender;    ///< 求测者性别（"男"/"女"）；空表示未提供

    /// 求测者出生时刻：四项齐备时由后端自动推本命与行年，省去手工查表；
    /// 任一项缺失则维持「未提供不推测」，与既有契约一致。
    /// 出生年用公历；早于当年立春的按上一年计（六壬与命理同口径）。
    std::optional<int> birth_year;  ///< 出生公历年
    std::optional<int> birth_month; ///< 出生公历月 1—12
    std::optional<int> birth_day;   ///< 出生公历日
    std::optional<int> birth_hour;  ///< 出生小时 0—23
    /// 行年推算口径："liu_jia_classic"（《大六壬类集》古法，默认）/ "modern"（现代通行法）。
    /// 空表示用默认口径；两法起点不同，结果可能不同，故显式可切。
    std::optional<std::string> xing_nian_rule;
};

/** @brief 自动推算出的本命/行年（仅当请求带了完整出生时刻时出现）。 */
struct derived_year_fate
{
    std::string ben_ming;          ///< 本命地支（单个汉字）
    std::string xing_nian;         ///< 行年地支（单个汉字）
    std::string xing_nian_gan_zhi; ///< 行年干支全称，如 "丙辰"（六壬重地支，干支供核对）
    int nominal_age{};             ///< 虚岁（立春换年，出生当年记一岁）
    int birth_cycle_year{};        ///< 生年太岁年（立春换年后的年份）
    std::string rule;              ///< 实际采用的推算口径
};

/** @brief 起课与占断结果投影。
 *  上游 pai_pan_solar 与 analyze 已经算好整盘，以下两个 JSON 字段把它一并交给
 *  接口调用方：
 *  - pan_json：上游 ZhouYi::DaLiuRenAnalysis::to_zh_json(pan, analysis) 的中文 JSON
 *    原样（天地盘、四课三传、课传关系、应期与总断），与 qi_men 的 pan 同源，
 *    键名全部是上游中文，保持不变；
 *  - chart_json：前端设计文档《大六壬天地盘界面设计》§5 字段映射表的 camelCase
 *    结构化盘面（schemaVersion / castingPillars / xunKong / yueJiang / guiRen /
 *    isDay / jiuZongMen / lessonPatterns / guaTi / palaces[] / lessons[] /
 *    transmissions[] / personalMarkers[] / transmissionRelations[] /
 *    crossLayerRelations[] / lessonPlateRelations[] / transmissionPlateRelations[] /
 *    groupCompositions[] / missingBranches[] / eightDoors[] / leiShen / patterns[] /
 *    favorable[] / unfavorable[] / timing[] / judgment / judgmentLabel / conclusion /
 *    reviewPoints / shenSha），契约版本置于结构化块首位。
 *
 *  字段值全部取自上游 DaLiuRenResult、AnalysisResult 与上游 ZhMap / GanZhi 映射，
 *  适配层只做重排、枚举名首字母降格与中文标签投影，不重算天地盘、四课、三传、
 *  遁干、课体、类神、应期或任何生克关系；枚举字段一律成对给出
 *  name（上游枚举名首字母降格）与 nameLabel（上游 ZhMap 中文）。
 *  transport 把 chart_json 的顶层键直接并入响应根。
 *
 *  设计文档 §6 中上游结构体确实没有的项（天地盘逐宫的旬空位、课局、时间口径）
 *  一律不输出，不用默认值、相近值或 analysisZh 文本切片顶替。
 */
struct cast_result
{
    cast_input input;        ///< 回显输入
    cnetmod::json::document pan_projection;   ///< 已解析的天地盘/四课三传/占断中文投影
    cnetmod::json::document chart_projection; ///< 已解析的 camelCase 结构化盘面
    std::string analysis_zh; ///< 中文课体与占断报告（上游 write_zh），供大模型提示词投影
    /// 自动推算出的本命/行年；仅当起课输入带了完整出生时刻时出现，否则为空。
    std::optional<derived_year_fate> derived;
};

} // namespace cnetnexus::chart::da_liu_ren
