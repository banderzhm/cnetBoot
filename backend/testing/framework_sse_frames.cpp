#include <cnetmod/json_codec.hpp>

import std;
import cnetmod.core;
import cnetmod.executor;
import cnetmod.coro;
import cnetmod.io;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.sse_stream;

// Byte-level SSE contract test. It drives sse_writer over a real loopback TCP
// connection and freezes headers, frame order, lazy start, and termination.

namespace {

using error_code = cnetboot::framework::error::error_code;

/// Frozen SSE response headers produced by http::sse::prepare.
constexpr std::string_view k_sse_head = "HTTP/1.1 200 OK\r\n" "Content-Type: text/event-stream; charset=utf-8\r\n" "Connection: keep-alive\r\n" "Transfer-Encoding: chunked\r\n" "X-Streamed: 1\r\n" "Cache-Control: no-cache, no-transform\r\n" "X-Accel-Buffering: no\r\n" "\r\n";

/// Terminal frame produced by http::sse::done.
constexpr std::string_view k_terminal_frame = "data: {\"done\":true}\n\n";

/// Successful chat stream with two deltas, a done payload, and termination.
constexpr std::string_view k_chat_frames =
    "event: delta\ndata: {\"text\":\"first\"}\n\n" "event: delta\ndata: {\"text\":\" response\"}\n\n" "event: done\ndata: " "{\"messageId\":42,\"content\":\"first response\",\"tokensIn\":3,\"tokensOut\":5}\n\n" "data: {\"done\":true}\n\n";

/// Mid-stream failure with a delta, canonical error, and termination.
constexpr std::string_view k_error_frames =
    "event: delta\ndata: {\"text\":\"partial\"}\n\n" "event: error\ndata: " "{\"code\":\"422\",\"message\":\"AI could not process the request\",\"retryable\":false,\"status\":\"AI.REQUEST_FAILED\"}\n\n" "data: {\"done\":true}\n\n";

/// Frozen encoder and payload samples.
constexpr std::string_view k_delta_frame = "event: delta\ndata: {\"text\":\"first\"}\n\n";
constexpr std::string_view k_error_payload =
    R"({"code":"422","message":"AI could not process the request","retryable":false,"status":"AI.REQUEST_FAILED"})";
constexpr std::string_view k_unavailable_body =
    R"({"code":"503","message":"An upstream dependency is temporarily unavailable","retryable":true,"status":"DEPENDENCY.UNAVAILABLE"})";

class assertions
{
public:
    void that(bool condition, std::string_view label, std::string_view actual = {}, std::string_view expected = {})
    {
        ++total_;
        if (condition)
            return;
        ++failures_;
        logger::error{"FAILED: {}", label};
        if (!expected.empty() || !actual.empty())
            logger::error{"expected=[{}] actual=[{}]", escape(expected), escape(actual)};
    }

    [[nodiscard]] auto failed() const noexcept -> std::size_t
    {
        return failures_;
    }

    void report() const
    {
        logger::info{"framework_sse_frames: {} checks, {} failures", total_, failures_};
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


struct exchange
{
    int status{};
    std::string head;
    std::string frames;
    bool streamed{false};
    bool completed{false};
    std::string stage{"init"};
};


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

    result.stage = "ai";
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
                  logger::error{"framework_sse_frames timed out after {}s; asynchronous path may be stalled", seconds};
                  std::_Exit(2);
              }}
    {
    }
};

} // namespace


