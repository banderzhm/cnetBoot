export module cnetnexus.chart.service.liu_yao_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 六爻用例：校验起卦输入并规范化动爻，再委派给算法接口。 */
class liu_yao_service
{
public:
    /// @param algorithm 算法端口（同步纯函数）
    /// @param compute 计算执行器：排盘在它上面执行，不占用请求事件循环
    liu_yao_service(cnetnexus::chart::liu_yao_algorithm& algorithm,
        cnetnexus::chart::compute_executor& compute) noexcept
        : algorithm_{&algorithm},
          compute_{&compute}
    {
    }

    /** @brief 六爻装卦并断卦。
     *  @param input 起卦输入；动爻会被去重并升序写入返回结果
     *  @return 断卦结果；输入非法返回 REQUEST.INVALID_ARGUMENT
     */
    [[nodiscard]] auto cast(const cnetnexus::chart::liu_yao::cast_input& input) const
        -> cnetmod::task<std::expected<cnetnexus::chart::liu_yao::cast_result, cnetnexus::framework::error::error_code>>;

private:
    cnetnexus::chart::liu_yao_algorithm* algorithm_;
    cnetnexus::chart::compute_executor* compute_;
};

} // namespace cnetnexus::chart
