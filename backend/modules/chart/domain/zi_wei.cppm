export module cnetnexus.chart.domain.zi_wei;

import std;
import cnetmod.json;

export namespace cnetnexus::chart::zi_wei {

/** @brief 输入历法。 */
enum class calendar_kind
{
    solar, ///< 公历
    lunar, ///< 农历；闰月以负数月份表示
};

/** @brief 紫微排盘输入。
 *  上游接口只接受时辰粒度（0—23 时），因此本契约**不含分钟**：请求带非零 minute
 * 必须被拒绝， 不能把它静默截断成整点。
 */
struct chart_input
{
    calendar_kind calendar{calendar_kind::solar}; ///< 输入历法
    int year{};                                   ///< 年
    int month{};                                  ///< 月；农历闰月为负数
    int day{};                                    ///< 日
    int hour{};                                   ///< 时，0-23
    bool is_male{true};                           ///< 性别：true 男，false 女

    /** @brief 目标流年公历年；取值 1900—2100。
     *  只有带该参数才计算并输出运限载荷；缺省时行为与本字段引入前完全一致。
     */
    std::optional<int> target_year;

    /** @brief 当前虚岁；取值 1—150。
     *  缺省时由 target_year − 出生年（本结构 year）+ 1 推导；显式给出时以调用方为准。
     */
    std::optional<int> current_age;

    /** @brief 目标农历月；取值 1—12。
     *
     *  **成套语义**（与 target_lunar_day、target_hour 三者同进同退）：三项要么齐备、要么全无。
     *  只给其中一项或两项、或三项齐备但没有 target_year，一律 REQUEST.INVALID_ARGUMENT
     *  (400)——不静默忽略、不替调用方补默认值，也不悄悄退回“不算流月/流日/流时”。
     *  三项齐备时上游 get_horoscope 才拿到真实的农历月/日/时；缺省时行为与这三个字段引入前
     *  完全一致（固定正月初一子时，且不投影 liuYue/liuRi/liuShi 与其流曜）。
     */
    std::optional<int> target_lunar_month;

    /** @brief 目标农历日；取值 1—30。成套语义见 target_lunar_month。
     *  农历月有大小月之分，30 日在只有 29 天的月份不存在；上游对不存在的农历日抛
     *  std::invalid_argument，适配层翻译成 REQUEST.INVALID_ARGUMENT (400)，不改成 29 日。
     */
    std::optional<int> target_lunar_day;

    /** @brief 目标时辰地支汉字：子丑寅卯辰巳午未申酉戌亥（与出生时辰 hour 的 0—23 数字时
     *  不同口径，上游 get_horoscope 只收地支）。取值映射取上游
     *  ZhouYi::GanZhi::Mapper::from_zh_zhi，适配层不自建对照表；非地支汉字一律
     *  REQUEST.INVALID_ARGUMENT (400)。成套语义见 target_lunar_month。
     */
    std::optional<std::string> target_hour;
};

/** @brief 紫微排盘结果投影。 */
struct chart_result
{
    chart_input input; ///< 回显输入

