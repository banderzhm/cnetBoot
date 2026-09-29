export module cnetnexus.chart.service.qi_men_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 奇门排盘用例：校验排盘输入后委派给算法接口。 */
class qi_men_service
{
public:
    /// @param algorithm 算法端口（同步纯函数）
    /// @param compute 计算执行器：排盘在它上面执行，不占用请求事件循环
    qi_men_service(cnetnexus::chart::qi_men_algorithm& algorithm,
        cnetnexus::chart::compute_executor& compute) noexcept
        : algorithm_{&algorithm},
          compute_{&compute}
    {
    }

    /** @brief 排局并占断。
     *  @param input 排盘输入
     *  @return 盘面与报告；参数越界返回 REQUEST.INVALID_ARGUMENT
     */
    [[nodiscard]] auto pai_pan(const cnetnexus::chart::qi_men::chart_input& input) const
        -> cnetmod::task<std::expected<cnetnexus::chart::qi_men::chart_result, cnetnexus::framework::error::error_code>>;

private:
    cnetnexus::chart::qi_men_algorithm* algorithm_;
    cnetnexus::chart::compute_executor* compute_;
};

} // namespace cnetnexus::chart
