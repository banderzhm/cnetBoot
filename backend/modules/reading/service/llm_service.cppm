/**
 * @brief 文本补全用例：把一次 LLM 请求交给已装配的补全实现，并统一失败语义。
 *
 * 未装配实现（未配置模型）时返回 DEPENDENCY.UNAVAILABLE，不静默降级成本地规则。
 * 流式与非流式共用同一份请求，保证两条路径的提示词逐字节一致。
 */
export module cnetnexus.reading.service.llm_service;

import std;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;

export namespace cnetnexus::reading {

/**
 * @brief 供应商无关的补全实现接口；具体适配器由组合根注入。
 *
 * 多供应商实现（cnetnexus.reading.service.model.composed_llm_service）在内部做
 * (provider, model) 解析，多 key 轮询与故障转移由 cnetmod 组合模型完成；用例侧只看到
 * 这一个接口，因此
 * reading_service / chat_service 无需知道供应商数量。
 */
class llm_service
{
public:
    llm_service() = default;
    llm_service(const llm_service&) = delete;
    llm_service(llm_service&&) = delete;
    auto operator=(const llm_service&) -> llm_service& = delete;
    auto operator=(llm_service&&) -> llm_service& = delete;
    virtual ~llm_service() = default;

    /** @brief 是否已配置可用的补全实现。
     *
     *  未配置时用例在**落库前**直接返回 DEPENDENCY.UNAVAILABLE（不静默降级、不写半条数据）；
     *  空实现 unavailable_llm_service 覆写为 false。
     */
    [[nodiscard]] virtual auto configured() const noexcept -> bool
    {
        return true;
    }

    /** @brief 执行一次非流式补全。
     *  @return 模型输出与 token 计量；未配置模型返回 DEPENDENCY.UNAVAILABLE，
     *          调用失败返回 ALGORITHM.FAILED
     */
    [[nodiscard]] virtual auto complete(const llm_request& request)
        -> cnetmod::task<std::expected<llm_completion,
            cnetnexus::framework::error::error_code>> = 0;

    /** @brief 执行一次流式补全，把增量依次交给 sink，最后返回全文与计量。
     *
     *  默认实现退化为一次非流式补全后单帧下发：不支持流式的实现无需改动即可满足契约。
     *  @param request 对话请求
     *  @param sink 增量回调；返回 false 表示下游已断开
     *  @return 失败语义与 complete 相同
     */
    [[nodiscard]] virtual auto stream(const llm_request& request, const llm_delta_sink& sink)
        -> cnetmod::task<std::expected<llm_completion,
            cnetnexus::framework::error::error_code>>
    {
        auto result = co_await complete(request);
        if (!result)
            co_return std::unexpected{result.error()};
        if (sink && !result->content.empty())
            (void)co_await sink(result->content);
        co_return *result;
    }
};

/** @brief 未装配补全实现时的空实现（保留用例的"未配置"分支仍返回 DEPENDENCY.UNAVAILABLE）。 */
class unavailable_llm_service final : public llm_service
{
public:
    [[nodiscard]] auto configured() const noexcept -> bool override
    {
        return false;
    }

    [[nodiscard]] auto complete(const llm_request&)
        -> cnetmod::task<std::expected<llm_completion,
            cnetnexus::framework::error::error_code>> override;
};

} // namespace cnetnexus::reading