    /** @brief 上游 export_to_json_full 的输出（含十二宫、四化与格局），供接口调用方使用。
     *
     *  适配层在 chart.palaces[] 上补投影十二宫页脚字段，**只新增键**，不改上游既有键名与
     *  嵌套结构：
     *  - chang_sheng：十二长生中文名，来源上游 PalaceInfo::chang_sheng；
     *  - bo_shi：博士十二神中文名，来源上游 PalaceInfo::bo_shi；
     *  - sui_qian：岁前十二神中文名，来源上游 PalaceInfo::sui_qian；
     *  - jiang_qian：将前十二神中文名，来源上游 PalaceInfo::jiang_qian；
     *  - san_fang_si_zheng：{ben_gong_index, dui_gong_index, cai_bo_index, guan_lu_index}
     *    四个索引，本宫与对宫、两个三合宫，来源上游 get_san_fang_si_zheng()。取值与既有
     *    opposite_index 同一空间，都是 palaces[] 的数组下标（0 = 寅宫），**不是**
     *    branch_index；前端按数组下标取宫，不要另做 % 12 推算。
     *  四个神煞键在上游该宫无值时输出 null，不用“平”或其它占位符填充。
     *
     *  顶层另补投影 flyStars（camelCase）供前端画飞星四化连线，同样**只新增键**：
     *  - flyStars.palaces：12 项，顺序与 chart.palaces[] 完全一致，每宫
     *    {palaceIndex, palaceName, gongGan, items}；palaceName/gongGan 是该宫宫名与宫干的中文；
     *  - flyStars.year：生年四化 {gan, items}，gan 取上游生年天干；
     *  - flyStars.daXian / flyStars.liuNian（可选，仅当请求带 target_year）：
     *    {ganZhi, items}，干支与 horoscope 的大限、流年同源，缺 target_year 时整键不出现，
     *    前端按缺键降级（不画线、不显示开关）；
     *  - items 固定 4 条、顺序禄→权→科→忌，元素为
     *    {hua, star, targetPalaceIndex, targetPalaceName, self}：hua 取单字“禄/权/科/忌”；
     *    star 是上游四化表的化星名；targetPalaceIndex 是化星所落之宫，取值与 chart.palaces[]
     *    同一索引空间（palaces[] 数组下标，0 = 寅宫），**不是** branch_index，化星不在十二宫内
     *    时 targetPalaceIndex 与 targetPalaceName 一并输出 null，不用编号占位；self 为 true 表示
     *    化星落回本宫（宫干自化），只有 palaces 组的条目可能为 true，生年/大限/流年三组的条目
     *    挂在干支上、本身没有本宫，恒为 false。
     *    四化表不重写：本命十二宫与生年取上游 get_si_hua_entries(TianGan)，运限两组取上游
     *    HoroscopeResult::si_hua_entries。
     *
     *  设计文档 §11 的 scopeBadges（当前运限角标）不在 chart_json 内，而是单独的
     *  horoscope_json（见下）；除 flyStars 的两个运限四化分组（仅在带 target_year 时出现、
     *  与 horoscope 同源）外，chart_json 只描述本命盘，既有键名与嵌套结构不变。
     */
    cnetmod::json::document chart_projection;

    std::string report_zh; ///< 上游逐宫中文排盘文本，供大模型提示词投影使用

