import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.llm_service;
import cnetnexus.reading.service.reading_service;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录请求并返回固定文本的异步测试替身；只在测试内使用。
class fake_llm_service final : public cnetnexus::reading::llm_service
{
public:
    mutable int calls{0};
    mutable cnetnexus::reading::llm_request last{};

    auto complete(const cnetnexus::reading::llm_request& request)
        -> cnetmod::task<std::expected<cnetnexus::reading::llm_completion, error_code>> override
    {
        ++calls;
        last = request;
        co_return cnetnexus::reading::llm_completion{
            .content = "模型解读占位文本", .tokens_in = 11, .tokens_out = 22};
    }
};

/// @brief 断言失败的统一出口，避免在协程里直接 return。
struct probe_result
{
    bool ok{false};
    int code{1};
};

} // namespace

// reading_service 用例：未装配模型必须 503，装配后提示词必须到达补全实现。
//
// 迁移自 xuanxue-algorithm tests/reading_service_test.cpp，断言逐条保留：
//  - 玄学的 llm_port + set_llm(&port) 在 CNetNexus 里改为「构造时注入 llm_service&」，
//    因此第 2 段「service.llm() == &port」的断言改成「llm().configured() 为真」；
//  - 未装配模型 = 注入 unavailable_llm_service（框架的空实现），错误码仍为
//    DEPENDENCY.UNAVAILABLE，且不得触达补全实现。
auto main() -> int
{
    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;
    probe_result probe{};

    cnetnexus::reading::unavailable_llm_service unavailable;
    fake_llm_service configured;

    auto work = [&]() -> cnetmod::task<void>
    {
        const cnetnexus::reading::reading_input input{.algorithm = "ba_zi",
            .report_zh = "四柱：庚辰 壬午 甲辰 壬申",
            .question = "今年事业如何"};

        // 1. 未装配模型：返回 DEPENDENCY.UNAVAILABLE，不静默降级。
        const cnetnexus::reading::reading_service unconfigured{unavailable};
        const auto missing = co_await unconfigured.read(input);
        if (missing || missing.error() != error_code::upstream_unavailable)
        {
            probe = {false, 2};
            io->stop();
            co_return;
        }

        // 2. 装配模型：中文报告与问题必须原样进入提示词。
        cnetnexus::reading::reading_service service{configured};
        if (!service.llm().configured())
        {
            probe = {false, 3};
            io->stop();
            co_return;
        }

        const auto answer = co_await service.read(input);
        if (!answer || *answer != "模型解读占位文本" || configured.calls != 1)
        {
            probe = {false, 4};
            io->stop();
            co_return;
        }
        if (configured.last.messages.size() != 2 || configured.last.messages[0].role != "system")
        {
            probe = {false, 5};
            io->stop();
            co_return;
        }
        if (!configured.last.messages[1].content.contains("四柱：庚辰 壬午 甲辰 壬申") ||
            !configured.last.messages[1].content.contains("今年事业如何"))
        {
            probe = {false, 6};
            io->stop();
            co_return;
        }

        // 3. 报告为空：先拒绝，且不得触达模型。
        auto empty = input;
        empty.report_zh.clear();
        const auto rejected = co_await service.read(empty);
        if (rejected || rejected.error() != error_code::request_invalid_argument || configured.calls != 1)
        {
            probe = {false, 7};
            io->stop();
            co_return;
        }

        // 4. 流式：增量按序交给 sink，返回全文与计量；提示词与非流式逐字节一致。
        const auto non_stream_request = configured.last;
        {
            std::vector<std::string> deltas;
            auto sink = [&deltas](std::string_view delta) -> cnetmod::task<bool>
            {
                deltas.emplace_back(delta);
                co_return true;
            };
            const auto streamed = co_await service.read_stream(input, sink);
            if (!streamed || streamed->content != "模型解读占位文本" || streamed->tokens_in != 11 ||
                streamed->tokens_out != 22 || configured.calls != 2)
            {
                probe = {false, 8};
                io->stop();
                co_return;
            }
            // 假实现只实现了 complete：llm_service::stream 的默认实现退化成「整段单帧」，
            // 这里同时覆盖了不支持真流式的实现的兜底语义。
            if (deltas.size() != 1 || deltas[0] != "模型解读占位文本")
            {
                probe = {false, 9};
                io->stop();
                co_return;
            }
            if (configured.last.messages.size() != non_stream_request.messages.size() ||
                configured.last.messages[0].content != non_stream_request.messages[0].content ||
                configured.last.messages[1].content != non_stream_request.messages[1].content)
            {
                probe = {false, 10};
                io->stop();
                co_return;
            }
        }

        // 5. 流式同样先做入参校验：空报告 400，且不触达模型。
        {
            auto sink = [](std::string_view) -> cnetmod::task<bool>
            {
                co_return true;
            };
            const auto rejected_stream = co_await service.read_stream(empty, sink);
            if (rejected_stream || rejected_stream.error() != error_code::request_invalid_argument ||
                configured.calls != 2)
            {
                probe = {false, 11};
                io->stop();
                co_return;
            }
        }

        probe = {true, 0};
        io->stop();
        co_return;
    };

    cnetmod::spawn(*io, work());
    io->run();
    return probe.ok ? 0 : probe.code;
}
