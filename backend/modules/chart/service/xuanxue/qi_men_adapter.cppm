export module cnetnexus.chart.service.xuanxue.qi_men_adapter;

import std;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab 奇门控制器实现的适配器；无状态，不需要生命周期托管。
 */
class qi_men_adapter final : public cnetnexus::chart::qi_men_algorithm
{
public:
    [[nodiscard]] auto pai_pan(const cnetnexus::chart::qi_men::chart_input& input)
        -> std::expected<cnetnexus::chart::qi_men::chart_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
