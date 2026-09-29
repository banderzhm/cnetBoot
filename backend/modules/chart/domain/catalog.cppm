export module cnetnexus.chart.domain.catalog;

import std;

export namespace cnetnexus::chart {

/** @brief 一个可对外暴露的算法能力。 */
struct algorithm_capability
{
    std::string_view key;          ///< 稳定算法标识，属于已发布契约，未经合同迁移不得改名
    std::string_view display_name; ///< 中文展示名
};

/** @brief 算法能力清单；顺序稳定，是本后端能力集合的唯一来源。 */
[[nodiscard]] auto capability_catalog() noexcept -> std::span<const algorithm_capability>;

} // namespace cnetnexus::chart
