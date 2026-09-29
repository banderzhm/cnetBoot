module cnetnexus.chart.domain.catalog;

import std;

namespace {

/// 能力清单顺序即 /api/v1/capabilities 的输出顺序，新增能力追加在末尾。
constexpr std::array k_catalog{
    cnetnexus::chart::algorithm_capability{"ba_zi", "八字"},
    cnetnexus::chart::algorithm_capability{"liu_yao", "六爻"},
    cnetnexus::chart::algorithm_capability{"mei_hua", "梅花"},
    cnetnexus::chart::algorithm_capability{"da_liu_ren", "大六壬"},
    cnetnexus::chart::algorithm_capability{"qi_men", "奇门"},
    cnetnexus::chart::algorithm_capability{"zi_wei", "紫微"},
};

} // namespace

namespace cnetnexus::chart {

auto capability_catalog() noexcept -> std::span<const algorithm_capability>
{
    return k_catalog;
}

} // namespace cnetnexus::chart
