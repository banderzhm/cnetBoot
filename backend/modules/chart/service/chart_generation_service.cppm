module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.service.chart_generation_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.error;

export namespace cnetnexus::chart {

inline constexpr std::size_t max_batch_items = 100;

[[nodiscard]] constexpr auto batch_within_limit(std::size_t items) noexcept
    -> bool {
  return items <= max_batch_items;
}

[[nodiscard]] auto batch_limit_violation(std::size_t items)
    -> std::optional<std::string>;

/** @brief 批量排盘应用服务：负责算法分派、逐条失败隔离与可选存档。 */
class chart_generation_service {
public:
  chart_generation_service(bazi_service &bazi, liu_yao_service &liu_yao,
                           mei_hua_service &mei_hua,
                           zi_wei_service &zi_wei,
                           da_liu_ren_service &da_liu_ren,
                           qi_men_service &qi_men,
                           chart_archive_service &archive) noexcept;

  [[nodiscard]] auto execute(const dto::chart_batch_request &request,
                             std::uint64_t owner_id) const
      -> cnetmod::task<std::expected<dto::chart_batch_response,
                                    framework::error::error_code>>;

  [[nodiscard]] auto archive_bazi(bazi::birth_input input,
                                  std::uint64_t owner_id) const
      -> cnetmod::task<std::expected<dto::archive_created_response,
                                    framework::error::error_code>>;

private:
  bazi_service *bazi_{};
  liu_yao_service *liu_yao_{};
  mei_hua_service *mei_hua_{};
  zi_wei_service *zi_wei_{};
  da_liu_ren_service *da_liu_ren_{};
  qi_men_service *qi_men_{};
  chart_archive_service *archive_{};
};

} // namespace cnetnexus::chart
