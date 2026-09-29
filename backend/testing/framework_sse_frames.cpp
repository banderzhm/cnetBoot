#include <cnetmod/json_codec.hpp>

import std;
import cnetmod.core;
import cnetmod.executor;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.protocol.http;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.sse_stream;

// SSE 线字节契约用例（framework_sse_frames）：线上帧字节是已冻结的前端契约，
// 任何一格变化都必须显式改这个用例。
//
// 迁移自 xuanxue-algorithm tests/sse_frames_test.cpp。**替代实现说明**：原用例经
// reading_responder / chat_routes 驱动，那些 transport 尚未迁入 CNetNexus（task-3），
// 因此这里改为直接驱动本仓已迁移的 cnetnexus.framework.web.sse_stream::sse_writer，
// 走真实 loopback TCP 抓线上字节。断言逐条等价：框架编码器冻结样本、SSE 响应头、
// delta/done/error 帧字节、started() 惰性开始判据、无名终止帧。
//
// 与玄学用例的覆盖差异（明确记录，不是静默放宽）：
//  - 原第 3 段「断语缓存命中只出一帧 done」的 reading 载荷形状由 task-3 的读取路由
//    用例覆盖（载荷构造在 reading_responder 里，目前不存在）；
//  - 原第 4 段「首帧之前失败回退普通 JSON」同理依赖 reading_responder；这里改为
//    直接断言 sse_writer 一帧未写时 started() == false（回退判据本身）。

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 冻结的 SSE 响应头样本（\r\n 行尾）；字段与顺序来自框架 http::sse::prepare。
constexpr std::string_view k_sse_head = "HTTP/1.1 200 OK\r\n" "Cache-Control: no-cache, no-transform\r\n" "Connection: keep-alive\r\n" "Content-Type: text/event-stream; charset=utf-8\r\n" "Transfer-Encoding: chunked\r\n" "X-Accel-Buffering: no\r\n" "X-Streamed: 1\r\n" "\r\n";

/// @brief 冻结的无名终止帧（框架 http::sse::done）：连接按响应头的 keep-alive 保持，没有心跳帧。
constexpr std::string_view k_terminal_frame = "data: {\"done\":true}\n\n";

/// @brief chat 形状的一次成功流：两层 delta + done（调用方构造载荷）+ 终止帧。
constexpr std::string_view k_chat_frames =
    "event: delta\ndata: {\"text\":\"增量\"}\n\n" "event: delta\ndata: {\"text\":\"，甲木\"}\n\n" "event: done\ndata: " "{\"messageId\":42,\"content\":\"增量，甲木\",\"tokensIn\":3,\"tokensOut\":5}\n\n" "data: {\"done\":true}\n\n";

/// @brief 断语形状的中途失败流：delta → error（统一错误体）→ 终止帧。
constexpr std::string_view k_error_frames =
    "event: delta\ndata: {\"text\":\"事业\"}\n\n" "event: error\ndata: " "{\"code\":\"422\",\"status\":\"ALGORITHM.FAILED\",\"message\":\"算法无法处理当前输入\",\"retryable\":false}\n\n" "data: {\"done\":true}\n\n";

/// @brief 框架编码器与载荷函数的冻结样本。
constexpr std::string_view k_delta_frame = "event: delta\ndata: {\"text\":\"增量\"}\n\n";
constexpr std::string_view k_error_payload =
    R"({"code":"422","status":"ALGORITHM.FAILED","message":"算法无法处理当前输入","retryable":false})";
constexpr std::string_view k_unavailable_body =
    R"({"code":"503","status":"DEPENDENCY.UNAVAILABLE","message":"上游依赖暂不可用，请稍后重试","retryable":true})";

