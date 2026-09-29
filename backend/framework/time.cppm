/** @brief UTC formatting helpers shared by transport projections. */
export module cnetboot.framework.time;

import std;

export namespace cnetboot::framework {

[[nodiscard]] auto iso8601_utc(std::int64_t seconds) -> std::string;

} // namespace cnetboot::framework
