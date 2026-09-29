export module cnetnexus.chart.service.xuanxue.liu_yao_adapter;

import std;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 用上游 ZhouYiLab 六爻控制器实现的适配器；无状态，不需要生命周期托管。
 */
class liu_yao_adapter final : public cnetnexus::chart::liu_yao_algorithm
{
public:
    [[nodiscard]] auto cast(const cnetnexus::chart::liu_yao::cast_input& input)
        -> std::expected<cnetnexus::chart::liu_yao::cast_result, cnetnexus::framework::error::error_code> override;
};

} // namespace cnetnexus::chart