/// @brief 断言行收集器：总数 / 失败项分明，失败时把期望与实际（转义后）一起打出来。
class assertions
{
public:
    void that(bool condition, std::string_view label, std::string_view actual = {}, std::string_view expected = {})
    {
        ++total_;
        if (condition)
            return;
        ++failures_;
        std::cout << std::format("  [FAIL] {}\n", label);
        if (!expected.empty() || !actual.empty())
            std::cout << std::format("    expected=[{}]\n    actual  =[{}]\n", escape(expected), escape(actual));
    }

    [[nodiscard]] auto failed() const noexcept -> std::size_t
    {
        return failures_;
    }

    void report() const
    {
        std::cout << std::format("framework_sse_frames: 断言行 {} 条，失败 {} 条", total_, failures_) << std::endl;
    }

private:
    static auto escape(std::string_view text) -> std::string
    {
        std::string out;
        out.reserve(text.size());
        for (const char ch : text)
        {
            if (ch == '\n')
                out += "\\n";
            else if (ch == '\r')
                out += "\\r";
            else if (ch == '\t')
                out += "\\t";
            else
                out += ch;
        }
        return out;
    }

    std::size_t total_{0};
    std::size_t failures_{0};
};

/// @brief 一次处理器调用的观察结果：response 对象 + 从真实 socket 上读回的原始字节。
struct exchange
{
    int status{};
    std::string head;
    std::string frames;
    bool streamed{false};
    bool completed{false};
    std::string stage{"init"};
};

/// @brief 在真实 loopback TCP 上跑一次处理器，把线上原始字节整段读回来。
auto call_handler(cnetmod::io_context& io, cnetmod::socket& listener, std::uint16_t port, std::string_view uri,
    cnetmod::http::handler_fn handler) -> cnetmod::task<exchange>
{
    exchange result;
    auto client = cnetmod::socket::create(cnetmod::address_family::ipv4, cnetmod::socket_type::stream);
    result.stage = "created";
    if (!client)
        co_return result;

    std::optional<cnetmod::socket> accepted;
    bool accepted_ready = false;
    auto acceptor = [&]() -> cnetmod::task<void>
    {
        auto socket = co_await cnetmod::async_accept(io, listener);
        if (socket)
            accepted.emplace(std::move(*socket));
        accepted_ready = true;
        co_return;
    };
    cnetmod::spawn(io, acceptor());

    result.stage = "connecting";
    const auto connected =
        co_await cnetmod::async_connect(io, *client, cnetmod::endpoint{cnetmod::ipv4_address::loopback(), port});
    if (!connected)
        co_return result;
    result.stage = "accepting";
    while (!accepted_ready)
        co_await cnetmod::post_awaitable{io};
    result.stage = "accepted";
    if (!accepted)
        co_return result;

    cnetmod::http::header_map headers;
    cnetmod::http::response response;
    cnetmod::http::route_params params;
    cnetmod::http::request_context context{io, *accepted, "GET", uri, headers, "", response, params};
    result.stage = "handling";
    co_await handler(context);

    result.stage = "handled";
    result.status = response.status_code();
    result.streamed = response.get_header("X-Streamed") == "1";
    if (!result.streamed)
        co_return result;

    result.stage = "reading";
    std::array<std::byte, 8192> buffer{};
    while (!result.frames.ends_with("0\r\n\r\n"))
    {
        auto received =
            co_await cnetmod::async_read(io, *client, cnetmod::mutable_buffer{buffer.data(), buffer.size()});
        if (!received || *received == 0)
            break;
        result.frames.append(reinterpret_cast<const char*>(buffer.data()), *received);
    }
    if (const auto split = result.frames.find("\r\n\r\n"); split != std::string::npos)
    {
        result.head = result.frames.substr(0, split + 4);
        result.frames = result.frames.substr(split + 4);
    }
    // 分块传输解码：只保留 chunk 数据（与原用例的 http_wire 支持模块同一口径）。
    std::string decoded;
    std::size_t position = 0;
    while (position < result.frames.size())
    {
        const auto line_end = result.frames.find("\r\n", position);
        if (line_end == std::string::npos)
            break;
        std::size_t chunk_size = 0;
        const auto size_text = std::string_view{result.frames}.substr(position, line_end - position);
        const auto parsed = std::from_chars(size_text.data(), size_text.data() + size_text.size(), chunk_size, 16);
        if (parsed.ec != std::errc{} || chunk_size == 0)
            break;
        const auto data_start = line_end + 2;
        if (data_start + chunk_size > result.frames.size())
            break;
        decoded.append(result.frames, data_start, chunk_size);
        position = data_start + chunk_size + 2;
    }
    result.frames = std::move(decoded);
    result.completed = result.frames.ends_with(k_terminal_frame);
    result.stage = "read";
    co_return result;
}

