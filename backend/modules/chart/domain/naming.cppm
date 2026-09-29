export module cnetnexus.chart.domain.naming;

import std;

export namespace cnetnexus::chart {

/** @brief 规范术数标识清单；顺序与 /api/v1/capabilities 一致。
 *  命理师 API（/charts/batch、/cases、/tags、/stats/overview、/archive/search）
 *  一律使用这里的取值，与存档目录、统计 key 同口径（契约 §8.1）。
 */
[[nodiscard]] auto algorithm_keys() noexcept
    -> std::span<const std::string_view>;

/** @brief 把请求里的术数标识归一成规范值。
 *  输入侧兼容别名 bazi →
 * ba_zi；其余五个标识无别名；未知值原样返回（由调用方判定是否支持）。
 */
[[nodiscard]] auto canonical_algorithm(std::string_view raw) -> std::string;

/** @brief raw 归一后是否属于六个已登记术数之一。 */
[[nodiscard]] auto is_known_algorithm(std::string_view raw) noexcept -> bool;

/** @brief ISO-8601 UTC 文本（2026-01-02T03:04:05Z）；0 返回空串。 */
[[nodiscard]] auto iso8601_utc(std::int64_t seconds) -> std::string;

/** @brief 解析 ISO-8601 文本（接受 YYYY-MM-DD、YYYY-MM-DDTHH:MM:SSZ 与 +00:00
 * 偏移）。
 *  @return 解析成功返回 UTC 秒；非法输入返回 std::nullopt
 */
[[nodiscard]] auto parse_iso8601(std::string_view text)
    -> std::optional<std::int64_t>;

/** @brief 给定时刻所在 UTC 日的零点（UTC 秒）。 */
[[nodiscard]] auto utc_day_start(std::int64_t seconds) noexcept -> std::int64_t;

} // namespace cnetnexus::chart
