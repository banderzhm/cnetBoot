export module cnetnexus.chart.service.xuanxue.zi_wei_adapter;

import std;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab 紫微排盘实现的适配器；无状态，不需要生命周期托管。
 */
class zi_wei_adapter final : public cnetnexus::chart::zi_wei_algorithm
{
public:
    [[nodiscard]] auto pai_pan(const cnetnexus::chart::zi_wei::chart_input& input)
        -> std::expected<cnetnexus::chart::zi_wei::chart_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
