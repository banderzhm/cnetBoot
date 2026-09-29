/**
 * @brief llm_settings 的实现：小节校验 + key 引用解析 + 请求目标解析。
 *
 * 配置文件由 cnetmod Application 加载与解码；本单元只做校验与纯逻辑解析。
 */
module cnetboot.framework.config.llm_settings;

import std;
import cnetmod.application.configuration;

namespace cnetboot::framework::config {
namespace {

    using service_table =
        std::map<std::string, cnetmod::application::configured_service,
            std::less<>>;

    [[nodiscard]] auto trimmed(std::string_view value) -> std::string_view
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos)
            return {};
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    /// @brief key 别名：显式 name 优先，缺省取 service 名。
    [[nodiscard]] auto key_alias(const llm_key_options& key) -> std::string
    {
        return trimmed(key.name).empty() ? key.service : key.name;
    }

    [[nodiscard]] auto validate_provider(const llm_provider_options& provider,
        std::size_t index) -> std::expected<void, std::string>
    {
        const auto path = std::format("providers[{}]", index);
        if (trimmed(provider.name).empty())
            return std::unexpected{std::format("{}.name 不能为空", path)};
        if (provider.type != "openai")
            return std::unexpected{std::format(
                "{}.type 必须是 openai（当前 {}）", path, provider.type)};
        if (provider.models.empty())
            return std::unexpected{
                std::format("{}.models 必须是至少含一个模型名的字符串数组", path)};
        for (std::size_t model = 0; model < provider.models.size(); ++model)
        {
            if (trimmed(provider.models[model]).empty())
                return std::unexpected{
                    std::format("{}.models[{}] 不能为空", path, model)};
            for (std::size_t other = model + 1; other < provider.models.size(); ++other)
                if (provider.models[model] == provider.models[other])
                    return std::unexpected{std::format("{}.models 的模型 {} 重复",
                        path, provider.models[model])};
        }
        if (provider.keys.empty())
            return std::unexpected{std::format(
                "{}.keys 至少要一条（多 key = 多个 services 实例）", path)};
        for (std::size_t key = 0; key < provider.keys.size(); ++key)
        {
            if (trimmed(provider.keys[key].service).empty())
                return std::unexpected{
                    std::format("{}.keys[{}].service 不能为空", path, key)};
            const auto alias = key_alias(provider.keys[key]);
            for (std::size_t other = key + 1; other < provider.keys.size(); ++other)
                if (key_alias(provider.keys[other]) == alias)
                    return std::unexpected{std::format(
                        "{}.keys[{}] 的 key 别名 {} 在同一供应商内重复", path, other,
                        alias)};
        }
        return {};
    }

