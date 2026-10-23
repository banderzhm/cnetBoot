
module cnetboot.ai.service.llm_catalog;

import std;
import cnetmod.json;
import cnetboot.framework.config.llm_settings;

namespace cnetboot::ai {

auto llm_catalog(const cnetboot::framework::config::llm_directory& directory)
    -> llm_catalog_snapshot
{
    return llm_catalog_snapshot{
        .configured = directory.usable(),
        .default_provider = directory.default_provider,
        .providers = cnetboot::framework::config::llm_provider_catalog(directory),
    };
}

auto llm_catalog_document(
    const cnetboot::framework::config::llm_directory& directory)
    -> cnetmod::json::document
{
    const auto snapshot = llm_catalog(directory);
    auto providers = cnetmod::json::array();
    for (const auto& provider : snapshot.providers)
    {
        auto models = cnetmod::json::array();
        for (const auto& model : provider.models)
            models.get_array().emplace_back(model);
        auto keys = cnetmod::json::array();
        for (const auto& key : provider.keys)
            keys.get_array().push_back(cnetmod::json::object(
                {{"name", key.name}, {"configured", key.configured}}));
        providers.get_array().push_back(cnetmod::json::object({
            {"name", provider.name},
            {"type", provider.type},
            {"default", provider.is_default},
            {"models", std::move(models)},
            {"keys", std::move(keys)},
        }));
    }
    return cnetmod::json::object({
        {"configured", snapshot.configured},
        {"default_provider", snapshot.default_provider},
        {"providers", std::move(providers)},
    });
}

} // namespace cnetboot::ai