/// @brief 墙钟看门狗：异步路径若挂死，_Exit 退出码 2（不会被 catch 吞掉），不把 CTest 挂住。
struct test_watchdog
{
    std::jthread thread;

    explicit test_watchdog(int seconds)
        : thread{[seconds](std::stop_token token)
              {
                  for (int tick = 0; tick < seconds * 10; ++tick)
                  {
                      if (token.stop_requested())
                          return;
                      std::this_thread::sleep_for(std::chrono::milliseconds{100});
                  }
                  std::cout << std::format("framework_sse_frames 用例超时 {}s：异步路径疑似挂死", seconds) << std::endl;
                  std::_Exit(2);
              }}
    {
    }
};

} // namespace

/// @brief SSE 线字节用例：帧/响应头/终止帧逐字节冻结，外加上 started() 惰性判据。
auto main() -> int
{
    test_watchdog watchdog{30};
    assertions checks;

    // 0. 框架编码器与载荷函数的冻结样本：期望值是字面量，不用被测代码反推。
    const auto delta_frame = cnetmod::http::sse::data("{\"text\":\"增量\"}", "delta");
    checks.that(delta_frame == k_delta_frame, "框架 sse::data 编码具名事件帧", delta_frame, k_delta_frame);
    const auto terminal_frame = cnetmod::http::sse::done();
    checks.that(terminal_frame == k_terminal_frame, "框架 sse::done 编码无名终止帧", terminal_frame, k_terminal_frame);
    const auto heartbeat_frame = cnetmod::http::sse::heartbeat();
    checks.that(heartbeat_frame == ": keepalive\n\n", "框架 sse::heartbeat 是注释帧（request_context 无出口）",
        heartbeat_frame, ": keepalive\n\n");
    const auto delta_payload = cnetnexus::framework::web::sse_delta_payload("增量");
    checks.that(delta_payload == R"({"text":"增量"})", "sse_delta_payload 只出纯 JSON 载荷（编码留给框架）",
        delta_payload, R"({"text":"增量"})");
    const auto error_payload = cnetnexus::framework::web::sse_error_payload(error_code::algorithm_failed);
    checks.that(error_payload == k_error_payload, "sse_error_payload 只出统一错误体载荷（编码留给框架）",
        error_payload, k_error_payload);

    // ?stream= 开关口径（原来属于 transport，现在归 framework.web）。
    checks.that(cnetnexus::framework::web::sse_requested("") == std::expected<bool, error_code>{false},
        "sse_requested：缺省 = 非流式");
    checks.that(cnetnexus::framework::web::sse_requested("stream=true") == std::expected<bool, error_code>{true},
        "sse_requested：stream=true = 流式");
    checks.that(cnetnexus::framework::web::sse_requested("stream=1") == std::expected<bool, error_code>{true},
        "sse_requested：stream=1 = 流式");
    checks.that(!cnetnexus::framework::web::sse_requested("stream=maybe").has_value(),
        "sse_requested：非法取值 400（不静默当 false）");
    checks.that(cnetnexus::framework::web::error_body_json(error_code::upstream_unavailable) == k_unavailable_body,
        "统一错误体 JSON 逐字节一致",
        cnetnexus::framework::web::error_body_json(error_code::upstream_unavailable), k_unavailable_body);

    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;

    int result = 0;
    auto work = [&]() -> cnetmod::task<void>
    {
        auto listener = cnetmod::socket::create(cnetmod::address_family::ipv4, cnetmod::socket_type::stream);
        if (!listener || !listener->bind(cnetmod::endpoint{cnetmod::ipv4_address::loopback(), 0}) || !listener->listen())
        {
            result = 2;
            io->stop();
            co_return;
        }
        const auto port = listener->local_endpoint()->port();

        // 1. chat 形状：begin()（显式开始）→ delta ×2 → done → finish()。
        {
            bool started_before{true};
            bool started_after_begin{false};
            bool started_after_done{false};
            const auto handler = [&](cnetmod::http::request_context& context) -> cnetmod::task<void>
            {
                co_await context.with_sse([&](cnetmod::http::request_context&,
                    cnetmod::http::sse_stream& stream) -> cnetmod::task<void>
                {
                    cnetnexus::framework::web::sse_writer writer{stream};
                    started_before = writer.started();
                    if (!co_await writer.begin())
                        co_return;
                    started_after_begin = writer.started();
                    (void)co_await writer.delta("增量");
                    (void)co_await writer.delta("，甲木");
                    (void)co_await writer.done(
                        R"({"messageId":42,"content":"增量，甲木","tokensIn":3,"tokensOut":5})");
                    started_after_done = writer.started();
                    (void)co_await writer.finish();
                });
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/chat", handler);
            checks.that(got.stage == "read", "chat：整条流读完（stage=read）", got.stage, "read");
            checks.that(!started_before, "chat：首帧写出之前 started() == false");
            checks.that(started_after_begin, "chat：begin() 之后 started() == true");
            checks.that(started_after_done, "chat：done 帧之后 started() 仍为 true");
            checks.that(got.status == 200, "chat：状态码 200", std::to_string(got.status), "200");
            checks.that(got.head == k_sse_head, "chat：SSE 响应头逐字节一致", got.head, k_sse_head);
            checks.that(got.completed, "chat：以无名终止帧收尾");
            checks.that(got.frames == k_chat_frames, "chat：帧字节逐字节一致", got.frames, k_chat_frames);
        }

        // 2. 中途失败：delta 之后失败 → error 帧 + 终止帧，不重放已下发的增量。
        {
            const auto handler = [&](cnetmod::http::request_context& context) -> cnetmod::task<void>
            {
                co_await context.with_sse([](cnetmod::http::request_context&,
                    cnetmod::http::sse_stream& stream) -> cnetmod::task<void>
                {
                    cnetnexus::framework::web::sse_writer writer{stream};
                    (void)co_await writer.delta("事业");
                    (void)co_await writer.error(error_code::algorithm_failed);
                    (void)co_await writer.finish();
                });
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/error", handler);
            checks.that(got.head == k_sse_head, "error：SSE 响应头逐字节一致", got.head, k_sse_head);
            checks.that(got.completed, "error：以无名终止帧收尾");
            checks.that(got.frames == k_error_frames, "error：帧字节逐字节一致（delta → error → 终止帧）",
                got.frames, k_error_frames);
        }

        // 3. 惰性开始：一帧未写时不带 X-Streamed（回退普通 HTTP 判据仍然成立）。
        {
            bool started_before{true};
            const auto handler = [&](cnetmod::http::request_context&) -> cnetmod::task<void>
            {
                co_return;
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/none", handler);
            (void)started_before;
            checks.that(!got.streamed, "未写帧：响应没有 X-Streamed");
            checks.that(!got.completed, "未写帧：没有终止帧");
        }

        result = 0;
        io->stop();
        co_return;
    };

    cnetmod::spawn(*io, work());
    io->run();
    checks.report();
    if (checks.failed() != 0)
        return 1;
    return result;
}