    /**
     * @brief 校验一个 services.<name> 引用并解析成一条 key。
     *
     * 校验点（加载期失败，不带进运行期）：
     *  - 服务条目存在且 type=openai（否则 cnetmod 不会注册 chat_model 服务）；
     *  - instance 非空（add_chat_model 的实例名）；
     *  - 仅对已启用实例：base_url 与 api_key 都是非空字符串（空 key 直接拒绝；
     *    多 key = 多实例），pool_size 在 1..1024（与 cnetmod openai 集成一致）。
     */
    [[nodiscard]] auto resolve_key(const service_table& services,
        const llm_key_options& key, std::string_view path)
        -> std::expected<llm_key, std::string>
    {
        const auto found = services.find(key.service);
        if (found == services.end())
            return std::unexpected{std::format(
                "{} 引用的 services.{} 不存在", path, key.service)};
        const auto& service = found->second;
        if (service.name != "openai")
            return std::unexpected{std::format(
                "{} 引用的 services.{} 的 type 必须是 openai（当前 {}）", path,
                key.service, service.name)};
        if (service.instance.empty())
            return std::unexpected{
                std::format("services.{}.instance 不能为空", key.service)};
        llm_key resolved{
            .name = key_alias(key),
            .service = key.service,
            .instance = service.instance,
            .enabled = service.enabled,
        };
        // 未启用的实例不会被 cnetmod 注册，也不会被调用：允许它的密钥为空（例如
        // enabled: ${LLM_ENABLED:-false} + api_key: ${KEY:-} 的部署模板）。
        if (!service.enabled)
            return resolved;
        auto base_url = service.string_property("base_url");
        if (!base_url)
            return std::unexpected{
                std::format("services.{}.base_url 必须是字符串", key.service)};
        if (!base_url->has_value() || trimmed(**base_url).empty())
            return std::unexpected{
                std::format("services.{}.base_url 不能为空", key.service)};
        auto api_key = service.string_property("api_key");
        if (!api_key)
            return std::unexpected{
                std::format("services.{}.api_key 必须是字符串", key.service)};
        if (!api_key->has_value() || trimmed(**api_key).empty())
            return std::unexpected{std::format(
                "services.{}.api_key 不能为空（每个 key 一个 services 实例，"
                "用 ${{VAR}} 环境变量占位符注入，禁止明文）",
                key.service)};
        auto pool_size = service.integer_property("pool_size");
        if (!pool_size)
            return std::unexpected{
                std::format("services.{}.pool_size 必须是整数", key.service)};
        if (pool_size->has_value() &&
            (**pool_size < 1 ||
                static_cast<std::size_t>(**pool_size) > llm_maximum_pool_size))
            return std::unexpected{std::format(
                "services.{}.pool_size 必须在 1..{}（当前 {}）", key.service,
                llm_maximum_pool_size, **pool_size)};
        return resolved;
    }

} // namespace

auto validate_llm_options(const llm_options& options)
    -> std::expected<void, std::string>
{
    if (options.resilience.attempts_per_key < 1)
        return std::unexpected{
            std::string{"resilience.attempts_per_key 必须 >= 1"}};
    if (options.resilience.initial_backoff_ms < 0 ||
        options.resilience.max_backoff_ms < options.resilience.initial_backoff_ms)
        return std::unexpected{std::string{
            "resilience 退避必须满足 0 <= initial_backoff_ms <= max_backoff_ms"}};

    if (options.providers.empty())
    {
        if (options.default_provider.empty() && options.default_model.empty())
            return {};
        return std::unexpected{std::string{
            "default_provider/default_model 在 providers 为空时必须为空"}};
    }
    if (trimmed(options.default_provider).empty())
        return std::unexpected{std::string{"缺少必填配置项 default_provider"}};
    if (trimmed(options.default_model).empty())
        return std::unexpected{std::string{"缺少必填配置项 default_model"}};

    for (std::size_t index = 0; index < options.providers.size(); ++index)
        if (auto valid = validate_provider(options.providers[index], index); !valid)
            return valid;

    for (std::size_t left = 0; left < options.providers.size(); ++left)
        for (std::size_t right = left + 1; right < options.providers.size(); ++right)
            if (options.providers[left].name == options.providers[right].name)
                return std::unexpected{std::format(
                    "providers 里的供应商名 {} 重复", options.providers[left].name)};

    // 一个 services 实例只能属于一条 key：否则两个供应商会共享同一连接池与熔断状态。
    std::set<std::string_view> referenced;
    for (const auto& provider : options.providers)
        for (const auto& key : provider.keys)
            if (!referenced.insert(key.service).second)
                return std::unexpected{std::format(
                    "services.{} 被多条 key 引用；一个 key 实例只能出现一次",
                    key.service)};

    const auto fallback = std::ranges::find(options.providers,
        options.default_provider, &llm_provider_options::name);
    if (fallback == options.providers.end())
        return std::unexpected{std::format(
            "default_provider={} 不在 providers 清单里", options.default_provider)};
    if (std::ranges::find(fallback->models, options.default_model) ==
        fallback->models.end())
        return std::unexpected{
            std::format("default_model={} 不在供应商 {} 的 models 清单里",
                options.default_model, fallback->name)};
    return {};
}

