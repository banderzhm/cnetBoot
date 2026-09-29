export module cnetnexus.chart.mapper.user_count_mapper;

import std;
import cnetmod.coro;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief Counts registered users for the overview statistics.
 *
 *  Identity storage stays owned by the identity layer, so chart asks only for the
 *  one aggregate it needs instead of importing another module's storage contract.
 *  Implementations live where the user schema lives.
 */
class user_count_mapper
{
public:
    user_count_mapper() = default;
    user_count_mapper(const user_count_mapper&) = delete;
    auto operator=(const user_count_mapper&) -> user_count_mapper& = delete;
    user_count_mapper(user_count_mapper&&) = delete;
    auto operator=(user_count_mapper&&) -> user_count_mapper& = delete;
    virtual ~user_count_mapper() = default;

    /** @brief Returns the number of registered users. */
    [[nodiscard]] virtual auto count()
        -> cnetmod::task<std::expected<std::uint64_t,
            cnetnexus::framework::error::error_code>> = 0;
};

} // namespace cnetnexus::chart
