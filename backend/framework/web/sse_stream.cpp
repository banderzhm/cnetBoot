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

namespace cnetboot::framework::web::dto {

    struct sse_delta_dto
    {
        std::string text;
    };

} // namespace cnetboot::framework::web::dto

CNETMOD_JSON(cnetboot::framework::web::dto::sse_delta_dto,
    CNETMOD_JSON_FIELD(text))

namespace cnetboot::framework::web {

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


    return stream_->started();
}

auto sse_writer::begin() -> cnetmod::task<bool>
{

    co_return co_await stream_->begin();
}

auto sse_writer::write_event(std::string_view payload, std::string_view event) -> cnetmod::task<bool>
{




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
