export module cnetnexus.chart.domain.bazi;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::bazi {

/** @brief 输入历法；农历闰月沿用上游约定，用负数月份表示。 */
enum class calendar_kind
{
    solar, ///< 公历
    lunar, ///< 农历
};

/** @brief 分析流派；决定适配器调用上游子平还是盲派分析入口。 */
enum class analysis_method
{
    ziping,  ///< 子平法：月令取格、调候与格局病药
    mangpai, ///< 盲派法：宾主、体用、做功与宫位取象
};

/** @brief 排盘输入：出生时间、性别与分析流派；本版本不接受地点，因而不做真太阳时修正。 */
struct birth_input
{
    calendar_kind calendar{calendar_kind::solar};    ///< 输入历法
    int year{};                                      ///< 年
    int month{};                                     ///< 月；农历闰月为负数
    int day{};                                       ///< 日
    int hour{};                                      ///< 时，0-23
    int minute{};                                    ///< 分，0-59
    bool is_male{true};                              ///< 性别：true 男，false 女
    analysis_method method{analysis_method::ziping}; ///< 分析流派；默认子平
};

/** @brief 四柱投影，取值为中文干支（如 "庚辰"）。 */
struct pillar_view
{
    std::string year;  ///< 年柱
    std::string month; ///< 月柱
    std::string day;   ///< 日柱
    std::string hour;  ///< 时柱
};

/** @brief 命盘；本后端对外的最小排盘结果。 */
struct chart
{
    birth_input birth;         ///< 回显排盘所用输入
    pillar_view pillars;       ///< 四柱
    std::string xun_kong_1;    ///< 旬空地支一
    std::string xun_kong_2;    ///< 旬空地支二
    cnetmod::json::document chart_projection; ///< 已解析的盘面结构化投影（camelCase 提供方契约）
                               ///< 干支/五行/十神/藏干/纳音/地势/旬空、起运、大运、五行占比、
                               ///< 关系与盲派结构；全部由上游结构体投影，transport 解析后平铺到响应顶层
    cnetmod::json::document analysis_projection; ///< 上游分析结果的已解析原样投影；
                               ///< 字段口径与上游一致，前端不得直接依赖其内部结构做业务，
                               ///< 仅作 RawJsonPanel 与排查用途
    std::string chart_zh;      ///< 中文排盘原局（上游 BaZiPresenter::to_zh：基本信息、四柱、
                               ///< 天干/地支十神、藏干详情、旬空、大运摘要），供大模型提示词投影
    std::string analysis_zh;   ///< 子平中文分析报告（上游
                               ///< BaZiAnalysis::to_zh），供大模型提示词投影使用
};

/** @brief 流年/流月查询输入：命主出生输入 + 目标公历年 + 可选大运序号/目标农历月。 */
struct liu_nian_input
{
    birth_input birth;               ///< 命主出生输入（与排盘同一套校验）
    int target_year{};               ///< 目标公历年
    std::optional<int> da_yun_index; ///< 大运序号 0-9；未提供时取目标年所处大运
    /// 目标农历月（1—12，节气月口径）；提供时运限详情里附该月的流日整月表。
    std::optional<int> target_lunar_month;
};

/** @brief 流年/流月查询结果；盘面 JSON 由适配器按 provider 契约拼装。 */
struct liu_nian_result
{
    birth_input birth;      ///< 回显排盘所用输入
    int target_year{};      ///< 目标公历年
    cnetmod::json::document chart_projection; ///< 已解析流年盘面投影：所处大运、该年流年、该大运下
                            ///< 十个流年（含 isCurrent）与十二个流月；数据取上游
                            ///< BaZiResult::get_liu_nian / get_liu_yue_list
    std::string period_zh;  ///< 运限详情中文表（与上游 examples/example_ba_zi.cpp 同源同量）：
                            ///< 大运 10 步 + 童限详情 + 目标年起 30 年流年 + 目标年整年流月
                            ///< （+ 提供 target_lunar_month 时该节气月所在公历月的流日整月）；
                            ///< 全部取 ZhouYi::BaZiPresenter 的 to_zh_*，供大模型提示词投影
};

} // namespace cnetnexus::chart::bazi
