export module cnetnexus.chart.service.xuanxue.da_liu_ren_adapter;

import std;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab
 * 大六壬控制器实现的适配器；无状态，不需要生命周期托管。 */
class da_liu_ren_adapter final : public cnetnexus::chart::da_liu_ren_algorithm
{
public:
    [[nodiscard]] auto cast(const cnetnexus::chart::da_liu_ren::cast_input& input)
        -> std::expected<cnetnexus::chart::da_liu_ren::cast_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
