
export module cnetboot.ai.domain.llm_contract;

import std;
import cnetmod.coro.task;

export namespace cnetboot::ai {


struct llm_message
{
    std::string role;    ///< system / user / assistant
    std::string content;
};


struct llm_request
{
    std::vector<llm_message> messages;
    std::string provider;
    std::string model;
    double temperature{0.2};
    int max_tokens{1024};
    std::string reasoning_effort;
};


struct llm_completion
{
    std::string content;
    int tokens_in{};
    int tokens_out{};
};


using llm_delta_sink = std::function<cnetmod::task<bool>(std::string_view)>;

} // namespace cnetboot::ai
