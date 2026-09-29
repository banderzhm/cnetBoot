module cnetnexus.chart.domain.naming;

import std;

namespace {

/// 六个术数的规范标识；新增术数必须同时改 capabilities 与本表。
constexpr std::array k_algorithms{
    std::string_view{"ba_zi"}, std::string_view{"liu_yao"},
    std::string_view{"mei_hua"}, std::string_view{"zi_wei"},
    std::string_view{"da_liu_ren"}, std::string_view{"qi_men"}};

/// @brief 读取定长十进制数；位数不足或含非数字返回 nullopt。
auto digits(std::string_view text, std::size_t offset, std::size_t count)
    -> std::optional<int>
{
    if (offset + count > text.size())
        return std::nullopt;
    int value = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const char c = text[offset + i];
        if (c < '0' || c > '9')
            return std::nullopt;
        value = value * 10 + (c - '0');
    }
    return value;
}

} // namespace

namespace cnetnexus::chart {

auto algorithm_keys() noexcept -> std::span<const std::string_view>
{
    return k_algorithms;
}

auto canonical_algorithm(std::string_view raw) -> std::string
{
    // 输入别名：bazi 是排盘路径段口径，新接口统一回 ba_zi（契约 §8.1）。
    if (raw == "bazi")
        return "ba_zi";
    return std::string{raw};
}

auto is_known_algorithm(std::string_view raw) noexcept -> bool
{
    const auto canonical = raw == "bazi" ? std::string_view{"ba_zi"} : raw;
    return std::ranges::find(k_algorithms, canonical) != k_algorithms.end();
}

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

auto parse_iso8601(std::string_view text) -> std::optional<std::int64_t>
{
    // 只接受 UTC 口径：日期必填，时间与 Z/+00:00 可选，其它偏移视为非法。
    if (text.size() < 10)
        return std::nullopt;
    const auto year = digits(text, 0, 4);
    const auto month = digits(text, 5, 2);
    const auto day = digits(text, 8, 2);
    if (!year || !month || !day || text[4] != '-' || text[7] != '-')
        return std::nullopt;

    int hour = 0;
    int minute = 0;
    int second = 0;
    if (text.size() > 10)
    {
        if (text[10] != 'T' && text[10] != ' ')
            return std::nullopt;
        const auto parsed_hour = digits(text, 11, 2);
        const auto parsed_minute = digits(text, 14, 2);
        if (!parsed_hour || !parsed_minute || text[13] != ':')
            return std::nullopt;
        hour = *parsed_hour;
        minute = *parsed_minute;
        if (text.size() >= 19 && text[16] == ':')
        {
            const auto parsed_second = digits(text, 17, 2);
            if (!parsed_second)
                return std::nullopt;
            second = *parsed_second;
        }
        const auto zone = text.substr(19);
        if (!zone.empty() && zone != "Z" && zone != "z" && zone != "+00:00" &&
            zone != "-00:00")
            return std::nullopt;
    }

    const std::chrono::year_month_day calendar{
        std::chrono::year{*year},
        std::chrono::month{static_cast<unsigned>(*month)},
        std::chrono::day{static_cast<unsigned>(*day)}};
    if (!calendar.ok())
        return std::nullopt;
    const auto point = std::chrono::sys_days{calendar} +
        std::chrono::hours{hour} + std::chrono::minutes{minute} +
        std::chrono::seconds{second};
    return std::chrono::duration_cast<std::chrono::seconds>(
        point.time_since_epoch())
        .count();
}

auto utc_day_start(std::int64_t seconds) noexcept -> std::int64_t
{
    constexpr std::int64_t seconds_per_day = 24 * 60 * 60;
    // 负数（1970 之前）走向下取整，保证落在当天零点。
    const auto days = seconds / seconds_per_day -
        (seconds % seconds_per_day != 0 && seconds < 0 ? 1 : 0);
    return days * seconds_per_day;
}

} // namespace cnetnexus::chart
