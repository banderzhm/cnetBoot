module;

#include <cnetmod/json_codec.hpp>

module cnetboot.framework.web.sse_stream;

import std;
import cnetmod.json;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.query;

namespace cnetboot::framework::web {

namespace dto {

    struct sse_delta_dto
    {
        std::string text;
    };

} // namespace dto

auto sse_delta_payload(std::string_view text) -> std::string
{
    auto payload = cnetmod::json::write(dto::sse_delta_dto{.text = std::string{text}});
    return payload ? std::move(*payload) : std::string{};
}

auto sse_error_payload(cnetboot::framework::error::error_code code) -> std::string
{
    return error_body_json(code);
}

sse_writer::sse_writer(cnetmod::http::sse_stream& stream) noexcept
    : stream_{&stream} {}

auto sse_writer::started() const noexcept -> bool
{
    // 不再镜像本地状态：直接转发框架的「是否已开始 SSE」（request_context::sse_started），
    // 框架在写响应头之前就把状态置为 committing，因此判据是「已尝试写出头部/帧」。
    return stream_->started();
}

auto sse_writer::begin() -> cnetmod::task<bool>
{
    // 判据交给框架：状态为 open 返回 true，committing/failed/closed 由 sse_begin 判定。
    co_return co_await stream_->begin();
}

auto sse_writer::write_event(std::string_view payload, std::string_view event) -> cnetmod::task<bool>
{
    // 唯一的具名事件出口：帧文本（event:/data: 与空行）由框架的 sse_send → http::sse::data 产出，
    // 本文件不拼任何字节。sse_send 会先把 200 + 事件流响应头写到连接上（框架在写之前就把状态
    // 置为 committing，写失败停在 failed，两者都算已开始），此后即使正文写失败，响应也已经
    // 不是普通 HTTP 错误了（回退判据比写动作更保守）。
    co_return co_await stream_->send(payload, event);
}

auto sse_writer::delta(std::string_view text) -> cnetmod::task<bool>
{
    co_return co_await write_event(sse_delta_payload(text), "delta");
}

auto sse_writer::done(std::string_view payload) -> cnetmod::task<bool>
{
    co_return co_await write_event(payload, "done");
}

auto sse_writer::error(cnetboot::framework::error::error_code code) -> cnetmod::task<bool>
{
    co_return co_await write_event(sse_error_payload(code), "error");
}

auto sse_writer::finish() -> cnetmod::task<bool>
{
    // 终止帧同样走框架：写之前 sse_begin 会把状态推到 committing/open，started() 随之变为 true。
    co_return co_await stream_->finish();
}

auto sse_delta_callback(sse_writer& writer) -> std::function<cnetmod::task<bool>(std::string_view)>
{
    return [&writer](std::string_view delta) -> cnetmod::task<bool>
    {
        co_return co_await writer.delta(delta);
    };
}

auto sse_requested(std::string_view query) -> std::expected<bool, cnetboot::framework::error::error_code>
{
    const auto raw = cnetboot::framework::web::query::value(query, "stream");
    if (!raw || raw->empty())
        return false;
    if (*raw == "true" || *raw == "1")
        return true;
    if (*raw == "false" || *raw == "0")
        return false;
    return std::unexpected{cnetboot::framework::error::error_code::request_invalid_argument};
}

} // namespace cnetboot::framework::web
