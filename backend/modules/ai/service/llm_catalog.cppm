
export module cnetboot.ai.service.llm_catalog;

import std;
import cnetmod.json;
import cnetboot.framework.config.llm_settings;

export namespace cnetboot::ai {


struct llm_catalog_snapshot
{
    bool configured{false};
    std::string default_provider;
    std::vector<cnetboot::framework::config::llm_provider_view> providers;
};


[[nodiscard]] auto llm_catalog(
    const cnetboot::framework::config::llm_directory& directory)
    -> llm_catalog_snapshot;


[[nodiscard]] auto llm_catalog_document(
    const cnetboot::framework::config::llm_directory& directory)
    -> cnetmod::json::document;

} // namespace cnetboot::ai
