export module cnetnexus.chart.service.stats_service;

import std;
import cnetmod.coro;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.mapper.chart_archive_repository;
import cnetnexus.chart.mapper.chart_case_mapper;
import cnetnexus.chart.mapper.user_count_mapper;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

/** @brief Assembles the administrator overview counters.
 *
 *  The three data-plane mappers are coroutine mappers of this module. today.logins
 *  is not produced here: login audit rows belong to the identity module and chart
 *  must not import another module's storage contract, so the transport layer
 *  merges that one counter into the response.
 */
class stats_service
{
public:
    stats_service(user_count_mapper& users, chart_case_mapper& cases,
        chart_archive_repository& archives) noexcept
        : users_{&users},
          cases_{&cases},
          archives_{&archives}
    {
    }

    /** @brief Assembles the overview; any failed query fails the whole call
     *  instead of returning half-filled data. */
    [[nodiscard]] auto overview()
        -> cnetmod::task<std::expected<cnetnexus::chart::stats_overview,
            cnetnexus::framework::error::error_code>>;

private:
    user_count_mapper* users_;
    chart_case_mapper* cases_;
    chart_archive_repository* archives_;
};

} // namespace cnetnexus::chart
