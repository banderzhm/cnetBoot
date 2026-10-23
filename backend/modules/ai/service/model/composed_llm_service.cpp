
module cnetboot.ai.service.model.composed_llm_service;

import std;
import cnetmod.ai;
import cnetmod.application.chat_model_composition;
import cnetmod.application.components;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetboot.framework.config.llm_settings;
import cnetboot.framework.error;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_service;

namespace cnetboot::ai {
namespace {

    using framework_error = cnetboot::framework::error::error_code;
    namespace config = cnetboot::framework::config;


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


    [[nodiscard]] auto failure(const config::llm_target& target,
        const std::string& detail) -> framework_error
    {
        logger::warn{"llm invocation failed: provider={} model={} detail={}",
            target.provider, target.model, detail};
        return framework_error::ai_request_failed;
    }


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
            continue;
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

} // namespace cnetboot::ai
