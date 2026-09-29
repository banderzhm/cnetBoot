/**
 * @brief Typed response boundary: DTO fields are serialized by Glaze and
 * absent optionals are not rebuilt manually in controllers.
 */
import std;
import cnetmod.core.socket;
import cnetmod.io.io_context;
import cnetmod.protocol.http;
import cnetnexus.framework.web.response;

namespace cnetnexus::testing {

struct sample_response
{
    std::string answer;
    std::optional<bool> cached;
};

[[nodiscard]] auto render(sample_response body) -> std::pair<int, std::string>
{
    auto io = cnetmod::make_io_context();
    cnetmod::socket peer;
    cnetmod::http::response response;
    cnetmod::http::header_map headers;
    cnetmod::http::route_params params;
    cnetmod::http::request_context request{*io, peer, "GET", "/typed",
        headers, {}, response, std::move(params)};
    cnetnexus::framework::web::respond_json(request, body);
    return {response.status_code(), std::string{response.body()}};
}

} // namespace cnetnexus::testing

auto main() -> int
{
    const auto [plain_status, plain] =
        cnetnexus::testing::render(
            cnetnexus::testing::sample_response{.answer = "甲木"});
    if (plain_status != cnetmod::http::status::ok ||
        plain != R"({"answer":"甲木"})")
        return 1;

    const auto [cached_status, cached] =
        cnetnexus::testing::render(cnetnexus::testing::sample_response{
            .answer = "甲木", .cached = true});
    if (cached_status != cnetmod::http::status::ok ||
        cached != R"({"answer":"甲木","cached":true})")
        return 2;
    return 0;
}