auto resolve_llm_directory(const llm_options& options,
    const service_table& services) -> std::expected<llm_directory, std::string>
{
    llm_directory directory{
        .default_provider = options.default_provider,
        .default_model = options.default_model,
        .resilience = options.resilience,
    };
    directory.providers.reserve(options.providers.size());
    for (std::size_t index = 0; index < options.providers.size(); ++index)
    {
        const auto& declared = options.providers[index];
        llm_provider provider{
            .name = declared.name, .type = declared.type, .models = declared.models};
        provider.keys.reserve(declared.keys.size());
        for (std::size_t key = 0; key < declared.keys.size(); ++key)
        {
            const auto path = std::format("{}.providers[{}].keys[{}]",
                llm_section_name, index, key);
            auto resolved = resolve_key(services, declared.keys[key], path);
            if (!resolved)
                return std::unexpected{std::move(resolved.error())};
            provider.keys.push_back(std::move(*resolved));
        }
        directory.providers.push_back(std::move(provider));
    }
    return directory;
}

auto llm_provider::usable() const noexcept -> bool
{
    return std::ranges::any_of(keys, &llm_key::enabled);
}

auto llm_provider::enabled_instances() const -> std::vector<std::string>
{
    std::vector<std::string> instances;
    instances.reserve(keys.size());
    for (const auto& key : keys)
        if (key.enabled)
            instances.push_back(key.instance);
    return instances;
}

auto llm_directory::find_provider(std::string_view name) const noexcept
    -> const llm_provider*
{
    const auto found = std::ranges::find(providers, name, &llm_provider::name);
    return found == providers.end() ? nullptr : std::addressof(*found);
}

auto llm_directory::usable() const noexcept -> bool
{
    return std::ranges::any_of(providers,
        [](const llm_provider& provider) { return provider.usable(); });
}

auto resolve_llm_target(const llm_directory& directory, std::string_view provider,
    std::string_view model) -> std::expected<llm_target, error_code>
{
    if (directory.providers.empty())
        return std::unexpected{llm_unconfigured_code};
    const auto provider_name =
        provider.empty() ? std::string_view{directory.default_provider} : provider;
    if (provider_name.empty())
        return std::unexpected{llm_unconfigured_code};
    const auto* found = directory.find_provider(provider_name);
    if (found == nullptr)
        return std::unexpected{llm_rejected_code};

    // 模型同理：空值回退 default_model，再校验它属于该供应商（配错清单直接拒绝）。
    const auto model_name =
        model.empty() ? std::string_view{directory.default_model} : model;
    if (model_name.empty())
        return std::unexpected{llm_unconfigured_code};
    if (std::ranges::find(found->models, model_name) == found->models.end())
        return std::unexpected{llm_rejected_code};
    if (!found->usable())
        return std::unexpected{llm_unconfigured_code};
    return llm_target{.provider = found->name, .model = std::string{model_name}};
}

auto llm_model_component(std::string_view provider) -> std::string
{
    return std::format("llm:{}", provider);
}

auto llm_provider_catalog(const llm_directory& directory)
    -> std::vector<llm_provider_view>
{
    std::vector<llm_provider_view> catalog;
    catalog.reserve(directory.providers.size());
    for (const auto& provider : directory.providers)
    {
        llm_provider_view view{
            .name = provider.name,
            .type = provider.type,
            .models = provider.models,
            .is_default = provider.name == directory.default_provider,
        };
        view.keys.reserve(provider.keys.size());
        for (const auto& key : provider.keys)
            view.keys.push_back(
                llm_credential_view{.name = key.name, .configured = key.enabled});
        catalog.push_back(std::move(view));
    }
    return catalog;
}

} // namespace cnetboot::framework::config
