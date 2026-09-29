module cnetboot.ai.service.llm_service;

import std;
import cnetmod.coro.task;
import cnetboot.framework.error;
import cnetboot.ai.domain.llm_contract;

namespace cnetboot::ai {

auto unavailable_llm_service::complete(const llm_request&)
    -> cnetmod::task<std::expected<llm_completion,
        cnetboot::framework::error::error_code>>
{
    co_return std::unexpected{cnetboot::framework::error::error_code::upstream_unavailable};
}

} // namespace cnetboot::ai
