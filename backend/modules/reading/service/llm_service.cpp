module cnetnexus.reading.service.llm_service;

import std;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;

namespace cnetnexus::reading {

auto unavailable_llm_service::complete(const llm_request&)
    -> cnetmod::task<std::expected<llm_completion,
        cnetnexus::framework::error::error_code>>
{
    co_return std::unexpected{cnetnexus::framework::error::error_code::upstream_unavailable};
}

} // namespace cnetnexus::reading
