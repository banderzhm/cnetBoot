import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.mapper.memory_chart_case_mapper;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.error;

namespace {

template <typename T>
auto drive(cnetmod::io_context& io, cnetmod::task<T> work) -> T
{
    std::optional<T> result;
    auto runner = [&io, &result, work = std::move(work)]() mutable
        -> cnetmod::task<void>
    {
        result.emplace(co_await std::move(work));
        io.stop();
    };
    cnetmod::spawn(io, runner());
    io.run();
    io.restart();
    return std::move(*result);
}

auto is_hex16(std::string_view value) -> bool
{
    return value.size() == 16 && std::ranges::all_of(value, [](char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

} // namespace

auto main() -> int
{
    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;

    cnetnexus::chart::chart_database database;
    cnetnexus::chart::memory_chart_archive_mapper repository{database};
    cnetnexus::chart::chart_archive_service service{repository};

    const auto first = drive(*io,
        service.archive("ba_zi", std::string{"{\"value\":1}"}, "first", 42));
    const auto second = drive(*io,
        service.archive("ba_zi", std::string{"{\"value\":2}"}, "second", 42));
    if (!first || !second || !is_hex16(*first) || !is_hex16(*second) ||
        *first == *second)
        return 2;

    const auto loaded = drive(*io, service.fetch("ba_zi", *first));
    if (!loaded || !loaded->has_value() || **loaded != "{\"value\":1}")
        return 3;

    cnetnexus::chart::archive_query query;
    query.algorithm = "ba_zi";
    const auto page = drive(*io, service.search(query));
    if (!page || page->total != 2)
        return 4;

    const auto removed = drive(*io, service.remove("ba_zi", *first));
    if (!removed)
        return 5;
    const auto missing = drive(*io, service.fetch("ba_zi", *first));
    if (!missing || missing->has_value())
        return 6;

    const auto empty = drive(*io, service.archive("ba_zi", std::string{}));
    if (empty || empty.error() !=
        cnetnexus::framework::error::error_code::request_invalid_argument)
        return 7;
    return 0;
}
