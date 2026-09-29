/**
 * @brief composed_llm_service 的实现：目标解析 + 协议翻译 + 一次组合模型调用。
 *
 * 只有这里认识 cnetmod 的 ai::chat_request / ai::chat_response；上层用例只看到
 * llm_request / llm_completion。
 */
module cnetnexus.reading.service.model.composed_llm_service;

import std;
import cnetmod.ai;
import cnetmod.application.chat_model_composition;
import cnetmod.application.components;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetnexus.framework.config.llm_settings;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.service.llm_service;

namespace cnetnexus::reading {
namespace {

    using framework_error = cnetnexus::framework::error::error_code;
    namespace config = cnetnexus::framework::config;

    /// @brief 把供应商无关的一条消息投影成 cnetmod 的消息（role 保持原样）。
    [[nodiscard]] auto to_ai_message(const llm_message& message) -> cnetmod::ai::message
    {
        if (message.role == "system")
            return cnetmod::ai::message::system(message.content);
        if (message.role == "assistant")
            return cnetmod::ai::message::model_output(message.content);
        if (message.role == "user")
            return cnetmod::ai::message::user(message.content);
        cnetmod::ai::message result;
        result.role = message.role;
        result.content = message.content;
        return result;
    }

    /// @brief 把一次 llm_request 投影成 cnetmod 的 chat_request：模型名只来自解析结果。
    [[nodiscard]] auto to_ai_request(const config::llm_target& target,
        const llm_request& request) -> cnetmod::ai::chat_request
    {
        cnetmod::ai::chat_request wire;
        wire.model = target.model;
        wire.temperature = request.temperature;
        wire.max_tokens = request.max_tokens;
        wire.messages.reserve(request.messages.size());
        for (const auto& message : request.messages)
            wire.messages.push_back(to_ai_message(message));
        // 推理强度不是 OpenAI 的标准字段，但兼容网关普遍接受；空值不发送。
        if (!request.reasoning_effort.empty())
            wire.set_extra_text("reasoning_effort", request.reasoning_effort);
        return wire;
    }

    [[nodiscard]] auto completion_of(const cnetmod::ai::chat_response& response)
        -> llm_completion
    {
        return llm_completion{
            .content = std::string{response.content()},
            .tokens_in = response.token_usage.prompt_tokens,
            .tokens_out = response.token_usage.completion_tokens,
        };
    }

    /// @brief 所有 key 都失败后一律 ALGORITHM.FAILED；失败原文只记日志（不含提示词正文）。
    [[nodiscard]] auto failure(const config::llm_target& target,
        const std::string& detail) -> framework_error
    {
        logger::warn{"llm invocation failed: provider={} model={} detail={}",
            target.provider, target.model, detail};
        return framework_error::algorithm_failed;
    }

    /**
     * @brief 流式增量转发器：把 cnetmod 的 chunk 交给业务 sink，并记录下游断开。
     *
     * 下游断开时返回 false，cnetmod 的流随之停止读取上游；resilient 装饰器在首帧
     * 之后不会重试，因此断开不会触发切 key。
     */
    class delta_relay
    {
    public:
        explicit delta_relay(const llm_delta_sink& sink) noexcept
            : sink_{&sink}
        {
        }

        [[nodiscard]] auto operator()(const cnetmod::ai::chat_chunk& chunk)
            -> cnetmod::task<bool>
        {
            if (chunk.delta_content.empty() || !*sink_)
                co_return true;
            co_return co_await (*sink_)(chunk.delta_content);
        }

    private:
        const llm_delta_sink* sink_;
    };

} // namespace

composed_llm_service::composed_llm_service(config::llm_directory directory,
    llm_model_table models)
    : directory_{std::move(directory)},
      models_{std::move(models)}
{
}

auto composed_llm_service::configured() const noexcept -> bool
{
    return !models_.empty() && directory_.usable();
}

auto composed_llm_service::directory() const noexcept -> const config::llm_directory&
{
    return directory_;
}

auto composed_llm_service::bind(const llm_request& request) const
    -> std::expected<binding, framework_error>
{
    auto target = config::resolve_llm_target(directory_, request.provider,
        request.model);
    if (!target)
        return std::unexpected{target.error()};
    const auto found = models_.find(target->provider);
    if (found == models_.end() || !found->second)
    {
        logger::warn{"llm provider has no composed model: provider={}",
            target->provider};
        return std::unexpected{config::llm_unconfigured_code};
    }
    return binding{.target = std::move(*target), .model = found->second.get()};
}

auto composed_llm_service::complete(const llm_request& request)
    -> cnetmod::task<std::expected<llm_completion, framework_error>>
{
    auto bound = bind(request);
    if (!bound)
        co_return std::unexpected{bound.error()};
    auto response = co_await bound->model->invoke(to_ai_request(bound->target, request));
    if (!response)
        co_return std::unexpected{failure(bound->target, response.error())};
    co_return completion_of(*response);
}

auto composed_llm_service::stream(const llm_request& request,
    const llm_delta_sink& sink)
    -> cnetmod::task<std::expected<llm_completion, framework_error>>
{
    auto bound = bind(request);
    if (!bound)
        co_return std::unexpected{bound.error()};
    auto response = co_await bound->model->stream(
        to_ai_request(bound->target, request), delta_relay{sink});
    if (!response)
        co_return std::unexpected{failure(bound->target, response.error())};
    // 客户端主动断开时上游返回「正常结束但内容被截断」，语义上仍算成功：
    // 上层（SSE）靠 sink 的 false 自己收尾，这里把已生成的部分交回去。
    co_return completion_of(*response);
}

void add_llm_models(cnetmod::application::component_collection& components,
    const config::llm_directory& directory)
{
    const cnetmod::ai::resilient_model_options resilience{
        .max_attempts_per_model = directory.resilience.attempts_per_key,
        .initial_backoff = std::chrono::milliseconds{
            directory.resilience.initial_backoff_ms},
        .max_backoff = std::chrono::milliseconds{directory.resilience.max_backoff_ms},
    };
    for (const auto& provider : directory.providers)
    {
        auto instances = provider.enabled_instances();
        if (instances.empty())
            continue; // 未启用的实例不会被 cnetmod 注册；该供应商按「未配置」处理
        cnetmod::application::add_chat_model(components,
            config::llm_model_component(provider.name),
            cnetmod::application::chat_model_composition{
                .instances = std::move(instances),
                .routing = cnetmod::application::chat_model_routing::round_robin,
                .resilience = resilience,
                .fallback_to_remaining_instances = true,
            });
    }
}

auto make_llm_service(cnetmod::application::component_resolver& resolver,
    config::llm_directory directory) -> std::shared_ptr<composed_llm_service>
{
    llm_model_table models;
    for (const auto& provider : directory.providers)
    {
        if (!provider.usable())
            continue;
        models.emplace(provider.name, resolver.shared<cnetmod::ai::chat_model>(
                                          config::llm_model_component(provider.name)));
    }
    return std::make_shared<composed_llm_service>(std::move(directory),
        std::move(models));
}

} // namespace cnetnexus::reading