auto main() -> int
{
    test_watchdog watchdog{30};
    assertions checks;
    cnetmod::net_init net;


    const auto delta_frame = cnetmod::http::sse::data("{\"text\":\"first\"}", "delta");
    checks.that(delta_frame == k_delta_frame, "sse::data encodes a named event", delta_frame, k_delta_frame);
    const auto terminal_frame = cnetmod::http::sse::done();
    checks.that(terminal_frame == k_terminal_frame, "sse::done encodes the terminal event", terminal_frame, k_terminal_frame);
    const auto heartbeat_frame = cnetmod::http::sse::heartbeat();
    checks.that(heartbeat_frame == ": keepalive\n\n", "sse::heartbeat is a comment frame",
        heartbeat_frame, ": keepalive\n\n");
    const auto delta_payload = cnetboot::framework::web::sse_delta_payload("first");
    checks.that(delta_payload == R"({"text":"first"})", "sse_delta_payload returns JSON payload only",
        delta_payload, R"({"text":"first"})");
    const auto error_payload = cnetboot::framework::web::sse_error_payload(error_code::ai_request_failed);
    checks.that(error_payload == k_error_payload, "sse_error_payload returns the canonical error payload",
        error_payload, k_error_payload);


    checks.that(cnetboot::framework::web::sse_requested("") == std::expected<bool, error_code>{false},
        "sse_requested defaults to non-streaming");
    checks.that(cnetboot::framework::web::sse_requested("stream=true") == std::expected<bool, error_code>{true},
        "sse_requested accepts stream=true");
    checks.that(cnetboot::framework::web::sse_requested("stream=1") == std::expected<bool, error_code>{true},
        "sse_requested accepts stream=1");
    checks.that(!cnetboot::framework::web::sse_requested("stream=maybe").has_value(),
        "sse_requested rejects invalid values with 400");
    checks.that(cnetboot::framework::web::error_body_json(error_code::upstream_unavailable) == k_unavailable_body,
        "canonical error JSON is byte-exact",
        cnetboot::framework::web::error_body_json(error_code::upstream_unavailable), k_unavailable_body);

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


        {
            bool started_before{true};
            bool started_after_begin{false};
            bool started_after_done{false};
            const auto handler = [&](cnetmod::http::request_context& context) -> cnetmod::task<void>
            {
                co_await context.with_sse([&](cnetmod::http::request_context&,
                    cnetmod::http::sse_stream& stream) -> cnetmod::task<void>
                {
                    cnetboot::framework::web::sse_writer writer{stream};
                    started_before = writer.started();
                    if (!co_await writer.begin())
                        co_return;
                    started_after_begin = writer.started();
                    (void)co_await writer.delta("first");
                    (void)co_await writer.delta(" response");
                    (void)co_await writer.done(
                        R"({"messageId":42,"content":"first response","tokensIn":3,"tokensOut":5})");
                    started_after_done = writer.started();
                    (void)co_await writer.finish();
                });
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/chat", handler);
            checks.that(got.stage == "read", "chat stream is read completely", got.stage, "read");
            checks.that(!started_before, "chat stream is not started before the first frame");
            checks.that(started_after_begin, "chat stream is started after begin()");
            checks.that(started_after_done, "chat stream remains started after done()");
            checks.that(got.status == 200, "chat stream returns status 200", std::to_string(got.status), "200");
            checks.that(got.head == k_sse_head, "chat SSE headers are byte-exact", got.head, k_sse_head);
            checks.that(got.completed, "chat stream ends with the terminal frame");
            checks.that(got.frames == k_chat_frames, "chat frames are byte-exact", got.frames, k_chat_frames);
        }


        {
            const auto handler = [&](cnetmod::http::request_context& context) -> cnetmod::task<void>
            {
                co_await context.with_sse([](cnetmod::http::request_context&,
                    cnetmod::http::sse_stream& stream) -> cnetmod::task<void>
                {
                    cnetboot::framework::web::sse_writer writer{stream};
                    (void)co_await writer.delta("partial");
                    (void)co_await writer.error(error_code::ai_request_failed);
                    (void)co_await writer.finish();
                });
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/error", handler);
            checks.that(got.head == k_sse_head, "error SSE headers are byte-exact", got.head, k_sse_head);
            checks.that(got.completed, "error stream ends with the terminal frame");
            checks.that(got.frames == k_error_frames, "error frames preserve delta, error, and terminal order",
                got.frames, k_error_frames);
        }


        {
            bool started_before{true};
            const auto handler = [&](cnetmod::http::request_context&) -> cnetmod::task<void>
            {
                co_return;
            };
            const auto got = co_await call_handler(*io, *listener, port, "/sse/none", handler);
            (void)started_before;
            checks.that(!got.streamed, "an empty stream has no X-Streamed header");
            checks.that(!got.completed, "an empty stream has no terminal frame");
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
