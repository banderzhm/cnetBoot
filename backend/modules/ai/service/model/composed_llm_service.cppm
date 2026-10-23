
export module cnetboot.ai.service.model.composed_llm_service;

import std;
import cnetmod.ai;
import cnetmod.application.chat_model_composition;
import cnetmod.application.components;
import cnetmod.coro.task;
import cnetboot.framework.config.llm_settings;
import cnetboot.framework.error;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_service;

export namespace cnetboot::ai {


using llm_model_table = std::map<std::string,
    std::shared_ptr<cnetmod::ai::chat_model>, std::less<>>;


class composed_llm_service final : public llm_service
{
public:

    composed_llm_service(cnetboot::framework::config::llm_directory directory,
        llm_model_table models);


    [[nodiscard]] auto configured() const noexcept -> bool override;

    [[nodiscard]] auto complete(const llm_request& request)
        -> cnetmod::task<std::expected<llm_completion,
            cnetboot::framework::error::error_code>> override;

    [[nodiscard]] auto stream(const llm_request& request, const llm_delta_sink& sink)
        -> cnetmod::task<std::expected<llm_completion,
            cnetboot::framework::error::error_code>> override;


    [[nodiscard]] auto directory() const noexcept
        -> const cnetboot::framework::config::llm_directory&;

private:

    struct binding
    {
        cnetboot::framework::config::llm_target target;
        cnetmod::ai::chat_model* model{};
    };

    [[nodiscard]] auto bind(const llm_request& request) const
        -> std::expected<binding, cnetboot::framework::error::error_code>;

    cnetboot::framework::config::llm_directory directory_;
    llm_model_table models_;
};


void add_llm_models(cnetmod::application::component_collection& components,
    const cnetboot::framework::config::llm_directory& directory);


[[nodiscard]] auto make_llm_service(cnetmod::application::component_resolver& resolver,
    cnetboot::framework::config::llm_directory directory)
    -> std::shared_ptr<composed_llm_service>;

} // namespace cnetboot::ai
