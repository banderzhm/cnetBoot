export module cnetnexus.chart.service.da_liu_ren_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 大六壬用例：校验起课输入后委派给算法接口。 */
class da_liu_ren_service
{
public:
    /// @param algorithm 算法端口（同步纯函数）
    /// @param compute 计算执行器：排盘在它上面执行，不占用请求事件循环
    da_liu_ren_service(cnetnexus::chart::da_liu_ren_algorithm& algorithm,
        cnetnexus::chart::compute_executor& compute) noexcept
        : algorithm_{&algorithm},
          compute_{&compute}
    {
    }

    /** @brief 起课并占断。
     *  @param input 起课输入
     *  @return 课盘与报告；参数越界或地支非法返回 REQUEST.INVALID_ARGUMENT
     */
    [[nodiscard]] auto cast(const cnetnexus::chart::da_liu_ren::cast_input& input) const
        -> cnetmod::task<std::expected<cnetnexus::chart::da_liu_ren::cast_result, cnetnexus::framework::error::error_code>>;

private:
    cnetnexus::chart::da_liu_ren_algorithm* algorithm_;
    cnetnexus::chart::compute_executor* compute_;
};

} // namespace cnetnexus::chart
