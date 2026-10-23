export module cnetboot.framework.web.query;

import std;

export namespace cnetboot::framework::web::query {


[[nodiscard]] auto value(std::string_view query, std::string_view key) -> std::optional<std::string>;


[[nodiscard]] auto integer(std::string_view query, std::string_view key) -> std::optional<int>;


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
