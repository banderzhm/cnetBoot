/**
 * @brief Typed response boundary: DTO fields use cnetmod's explicit JSON
 * mapping and absent optionals are not rebuilt manually in controllers.
 */
#include <cnetmod/json.hpp>

import std;
import cnetmod.core.socket;
import cnetmod.io.io_context;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetboot.framework.web.response;

namespace cnetboot::testing {

struct sample_response
{
    std::string answer;
    std::optional<bool> cached;
};

} // namespace cnetboot::testing

CNETMOD_JSON(cnetboot::testing::sample_response,
    CNETMOD_JSON_FIELD(answer),
    CNETMOD_JSON_FIELD(cached))

namespace cnetboot::testing {

[[nodiscard]] auto render(sample_response body) -> std::pair<int, std::string>
{
    auto io = cnetmod::make_io_context();
    cnetmod::socket peer;
    cnetmod::http::response response;
    cnetmod::http::header_map headers;
    cnetmod::http::route_params params;
    cnetmod::http::request_context request{*io, peer, "GET", "/typed",
        headers, {}, response, std::move(params)};
    cnetboot::framework::web::respond_json(request, body);
    return {response.status_code(), std::string{response.body()}};
}

} // namespace cnetboot::testing

auto main() -> int
{
    const auto [plain_status, plain] =
        cnetboot::testing::render(
            cnetboot::testing::sample_response{.answer = "sample"});
    if (plain_status != cnetmod::http::status::ok ||
        plain != R"({"answer":"sample"})")
        return 1;

    const auto [cached_status, cached] =
        cnetboot::testing::render(cnetboot::testing::sample_response{
            .answer = "sample", .cached = true});
    if (cached_status != cnetmod::http::status::ok ||
        cached != R"({"answer":"sample","cached":true})")
        return 2;
    return 0;
}
