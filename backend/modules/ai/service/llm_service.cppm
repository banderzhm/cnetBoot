
export module cnetboot.ai.service.llm_service;

import std;
import cnetmod.coro.task;
import cnetboot.framework.error;
import cnetboot.ai.domain.llm_contract;

export namespace cnetboot::ai {


class llm_service
{
public:
    llm_service() = default;
    llm_service(const llm_service&) = delete;
    llm_service(llm_service&&) = delete;
    auto operator=(const llm_service&) -> llm_service& = delete;
    auto operator=(llm_service&&) -> llm_service& = delete;
    virtual ~llm_service() = default;


    [[nodiscard]] virtual auto configured() const noexcept -> bool
    {
        return true;
    }


    [[nodiscard]] virtual auto complete(const llm_request& request)
        -> cnetmod::task<std::expected<llm_completion,
            cnetboot::framework::error::error_code>> = 0;


    [[nodiscard]] virtual auto stream(const llm_request& request, const llm_delta_sink& sink)
        -> cnetmod::task<std::expected<llm_completion,
            cnetboot::framework::error::error_code>>
    {
        auto result = co_await complete(request);
        if (!result)
            co_return std::unexpected{result.error()};
        if (sink && !result->content.empty())
            (void)co_await sink(result->content);
        co_return *result;
    }
};


class unavailable_llm_service final : public llm_service
{
public:
    [[nodiscard]] auto configured() const noexcept -> bool override
    {
        return false;
    }

    [[nodiscard]] auto complete(const llm_request&)
        -> cnetmod::task<std::expected<llm_completion,
            cnetboot::framework::error::error_code>> override;
};

} // namespace cnetboot::ai
