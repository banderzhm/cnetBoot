export module cnetnexus.chart.service.mei_hua_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 梅花易数用例：校验起卦输入后委派给算法接口。 */
class mei_hua_service
{
public:
    /// @param algorithm 算法端口（同步纯函数）
    /// @param compute 计算执行器：排盘在它上面执行，不占用请求事件循环
    mei_hua_service(cnetnexus::chart::mei_hua_algorithm& algorithm,
        cnetnexus::chart::compute_executor& compute) noexcept
        : algorithm_{&algorithm},
          compute_{&compute}
    {
    }

    /** @brief 按时间起卦并占断。
     *  @param input 起卦输入
     *  @return 占断结果；参数越界返回 REQUEST.INVALID_ARGUMENT，算法失败沿用算法接口返回的错误码
     */
    [[nodiscard]] auto cast(const cnetnexus::chart::mei_hua::cast_input& input) const
        -> cnetmod::task<std::expected<cnetnexus::chart::mei_hua::cast_result, cnetnexus::framework::error::error_code>>;

private:
    cnetnexus::chart::mei_hua_algorithm* algorithm_;
    cnetnexus::chart::compute_executor* compute_;
};

} // namespace cnetnexus::chart
