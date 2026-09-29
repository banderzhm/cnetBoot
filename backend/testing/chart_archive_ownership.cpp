import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.mapper.memory_chart_case_mapper;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

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

auto user(std::uint64_t id) -> cnetnexus::chart::archive_access
{
    return {.enforce = true, .owner_id = id, .authenticated = true};
}

auto anonymous() -> cnetnexus::chart::archive_access
{
    return {.enforce = true};
}

auto administrator() -> cnetnexus::chart::archive_access
{
    return {.enforce = true, .owner_id = 1, .authenticated = true,
        .unrestricted = true};
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

    const auto owned = drive(*io,
        service.archive("ba_zi", std::string{"private"}, "owned", 42));
    const auto public_id = drive(*io,
        service.archive("ba_zi", std::string{"public"}, "public", 0));
    if (!owned || !public_id)
        return 2;

    const auto owner_read = drive(*io, service.fetch("ba_zi", *owned, user(42)));
    if (!owner_read || !owner_read->has_value())
        return 3;
    const auto foreign_read = drive(*io, service.fetch("ba_zi", *owned, user(7)));
    if (foreign_read || foreign_read.error() != error_code::auth_forbidden)
        return 4;
    const auto anonymous_read =
        drive(*io, service.fetch("ba_zi", *owned, anonymous()));
    if (anonymous_read ||
        anonymous_read.error() != error_code::auth_token_missing)
        return 5;
    const auto public_read =
        drive(*io, service.fetch("ba_zi", *public_id, anonymous()));
    if (!public_read || !public_read->has_value() || **public_read != "public")
        return 6;
    const auto admin_read =
        drive(*io, service.fetch("ba_zi", *owned, administrator()));
    if (!admin_read || !admin_read->has_value())
        return 7;

    const auto denied_delete =
        drive(*io, service.remove("ba_zi", *owned, user(7)));
    if (denied_delete || denied_delete.error() != error_code::auth_forbidden)
        return 8;
    if (!drive(*io, service.remove("ba_zi", *owned, user(42))))
        return 9;
    return 0;
}
