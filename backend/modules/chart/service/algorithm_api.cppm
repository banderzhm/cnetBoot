export module cnetnexus.chart.service.algorithm_api;

import std;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief bazi 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class bazi_algorithm
{
public:
    bazi_algorithm() = default;
    bazi_algorithm(const bazi_algorithm&) = delete;
    auto operator=(const bazi_algorithm&) -> bazi_algorithm& = delete;
    bazi_algorithm(bazi_algorithm&&) = delete;
    auto operator=(bazi_algorithm&&) -> bazi_algorithm& = delete;
    virtual ~bazi_algorithm() = default;

    /** @brief 按出生信息排命盘。
     *  @param input 已校验的输入
     *  @return 命盘；输入被上游拒绝返回 REQUEST.INVALID_ARGUMENT，其它算法失败返回 ALGORITHM.FAILED 或 BAZI.CHART_UNAVAILABLE */
    [[nodiscard]] virtual auto pai_pan(const cnetnexus::chart::bazi::birth_input& input)
        -> std::expected<cnetnexus::chart::bazi::chart,
            cnetnexus::framework::error::error_code> = 0;

    /** @brief 取目标公历年所处的流年盘面（大运、流年、该运十流年、该年十二流月）。
     *  @param input 出生信息 + 目标公历年 + 可选大运序号；范围校验由 service 负责
     *  @return 流年盘面；输入被上游拒绝返回 REQUEST.INVALID_ARGUMENT，其它失败返回
     *  ALGORITHM.FAILED */
    [[nodiscard]] virtual auto liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input)
        -> std::expected<cnetnexus::chart::bazi::liu_nian_result,
            cnetnexus::framework::error::error_code> = 0;
};

/** @brief da_liu_ren 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class da_liu_ren_algorithm
{
public:
    da_liu_ren_algorithm() = default;
    da_liu_ren_algorithm(const da_liu_ren_algorithm&) = delete;
    auto operator=(const da_liu_ren_algorithm&) -> da_liu_ren_algorithm& = delete;
    da_liu_ren_algorithm(da_liu_ren_algorithm&&) = delete;
    auto operator=(da_liu_ren_algorithm&&) -> da_liu_ren_algorithm& = delete;
    virtual ~da_liu_ren_algorithm() = default;

    /** @brief 起课并占断。
     *  @param input 已校验的输入
     *  @return 课盘与报告；参数越界或地支非法返回 REQUEST.INVALID_ARGUMENT */
    [[nodiscard]] virtual auto cast(const cnetnexus::chart::da_liu_ren::cast_input& input)
        -> std::expected<cnetnexus::chart::da_liu_ren::cast_result,
            cnetnexus::framework::error::error_code> = 0;
};

/** @brief liu_yao 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class liu_yao_algorithm
{
public:
    liu_yao_algorithm() = default;
    liu_yao_algorithm(const liu_yao_algorithm&) = delete;
    auto operator=(const liu_yao_algorithm&) -> liu_yao_algorithm& = delete;
    liu_yao_algorithm(liu_yao_algorithm&&) = delete;
    auto operator=(liu_yao_algorithm&&) -> liu_yao_algorithm& = delete;
    virtual ~liu_yao_algorithm() = default;

    /** @brief 六爻装卦并断卦。
     *  @param input 已校验的输入
     *  @return 断卦结果；上游拒绝输入返回 REQUEST.INVALID_ARGUMENT，其它失败返回 ALGORITHM.FAILED */
    [[nodiscard]] virtual auto cast(const cnetnexus::chart::liu_yao::cast_input& input)
        -> std::expected<cnetnexus::chart::liu_yao::cast_result,
            cnetnexus::framework::error::error_code> = 0;
};

/** @brief mei_hua 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class mei_hua_algorithm
{
public:
    mei_hua_algorithm() = default;
    mei_hua_algorithm(const mei_hua_algorithm&) = delete;
    auto operator=(const mei_hua_algorithm&) -> mei_hua_algorithm& = delete;
    mei_hua_algorithm(mei_hua_algorithm&&) = delete;
    auto operator=(mei_hua_algorithm&&) -> mei_hua_algorithm& = delete;
    virtual ~mei_hua_algorithm() = default;

    /** @brief 按时间起卦并占断。
     *  @param input 已校验的输入
     *  @return 占断结果；上游拒绝输入返回 REQUEST.INVALID_ARGUMENT，其它失败返回 ALGORITHM.FAILED */
    [[nodiscard]] virtual auto cast(const cnetnexus::chart::mei_hua::cast_input& input)
        -> std::expected<cnetnexus::chart::mei_hua::cast_result,
            cnetnexus::framework::error::error_code> = 0;
};

/** @brief qi_men 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class qi_men_algorithm
{
public:
    qi_men_algorithm() = default;
    qi_men_algorithm(const qi_men_algorithm&) = delete;
    auto operator=(const qi_men_algorithm&) -> qi_men_algorithm& = delete;
    qi_men_algorithm(qi_men_algorithm&&) = delete;
    auto operator=(qi_men_algorithm&&) -> qi_men_algorithm& = delete;
    virtual ~qi_men_algorithm() = default;

    /** @brief 排局并占断。
     *  @param input 已校验的输入
     *  @return 九宫盘 JSON 与中文报告；输入非法返回 REQUEST.INVALID_ARGUMENT */
    [[nodiscard]] virtual auto pai_pan(const cnetnexus::chart::qi_men::chart_input& input)
        -> std::expected<cnetnexus::chart::qi_men::chart_result,
            cnetnexus::framework::error::error_code> = 0;
};

/** @brief zi_wei 能力接口；具体实现在 service/xuanxue 适配器。
 *  接口本身定义在 chart 模块内，不经任何 port 抽象转发。 */
class zi_wei_algorithm
{
public:
    zi_wei_algorithm() = default;
    zi_wei_algorithm(const zi_wei_algorithm&) = delete;
    auto operator=(const zi_wei_algorithm&) -> zi_wei_algorithm& = delete;
    zi_wei_algorithm(zi_wei_algorithm&&) = delete;
    auto operator=(zi_wei_algorithm&&) -> zi_wei_algorithm& = delete;
    virtual ~zi_wei_algorithm() = default;

    /** @brief 排紫微命盘。
     *  @param input 已校验的输入
     *  @return 命盘 JSON；上游拒绝输入返回 REQUEST.INVALID_ARGUMENT，其它失败返回 ALGORITHM.FAILED */
    [[nodiscard]] virtual auto pai_pan(const cnetnexus::chart::zi_wei::chart_input& input)
        -> std::expected<cnetnexus::chart::zi_wei::chart_result,
            cnetnexus::framework::error::error_code> = 0;
};

} // namespace cnetnexus::chart
