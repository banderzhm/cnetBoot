/**
 * @brief 断语缓存（reading_cache 表）的 MySQL 映射器与 XML 语句校验。
 *
 * 读路径走 ORM query_wrapper（列清单由模型生成）；XML
 * 只保留构建器表达不了的两条： 唯一键 UPSERT（INSERT ... ON DUPLICATE KEY
 * UPDATE）与命中计数列自增。 时间列收 UTC 墙钟（边界从 unix 秒换算后以 DATETIME
 * 参数绑定，不依赖会话时区）。 唯一键是 (user_id, algorithm, chart_fingerprint,
 * question_hash)，与建表脚本逐列一致。
 */
export module cnetnexus.reading.mapper.reading_cache_mapper;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_cache_record;

export namespace cnetnexus::reading {

namespace error = cnetnexus::framework::error;

/** @brief 断语缓存的键：用户 + 算法 + 盘面指纹 +
 * 所问指纹（与表的唯一键逐列一致）。 */
struct reading_cache_key {
  std::uint64_t user_id{0};      ///< 登录用户 id；0 表示匿名（未带有效 Bearer）
  std::string algorithm;         ///< 规范术数标识
  std::string chart_fingerprint; ///< 盘面指纹（sha256 hex）
  std::string question_hash;     ///< 所问指纹（sha256 hex）
};

/** @brief 断语缓存的一行（键之外的全部列）。 */
struct reading_cache_row {
  std::string question;       ///< 问句原文（排查与展示用）
  std::string content;        ///< 断语正文
  std::string model;          ///< 生成时使用的模型名；可能为空
  int tokens_in{0};           ///< 生成时的输入 token；流式路径通常为 0
  int tokens_out{0};          ///< 生成时的输出 token；流式路径通常为 0
  std::string engine_version; ///< 生成时的引擎版本；读取方据此判定旧缓存失效
  std::int64_t hit_count{0};  ///< 该行已被复用的次数
};

/** @brief 断语缓存的 MySQL 映射器。
 *
 *  与用例层（reading_cache_service）的分工：
 *  - find **不做** engine_version
 * 判定：版本口径集中在用例层，映射器只按唯一键取行；
 *  - upsert 按唯一键覆盖同键内容与计量（单语句原子，后写覆盖先写）；
 *  - record_hit 只做命中计数 + 1 与命中时间刷新（单语句列自增，不做先读后写）。
 */
class reading_cache_mapper {
public:
  virtual ~reading_cache_mapper() = default;

  [[nodiscard]] virtual auto
  find(const reading_cache_key &key) -> cnetmod::task<
      std::expected<std::optional<reading_cache_row>, error::error_code>> = 0;
  [[nodiscard]] virtual auto upsert(const reading_cache_key &key,
                                    const reading_cache_row &row)
      -> cnetmod::task<std::expected<void, error::error_code>> = 0;
  [[nodiscard]] virtual auto record_hit(const reading_cache_key &key)
      -> cnetmod::task<std::expected<void, error::error_code>> = 0;
};

class mysql_reading_cache_mapper final : public reading_cache_mapper {
public:
  mysql_reading_cache_mapper(
      cnetmod::application::managed_repository<reading_cache_record> cache,
      std::shared_ptr<const cnetmod::orm::mapper_registry> registry);

  /** @brief 按唯一键取一行；不存在返回 std::nullopt。 */
  [[nodiscard]] auto find(const reading_cache_key &key)
      -> cnetmod::task<std::expected<std::optional<reading_cache_row>,
                                     error::error_code>> override;

  /** @brief 写入或覆盖同键行（唯一键 UPSERT），并更新 updated_at。 */
  [[nodiscard]] auto upsert(const reading_cache_key &key,
                            const reading_cache_row &row)
      -> cnetmod::task<std::expected<void, error::error_code>> override;

  /** @brief 命中计数 +1 并刷新命中时间。 */
  [[nodiscard]] auto record_hit(const reading_cache_key &key)
      -> cnetmod::task<std::expected<void, error::error_code>> override;

private:
  cnetmod::application::managed_repository<reading_cache_record> cache_;
  std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
};

/** @brief 启动期校验断语缓存 XML 的必需语句（装载由统一的 mapper registry
 * 服务负责）。 */
[[nodiscard]] auto validate_reading_cache_mapper_registry(
    const cnetmod::orm::mapper_registry &registry)
    -> std::expected<void, std::string>;

} // namespace cnetnexus::reading
