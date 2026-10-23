
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


    [[nodiscard]] auto key_alias(const llm_key_options& key) -> std::string
    {
        return trimmed(key.name).empty() ? key.service : key.name;
    }

    [[nodiscard]] auto validate_provider(const llm_provider_options& provider,
        std::size_t index) -> std::expected<void, std::string>
    {
        const auto path = std::format("providers[{}]", index);
        if (trimmed(provider.name).empty())
            return std::unexpected{std::format("{}.name must not be empty", path)};
        if (provider.type != "openai")
            return std::unexpected{std::format(
                "{}.type must be openai (got {})", path, provider.type)};
        if (provider.models.empty())
            return std::unexpected{
                std::format("{}.models must contain at least one model name", path)};
        for (std::size_t model = 0; model < provider.models.size(); ++model)
        {
            if (trimmed(provider.models[model]).empty())
                return std::unexpected{
                    std::format("{}.models[{}] must not be empty", path, model)};
            for (std::size_t other = model + 1; other < provider.models.size(); ++other)
                if (provider.models[model] == provider.models[other])
                    return std::unexpected{std::format("{}.models contains duplicate model {}",
                        path, provider.models[model])};
        }
        if (provider.keys.empty())
            return std::unexpected{std::format(
                "{}.keys must contain at least one service-backed key", path)};
        for (std::size_t key = 0; key < provider.keys.size(); ++key)
        {
            if (trimmed(provider.keys[key].service).empty())
                return std::unexpected{
                    std::format("{}.keys[{}].service must not be empty", path, key)};
            const auto alias = key_alias(provider.keys[key]);
            for (std::size_t other = key + 1; other < provider.keys.size(); ++other)
                if (key_alias(provider.keys[other]) == alias)
                    return std::unexpected{std::format(
                        "{}.keys[{}] duplicates key alias {} within the provider", path, other,
                        alias)};
        }
        return {};
    }


    [[nodiscard]] auto resolve_key(const service_table& services,
        const llm_key_options& key, std::string_view path)
        -> std::expected<llm_key, std::string>
    {
        const auto found = services.find(key.service);
        if (found == services.end())
            return std::unexpected{std::format(
                "{} references missing services.{}", path, key.service)};
        const auto& service = found->second;
        if (service.name != "openai")
            return std::unexpected{std::format(
                "{} references services.{} whose type must be openai (got {})", path,
                key.service, service.name)};
        if (service.instance.empty())
            return std::unexpected{
                std::format("services.{}.instance must not be empty", key.service)};
        llm_key resolved{
            .name = key_alias(key),
            .service = key.service,
            .instance = service.instance,
            .enabled = service.enabled,
        };


        if (!service.enabled)
            return resolved;
        auto base_url = service.string_property("base_url");
        if (!base_url)
            return std::unexpected{
                std::format("services.{}.base_url must be a string", key.service)};
        if (!base_url->has_value() || trimmed(**base_url).empty())
            return std::unexpected{
                std::format("services.{}.base_url must not be empty", key.service)};
        auto api_key = service.string_property("api_key");
        if (!api_key)
            return std::unexpected{
                std::format("services.{}.api_key must be a string", key.service)};
        if (!api_key->has_value() || trimmed(**api_key).empty())
            return std::unexpected{std::format(
                "services.{}.api_key must not be empty; inject one key per service "
                "instance through a ${{VAR}} environment placeholder",
                key.service)};
        auto pool_size = service.integer_property("pool_size");
        if (!pool_size)
            return std::unexpected{
                std::format("services.{}.pool_size must be an integer", key.service)};
        if (pool_size->has_value() &&
            (**pool_size < 1 ||
                static_cast<std::size_t>(**pool_size) > llm_maximum_pool_size))
            return std::unexpected{std::format(
                "services.{}.pool_size must be within 1..{} (got {})", key.service,
                llm_maximum_pool_size, **pool_size)};
        return resolved;
    }

} // namespace

auto validate_llm_options(const llm_options& options)
    -> std::expected<void, std::string>
{
    if (options.resilience.attempts_per_key < 1)
        return std::unexpected{
            std::string{"resilience.attempts_per_key must be >= 1"}};
    if (options.resilience.initial_backoff_ms < 0 ||
        options.resilience.max_backoff_ms < options.resilience.initial_backoff_ms)
        return std::unexpected{std::string{
            "resilience backoff must satisfy 0 <= initial_backoff_ms <= max_backoff_ms"}};

    if (options.providers.empty())
    {
        if (options.default_provider.empty() && options.default_model.empty())
            return {};
        return std::unexpected{std::string{
            "default_provider and default_model must be empty when providers is empty"}};
    }
    if (trimmed(options.default_provider).empty())
        return std::unexpected{std::string{"default_provider is required"}};
    if (trimmed(options.default_model).empty())
        return std::unexpected{std::string{"default_model is required"}};

    for (std::size_t index = 0; index < options.providers.size(); ++index)
        if (auto valid = validate_provider(options.providers[index], index); !valid)
            return valid;

    for (std::size_t left = 0; left < options.providers.size(); ++left)
        for (std::size_t right = left + 1; right < options.providers.size(); ++right)
            if (options.providers[left].name == options.providers[right].name)
                return std::unexpected{std::format(
                    "providers contains duplicate provider name {}", options.providers[left].name)};


    std::set<std::string_view> referenced;
    for (const auto& provider : options.providers)
        for (const auto& key : provider.keys)
            if (!referenced.insert(key.service).second)
                return std::unexpected{std::format(
                    "services.{} is referenced by multiple keys; each key service must be unique",
                    key.service)};

    const auto fallback = std::ranges::find(options.providers,
        options.default_provider, &llm_provider_options::name);
    if (fallback == options.providers.end())
        return std::unexpected{std::format(
            "default_provider={} is not present in providers", options.default_provider)};
    if (std::ranges::find(fallback->models, options.default_model) ==
        fallback->models.end())
        return std::unexpected{
            std::format("default_model={} is not listed by provider {}",
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
