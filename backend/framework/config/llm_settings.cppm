
module;

#include <cnetmod/json.hpp>

export module cnetboot.framework.config.llm_settings;

import std;
import cnetmod.application.configuration;
import cnetmod.json;
import cnetboot.framework.error;

export namespace cnetboot::framework::config {


using error_code = cnetboot::framework::error::error_code;


inline constexpr error_code llm_unconfigured_code{error_code::upstream_unavailable};


inline constexpr error_code llm_rejected_code{error_code::ai_request_failed};


inline constexpr std::size_t llm_maximum_pool_size{1024};


inline constexpr std::string_view llm_section_name{"llm"};


struct llm_key_options
{
    std::string name;
    std::string service;
};


struct llm_provider_options
{
    std::string name;
    std::string type{"openai"};
    std::vector<std::string> models;
    std::vector<std::string> model_prefixes;
    std::vector<llm_key_options> keys;
};


struct llm_resilience_options
{

    std::size_t attempts_per_key{1};

    std::int64_t initial_backoff_ms{100};

    std::int64_t max_backoff_ms{2000};
};


struct llm_options
{
    std::string default_provider;
    std::string default_model;
    std::vector<llm_provider_options> providers;
    llm_resilience_options resilience;
};


[[nodiscard]] auto validate_llm_options(const llm_options& options)
    -> std::expected<void, std::string>;


struct llm_key
{
    std::string name;
    std::string service;
    std::string instance;
    bool enabled{false};
};


struct llm_provider
{
    std::string name;
    std::string type;
    std::vector<std::string> models;
    std::vector<llm_key> keys;


    [[nodiscard]] auto usable() const noexcept -> bool;

    [[nodiscard]] auto enabled_instances() const -> std::vector<std::string>;
};


struct llm_directory
{
    std::string default_provider;
    std::string default_model;
    std::vector<llm_provider> providers;
    llm_resilience_options resilience;


    [[nodiscard]] auto find_provider(std::string_view name) const noexcept
        -> const llm_provider*;

    [[nodiscard]] auto usable() const noexcept -> bool;
};


[[nodiscard]] auto resolve_llm_directory(const llm_options& options,
    const std::map<std::string, cnetmod::application::configured_service,
        std::less<>>& services)
    -> std::expected<llm_directory, std::string>;


struct llm_target
{
    std::string provider;
    std::string model;

    auto operator==(const llm_target&) const -> bool = default;
};


[[nodiscard]] auto resolve_llm_target(const llm_directory& directory,
    std::string_view provider, std::string_view model)
    -> std::expected<llm_target, error_code>;


[[nodiscard]] auto llm_model_component(std::string_view provider) -> std::string;


struct llm_credential_view
{
    std::string name;
    bool configured{false};
};


struct llm_provider_view
{
    std::string name;
    std::string type;
    std::vector<std::string> models;
    std::vector<llm_credential_view> keys;
    bool is_default{false};
};


[[nodiscard]] auto llm_provider_catalog(const llm_directory& directory)
    -> std::vector<llm_provider_view>;

} // namespace cnetboot::framework::config

CNETMOD_JSON(cnetboot::framework::config::llm_key_options,
    CNETMOD_JSON_FIELD(name),
    CNETMOD_JSON_FIELD(service))

CNETMOD_JSON(cnetboot::framework::config::llm_provider_options,
    CNETMOD_JSON_FIELD(name),
    CNETMOD_JSON_FIELD(type),
    CNETMOD_JSON_FIELD(models),
    CNETMOD_JSON_FIELD(model_prefixes),
    CNETMOD_JSON_FIELD(keys))

CNETMOD_JSON(cnetboot::framework::config::llm_resilience_options,
    CNETMOD_JSON_FIELD(attempts_per_key),
    CNETMOD_JSON_FIELD(initial_backoff_ms),
    CNETMOD_JSON_FIELD(max_backoff_ms))

CNETMOD_JSON(cnetboot::framework::config::llm_options,
    CNETMOD_JSON_FIELD(default_provider),
    CNETMOD_JSON_FIELD(default_model),
    CNETMOD_JSON_FIELD(providers),
    CNETMOD_JSON_FIELD(resilience))
