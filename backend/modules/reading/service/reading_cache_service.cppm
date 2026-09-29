/**
 * @brief 断语缓存用例：键计算、引擎版本失效判定与命中计数。
 *
 * 与 transport
 * 的分工：调用方只问「有没有命中」与「回填这一条」，键的规范化、版本口径
 * 与计数副作用都收在这里（唯一实现）。
 */
export module cnetnexus.reading.service.reading_cache_service;

import std;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.reading.mapper.reading_cache_mapper;

export namespace cnetnexus::reading {

namespace error = cnetnexus::framework::error;

/** @brief 一次断语缓存访问的输入：调用方只提供这些，键与版本由用例层统一计算。
 */
struct reading_cache_request {
  std::uint64_t user_id{0}; ///< 登录用户 id；0 表示匿名
  std::string algorithm;    ///< 规范术数标识

  /** @brief 规范化生成范围串（reading_scope_selection::canonical()，缺省
   * benming）； 并进盘面指纹，因此同一盘面选不同范围不会互相命中。 */
  std::string scope{"benming"};

  std::string report_zh; ///< 确定性中文报告（盘面指纹的输入）
  std::string question;  ///< 用户问题（所问指纹的输入）
};

/** @brief 一次缓存命中：断语正文与计量；不含键与 engine_version。 */
struct reading_cache_hit {
  std::string content; ///< 断语正文
  std::string model;   ///< 生成时使用的模型名
  int tokens_in{0};    ///< 输入 token（流式路径通常为 0）
  int tokens_out{0};   ///< 输出 token（流式路径通常为 0）
};

/** @brief 断语缓存用例：键计算、engine_version 失效判定与命中计数（全协程）。
 */
class reading_cache_service {
public:
  explicit reading_cache_service(reading_cache_mapper &mapper) noexcept
      : mapper_{&mapper} {}

  reading_cache_service(const reading_cache_service &) = delete;
  reading_cache_service(reading_cache_service &&) = delete;

  /** @brief 查缓存。
   *  @return 命中返回条目；未命中返回
   * std::nullopt；映射器故障返回错误（调用方按未命中继续）
   *  @note 命中时把命中计数 +1 落库；这一步失败只记日志，不影响本次返回。 */
  [[nodiscard]] auto lookup(const reading_cache_request &request)
      -> cnetmod::task<
          std::expected<std::optional<reading_cache_hit>, error::error_code>>;

  /** @brief 生成成功后回填（唯一键 UPSERT，覆盖同键内容与计量）。 */
  [[nodiscard]] auto save(const reading_cache_request &request,
                          const reading_cache_hit &hit)
      -> cnetmod::task<std::expected<void, error::error_code>>;

private:
  reading_cache_mapper *mapper_;
};

} // namespace cnetnexus::reading
