/**
 * @brief 术数计算的执行端口：CPU 密集的排盘不在请求事件循环上跑。
 *
 * 算法端口（algorithm_api）保持同步纯函数；六个术数 service 经本端口把一次计算
 * 交给执行器，协程在计算完成后回到发起请求的事件循环。生产实现由组合根提供
 * （Application 的 offload()，共享唯一的 CPU 池）；inline_compute() 只供单元测试
 * 就地执行。
 */
export module cnetnexus.chart.service.compute_executor;

import std;
import cnetmod.coro.task;

export namespace cnetnexus::chart {

class compute_executor
{
public:
    compute_executor() = default;
    compute_executor(const compute_executor&) = delete;
    auto operator=(const compute_executor&) -> compute_executor& = delete;
    virtual ~compute_executor() = default;

    /**
     * @brief 在计算线程上执行 work，完成后在调用方的事件循环上恢复。
     *
     * work 不得抛出异常（compute() 已经把异常装箱）。
     */
    [[nodiscard]] virtual auto execute(std::function<void()> work) -> cnetmod::task<void> = 0;
};

/// @brief 就地执行：只供单元测试（没有运行中的事件循环与 CPU 池）。
class inline_compute_executor final : public compute_executor
{
public:
    [[nodiscard]] auto execute(std::function<void()> work) -> cnetmod::task<void> override
    {
        work();
        co_return;
    }
};

/// @brief 进程内共享的就地执行器（测试夹具用）。
[[nodiscard]] inline auto inline_compute() -> compute_executor&
{
    static inline_compute_executor executor;
    return executor;
}

/**
 * @brief 把一次计算交给执行器并取回结果；计算抛出的异常在调用方协程里重新抛出。
 */
template <typename Function>
requires std::invocable<Function&> && (!std::is_void_v<std::invoke_result_t<Function&>>)
[[nodiscard]] auto compute(compute_executor& executor, Function function)
    -> cnetmod::task<std::invoke_result_t<Function&>>
{
    std::optional<std::invoke_result_t<Function&>> result;
    std::exception_ptr failure;
    co_await executor.execute([&function, &result, &failure]
        {
            try
            {
                result.emplace(std::invoke(function));
            }
            catch (...)
            {
                failure = std::current_exception();
            }
        });
    if (failure)
        std::rethrow_exception(failure);
    co_return std::move(*result);
}

} // namespace cnetnexus::chart
