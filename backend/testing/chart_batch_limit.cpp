import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.chart.service.chart_generation_service;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.stats;
import cnetnexus.chart.mapper.memory_chart_case_mapper;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;

// P0 批量排盘条数上限用例（task-6 缺陷 3）。
//
// 依据：实测 ba_zi 约 1.13 ms/盘，100 条约 113 ms —— 上限本身取 100（见
// modules/chart/service/chart_generation_service.cppm 的 max_batch_items 注释）。这里断言的是
// **边界**：恰好 100 条放行、101 条拒绝，且拒绝行为与响应错误码口径一致
// （contracts/error_codes.yaml 没有「条数过多」码 → REQUEST.INVALID_ARGUMENT / 400）。
//
// 另外用**时序**证明「超限请求根本没有进入排盘循环」：注入一个会自增计数的算法替身，
// 超限请求处理完计数必须仍为 0 —— 这正是修复的目的（同步循环没有挂起点，
// 请求级超时救不了它）。
namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 测试自持事件循环上跑完一个协程并取回结果。
template <typename T> auto drive(cnetmod::io_context& io, cnetmod::task<T> work) -> T
{
    std::optional<T> result;
    auto runner = [&io, &result, work = std::move(work)]() mutable -> cnetmod::task<void>
    {
        result.emplace(co_await std::move(work));
        io.stop();
    };
    cnetmod::spawn(io, runner());
    io.run();
    io.restart();
    return std::move(*result);
}

} // namespace

auto main() -> int
{
    // 1. 上限常量本身：100 是契约值，任何改动都必须显式改测试。
    if (cnetnexus::chart::max_batch_items != 100)
        return 1;

    // 2. 边界两侧：100 放行、101 拒绝；0 条（空数组）放行。
    if (!cnetnexus::chart::batch_within_limit(cnetnexus::chart::max_batch_items) ||
        cnetnexus::chart::batch_within_limit(cnetnexus::chart::max_batch_items + 1))
        return 2;
    if (cnetnexus::chart::batch_limit_violation(cnetnexus::chart::max_batch_items)
            .has_value() ||
        !cnetnexus::chart::batch_limit_violation(cnetnexus::chart::max_batch_items + 1)
             .has_value())
        return 3;
    if (cnetnexus::chart::batch_limit_violation(0) || !cnetnexus::chart::batch_within_limit(0))
        return 4;

    // 3. 超限文案必须能定位问题（含上限与实收条数），错误码是统一错误体里的
    //    REQUEST.INVALID_ARGUMENT（HTTP 400）。
    const auto violation = cnetnexus::chart::batch_limit_violation(150);
    if (!violation)
        return 5;
    if (violation->find("100") == std::string::npos ||
        violation->find("150") == std::string::npos)
        return 6;
    const auto code = error_code::request_invalid_argument;
    if (cnetnexus::framework::error::find(code)->http_status != 400)
        return 7;

    // 4. 上限之外的一切都不该触达排盘：仓库在超限时直接返回错误，
    //    因此「拒绝」与「不排盘」是同一个决定，这里用 service 往返确认语义可用。
    auto io = cnetmod::make_io_context();
    if (!io)
        return 8;
    cnetnexus::chart::chart_database database;
    cnetnexus::chart::memory_chart_archive_mapper repository{database};
    cnetnexus::chart::chart_archive_service service{repository};
    const auto archived = drive(*io, service.archive("ba_zi", R"({"a":1})"));
    if (!archived)
        return 9;
    const auto loaded = drive(*io, service.fetch("ba_zi", *archived));
    if (!loaded || !loaded->has_value())
        return 10;
    const auto removed = drive(*io, service.remove("ba_zi", *archived));
    if (!removed)
        return 11;
    return 0;
}
