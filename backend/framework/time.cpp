module cnetboot.framework.time;

import std;

namespace cnetboot::framework {

auto iso8601_utc(std::int64_t seconds) -> std::string
{
    if (seconds <= 0)
        return {};
    const auto point =
        std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
    const auto day = std::chrono::floor<std::chrono::days>(point);
    const std::chrono::year_month_day calendar{day};
    const std::chrono::hh_mm_ss clock{point - day};
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z",
        static_cast<int>(calendar.year()),
        static_cast<unsigned>(calendar.month()),
        static_cast<unsigned>(calendar.day()),
        static_cast<long long>(clock.hours().count()),
        static_cast<long long>(clock.minutes().count()),
        static_cast<long long>(clock.seconds().count()));
}

} // namespace cnetboot::framework
