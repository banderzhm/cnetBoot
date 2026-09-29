/**
 * @brief 多供应商补全实现：业务契约 llm_service 到 cnetmod 组合模型的薄适配层。
 *
 * cnetmod 提供显式组合能力，本层负责声明供应商和 key 策略：
 *  1. 解析请求目标（framework.config.resolve_llm_target：默认值回退、拒绝未知值）；
 *  2. 协议翻译（llm_request <-> ai::chat_request / chat_response）与错误码映射。
 *
 * 每个逻辑供应商在组件容器里是一个 ai::chat_model 组件 "llm:<provider>"，由
 * add_llm_models() 显式调用 cnetmod add_chat_model，并明确开启 round-robin 与跨实例
 * fallback；仅列出多个实例不会隐式开启这些行为。首帧之后绝不重试，避免重复文本。
 *
 * 错误语义（与 llm_service 契约一致）：
 *  - 目录为空 / 该供应商没有可用 key -> DEPENDENCY.UNAVAILABLE；
 *  - 未知 provider / 未知 model       -> ALGORITHM.FAILED（拒绝，不静默换模型）；
 *  - 所有 key 都调用失败              -> ALGORITHM.FAILED（原文只记日志）。
 *
 * 流式：sink 返回 false（客户端断开）时上游流随之停止，已生成部分按成功返回。
 */
export module cnetnexus.reading.service.model.composed_llm_service;

import std;
import cnetmod.ai;
import cnetmod.application.chat_model_composition;
import cnetmod.application.components;
import cnetmod.coro.task;
import cnetnexus.framework.config.llm_settings;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.service.llm_service;

export namespace cnetnexus::reading {

/// @brief 供应商名 -> 已组合的模型（每个供应商一个）。
using llm_model_table = std::map<std::string,
    std::shared_ptr<cnetmod::ai::chat_model>, std::less<>>;

/** @brief 多供应商 / 多模型 / 多 key 的补全实现。 */
class composed_llm_service final : public llm_service
{
public:
    /**
     * @param directory 已解析的供应商目录（不可变快照）
     * @param models 每个可用供应商的组合模型；缺失的供应商按「未配置」处理
     */
    composed_llm_service(cnetnexus::framework::config::llm_directory directory,
        llm_model_table models);

    /// @brief 至少一个供应商有可用 key 且已组合出模型。
    [[nodiscard]] auto configured() const noexcept -> bool override;

    [[nodiscard]] auto complete(const llm_request& request)
        -> cnetmod::task<std::expected<llm_completion,
            cnetnexus::framework::error::error_code>> override;

    [[nodiscard]] auto stream(const llm_request& request, const llm_delta_sink& sink)
        -> cnetmod::task<std::expected<llm_completion,
            cnetnexus::framework::error::error_code>> override;

    /// @brief 目录快照（脱敏目录端点的数据源）。
    [[nodiscard]] auto directory() const noexcept
        -> const cnetnexus::framework::config::llm_directory&;

private:
    /// @brief 一次请求的解析结果：目标 + 承载它的模型。
    struct binding
    {
        cnetnexus::framework::config::llm_target target;
        cnetmod::ai::chat_model* model{};
    };

    [[nodiscard]] auto bind(const llm_request& request) const
        -> std::expected<binding, cnetnexus::framework::error::error_code>;

    cnetnexus::framework::config::llm_directory directory_;
    llm_model_table models_;
};

/**
 * @brief 为目录里每个可用供应商注册一个 ai::chat_model 组件 "llm:<provider>"。
 *
 * 实例是该供应商已启用 key 对应的 chat_model_service 实例；这里显式声明 round-robin、
 * 跨实例 fallback 和 directory.resilience，不依赖 cnetmod 默认行为。
 */
void add_llm_models(cnetmod::application::component_collection& components,
    const cnetnexus::framework::config::llm_directory& directory);

/**
 * @brief 从组件容器取出 add_llm_models() 注册的模型，组装 composed_llm_service。
 */
[[nodiscard]] auto make_llm_service(cnetmod::application::component_resolver& resolver,
    cnetnexus::framework::config::llm_directory directory)
    -> std::shared_ptr<composed_llm_service>;

} // namespace cnetnexus::reading
