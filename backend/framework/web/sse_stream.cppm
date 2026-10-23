export module cnetboot.framework.web.sse_stream;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.query;

export namespace cnetboot::framework::web {


[[nodiscard]] auto sse_delta_payload(std::string_view text) -> std::string;


[[nodiscard]] auto sse_error_payload(cnetboot::framework::error::error_code code) -> std::string;


class sse_writer
{
public:
    explicit sse_writer(cnetmod::http::sse_stream& stream) noexcept;


    [[nodiscard]] auto started() const noexcept -> bool;


    auto begin() -> cnetmod::task<bool>;


    auto delta(std::string_view text) -> cnetmod::task<bool>;


    auto done(std::string_view payload) -> cnetmod::task<bool>;


    auto error(cnetboot::framework::error::error_code code) -> cnetmod::task<bool>;


    auto finish() -> cnetmod::task<bool>;

private:

    auto write_event(std::string_view payload, std::string_view event) -> cnetmod::task<bool>;

    cnetmod::http::sse_stream* stream_;
};


[[nodiscard]] auto sse_delta_callback(sse_writer& writer) -> std::function<cnetmod::task<bool>(std::string_view)>;


[[nodiscard]] auto sse_requested(std::string_view query) -> std::expected<bool, cnetboot::framework::error::error_code>;

} // namespace cnetboot::framework::web
