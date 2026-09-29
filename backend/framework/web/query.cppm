export module cnetboot.framework.web.query;

import std;

export namespace cnetboot::framework::web::query {

/** @brief 从 application/x-www-form-urlencoded 形式的 query string
 * 中取一个键的值。
 *  @param query 原始 query string（不含 '?'）
 *  @param key 键名
 *  @return 命中时返回百分号解码后的值（'+' 视为空格）；不存在返回 std::nullopt
 */
[[nodiscard]] auto value(std::string_view query, std::string_view key) -> std::optional<std::string>;

/** @brief 解析十进制整数参数；缺失、空值或含非数字返回 std::nullopt。 */
[[nodiscard]] auto integer(std::string_view query, std::string_view key) -> std::optional<int>;

/** @brief 解析逗号分隔的整数列表；缺失返回空列表，出现非法元素返回
 * std::nullopt。 */
[[nodiscard]] auto integer_list(std::string_view query, std::string_view key) -> std::optional<std::vector<int>>;

/**
 * @brief Parses an unsigned decimal route identifier.
 *
 * @param raw Route parameter text.
 * @return The positive identifier, or std::nullopt when the text is empty,
 * malformed, zero, or outside the uint64_t range.
 */
[[nodiscard]] auto path_id(std::string_view raw) -> std::optional<std::uint64_t>;

} // namespace cnetboot::framework::web::query
