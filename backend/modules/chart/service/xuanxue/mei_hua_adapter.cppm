export module cnetnexus.chart.service.xuanxue.mei_hua_adapter;

import std;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab 梅花控制器实现的适配器；无状态，不需要生命周期托管。
 */
class mei_hua_adapter final : public cnetnexus::chart::mei_hua_algorithm
{
public:
    [[nodiscard]] auto cast(const cnetnexus::chart::mei_hua::cast_input& input)
        -> std::expected<cnetnexus::chart::mei_hua::cast_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