    /** @brief 可选运限载荷（camelCase JSON）；仅当请求带 target_year 时存在。
     *
     *  **触发条件**：chart_input::target_year 有值才调用上游
     *  ZiWeiResult::get_horoscope() 并填充本字段；没有该参数时本字段为 std::nullopt，
     *  transport 不输出 horoscope 键，响应与该参数引入前完全一致。
     *
     *  **current_age（虚岁）**：chart_input::current_age 显式给出时以调用方为准；未给出时
     *  由 target_year − 出生年（chart_input::year）+ 1 推导。上游只接受虚岁 ≥ 1，推导出的
     *  虚岁小于 1 时按 REQUEST.INVALID_ARGUMENT 拒绝，不静默改口径。
     *
     *  **索引空间**：载荷里所有 gongIndex（daXian / xiaoXian / liuNian / liuYue / liuRi /
     *  liuShi、transitStars 每组 12 项以及 palaceTags 的每一项）都是上游
     *  ZiWeiResult::palaces[] 的数组下标 = 寅起宫序
     *  （0=寅、1=卯、2=辰、3=巳、4=午、5=未、6=申、7=酉、8=戌、9=亥、10=子、11=丑），
     *  **不是 DiZhi 枚举值（DiZhi 里 子=0）**。它与既有 chart_json 的宫位下标同一空间，
     *  前端按数组下标取宫，不要另做 % 12 推算。
     *
     *  **目标农历月/日/时（流月/流日/流时的触发条件）**：target_lunar_month、
     *  target_lunar_day、target_hour 三项要齐备、且与 target_year 同时给出，才把真实的目标
     *  农历月/日/时交给上游 get_horoscope，并投影 liuYue/liuRi/liuShi 与 transitStars 的
     *  对应 scope；三项没给全（或三项齐备但没有 target_year）一律 REQUEST.INVALID_ARGUMENT
     *  (400)，见 chart_input::target_lunar_month。没给时沿用固定正月初一子时——这三项只影响
     *  流月/流日/流时及其流曜，不参与大限/小限/流年/十二宫角标，因此不会污染已投影字段——
     *  且**不投影** liuYue/liuRi/liuShi 与它们的流曜 scope，避免发出无意义数据。
     *
     *  **结构**（适配层投影的真实 nlohmann 对象，键名全部 camelCase）：
     *  - targetYear：目标公历年；
     *  - currentAge：本次运限使用的虚岁；
     *  - daXian：{gongIndex, startAge, endAge, tianGan, diZhi, siHua}，当前虚岁所在大限；
     *    tianGan/diZhi 取该宫真实宫干支，与 chart_json 的 palaces[gongIndex].gan_zhi 等值；
     *  - xiaoXian：{age, gongIndex}，当前虚岁小限宫；
     *  - liuNian：{year, ganZhi, gongIndex, siHua}，流年干支与流年命宫；
     *  - liuYue / liuRi / liuShi（可选，出现条件见上）：
     *    - liuYue：{lunarMonthIndex, ganZhi, gongIndex, siHua}，lunarMonthIndex 即目标农历月；
     *    - liuRi：{lunarDay, ganZhi, gongIndex, siHua}，lunarDay 即目标农历日；
     *    - liuShi：{hourBranch, ganZhi, gongIndex, siHua}，hourBranch 取上游
     *      LiuShiData::shi_chen 的地支中文；
     *    三者的 ganZhi 一律由上游 tian_gan/di_zhi 经 Mapper::to_zh 拼接，siHua 为
     *    [{star, hua}]，顺序固定化禄→化权→化科→化忌；
     *  - transitStars：{daXian, liuNian, liuYue?, liuRi?, liuShi?}，五组运限流曜星名
     *    （运魁钺昌曲禄羊陀马鸾喜／流…＋年解／月…／日…／时…）。每组固定 12 项
     *    [{gongIndex, stars} ×12]，gongIndex 恰为 0..11（与上面同一索引空间），stars 是上游
     *    HoroscopeResult::da_xian_stars / liu_nian_stars / liu_yue_stars / liu_ri_stars /
     *    liu_shi_stars 给的星名数组：该宫没有流曜就是空数组，**不代填**。daXian/liuNian
     *    恒有；liuYue/liuRi/liuShi 三个 scope 与同名流月/流日/流时键同步出现；
     *  - palaceTags：[{gongIndex, isDaXian, isXiaoXian, isLiuNian, suiQian, jiangQian} ×12]，
     *    十二宫角标；suiQian/jiangQian 是**目标流年口径**（流年地支起例），与本命盘
     *    palaces[i].sui_qian/jiang_qian 的生年支口径不同。中文名一律取上游
     *    ZhouYi::Mapper::to_zh 的 SuiQian12/JiangQian12 基准表，适配层不自建对照表。
     *
     *  **不再有“已知缺陷故不投影”清单**：大限干支、五组运限四化、五组流曜、流月/流日/流时
     *  的宫位与干支都已由上游修复（ZhouYiLab db04449 的 get_horoscope 实现；固定基准见上游
     *  tests/regression_zi_wei_horoscope.cpp），本适配层照实投影，不补算、不伪造、不用占位符
     *  冒充：上游给空数组就输出空数组，没被请求到的字段整键不出现。
     */
    std::optional<cnetmod::json::document> horoscope_projection;
};

} // namespace cnetnexus::chart::zi_wei
