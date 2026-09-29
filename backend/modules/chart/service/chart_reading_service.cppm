module;

#include <cnetmod/json_codec.hpp>

/**
 * @brief 断语生成范围的查询参数解析与「生成范围数据」段渲染。
 *
 * 口径（契约冻结，前后端一致）：
 *  - `scope` 逗号分隔、大小写不敏感、重复去重、顺序无关；缺省等价于 `benming`；
 *  - 未知取值 → 400；只有 ba_zi 与 zi_wei 支持 scope，其余术数带 scope 一律 400
 *    （统一 REQUEST.INVALID_ARGUMENT，不引入 ALGORITHM.NOT_SUPPORTED）；
 *  - scope 含 liunian 或 liuyue 必须给 target_year；含 liuyue 还必须给 target_lunar_month；
 *  - target_lunar_month / target_lunar_day / target_hour 是成套参数。
 *
 *  「生成范围数据」段只从既有盘面投影取值渲染文本，不重新推算任何干支、宫位或吉凶。
 */
export module cnetnexus.chart.service.chart_reading_service;

import std;
import cnetmod.json;
import cnetmod.coro.task;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;

export namespace cnetnexus::chart {

/** @brief 断语生成范围的查询参数解析结果（scope + target_*），已通过取值与依赖校验。 */
struct reading_scope_request
{
    cnetnexus::reading::reading_scope_selection scopes{}; ///< 规范化生成范围；缺省仅本命

    std::optional<int> target_year;         ///< 目标公历年（1900—2100）
    std::optional<int> target_lunar_month;  ///< 目标农历月 1—12
    std::optional<int> target_lunar_day;    ///< 目标农历日 1—30；仅紫微流月需要
    std::optional<std::string> target_hour; ///< 目标时辰地支汉字；仅紫微流月需要

    /** @brief 是否选中了大运/流年/流月（即需要目标年运限投影的范围）。 */
    [[nodiscard]] auto uses_target_projection() const noexcept -> bool;
};

/** @brief 八字的「生成范围数据」段：大运列表 / 当前大运 / 流年 / 流月。 */
[[nodiscard]] auto bazi_scope_section(const cnetmod::json::document& chart,
    const cnetmod::json::document* liu_nian,
    const reading_scope_request& request)
    -> std::string;

/** @brief 紫微的「生成范围数据」段：大限列表 / 当前大限 / 流年 / 流月。 */
[[nodiscard]] auto zi_wei_scope_section(const cnetmod::json::document& chart,
    const cnetmod::json::document* horoscope,
    const reading_scope_request& request)
    -> std::string;

struct chart_reading_material {
    std::string algorithm;
    std::string report_zh;
    std::string question;
};

/** @brief 生成大模型解读所需的确定性业务材料。 */
class chart_reading_service {
public:
    chart_reading_service(bazi_service& bazi, liu_yao_service& liu_yao,
        mei_hua_service& mei_hua, zi_wei_service& zi_wei,
        da_liu_ren_service& da_liu_ren, qi_men_service& qi_men) noexcept;

    [[nodiscard]] auto prepare(bazi::birth_input input,
        const reading_scope_request& scope, std::string question) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto prepare(liu_yao::cast_input input) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto prepare(mei_hua::cast_input input) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto prepare(zi_wei::chart_input input,
        const reading_scope_request& scope, std::string question) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto prepare(da_liu_ren::cast_input input) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;
    [[nodiscard]] auto prepare(qi_men::chart_input input) const
        -> cnetmod::task<std::expected<chart_reading_material,
            cnetnexus::framework::error::error_code>>;

private:
    bazi_service* bazi_{};
    liu_yao_service* liu_yao_{};
    mei_hua_service* mei_hua_{};
    zi_wei_service* zi_wei_{};
    da_liu_ren_service* da_liu_ren_{};
    qi_men_service* qi_men_{};
};

} // namespace cnetnexus::chart
