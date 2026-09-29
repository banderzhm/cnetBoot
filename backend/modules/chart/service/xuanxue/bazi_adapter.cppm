export module cnetnexus.chart.service.xuanxue.bazi_adapter;

import std;
import cnetnexus.chart.domain.bazi;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab 控制器实现的排盘适配器；无状态，不需要生命周期托管。
 */
class bazi_adapter final : public cnetnexus::chart::bazi_algorithm
{
public:
    [[nodiscard]] auto pai_pan(const cnetnexus::chart::bazi::birth_input& input)
        -> std::expected<cnetnexus::chart::bazi::chart, cnetnexus::framework::error::error_code> override;

    [[nodiscard]] auto liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input)
        -> std::expected<cnetnexus::chart::bazi::liu_nian_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
