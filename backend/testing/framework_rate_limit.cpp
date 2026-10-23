
import std;
import cnetmod.coro.task;
import cnetmod.core.socket;
import cnetmod.io.io_context;
import cnetmod.protocol.http;
import cnetboot.framework.web.rate_limit;
import cnetboot.framework.web.route_handler;

namespace {

namespace web = cnetboot::framework::web;

struct outcome
{
    bool reached{false};
    int status{};
    std::string body;
};

auto invoke(const cnetmod::http::middleware_fn& middleware,
    std::optional<cnetmod::http::endpoint_metadata> metadata) -> outcome
{
    auto io = cnetmod::make_io_context();
    cnetmod::socket peer;
    cnetmod::http::response response;
    cnetmod::http::header_map headers{{"X-Forwarded-For", "1.2.3.4"}};
    cnetmod::http::route_params params;
    std::optional<cnetmod::http::endpoint> matched;
    if (metadata)
    {
        matched.emplace();
        matched->pattern = "/limited";
        matched->metadata = std::move(*metadata);
        params.matched = &*matched;
    }
    cnetmod::http::request_context request{*io, peer, "POST", "/limited", headers, {},
        response, std::move(params)};
    outcome result;
    cnetmod::sync_wait(middleware(request,
        [&result]() -> cnetmod::task<void>
        {
            result.reached = true;
            co_return;
        }));
    result.status = response.status_code();
    result.body = std::string{response.body()};
    return result;
}

} // namespace

auto main() -> int
{
    int failures = 0;
    const auto check = [&failures](bool condition, std::string_view what)
    {
        if (!condition)
        {
            ++failures;
            std::println("FAIL: {}", what);
        }
    };

    web::edge_options options;
    options.rate_limits.ai = {.rate_per_second = 0.000001, .burst = 1.0};
    check(web::validate_edge_options(options).has_value(), "valid options pass");
    auto middleware = web::rate_limit_middleware(options);

    const auto ai = web::with_rate_limit(web::optional_identity(),
        web::rate_limit_policy::ai);
    const auto first = invoke(middleware, ai);
    const auto second = invoke(middleware, ai);
    check(first.reached, "the first request within burst reaches the handler");
    check(!second.reached && second.status == 429 &&
            second.body.contains("REQUEST.RATE_LIMITED"),
        "the request over budget answers REQUEST.RATE_LIMITED");


    const auto spoofed = invoke(middleware, ai);
    check(!spoofed.reached, "a spoofed forwarding header does not reset the budget");

    const auto login = invoke(middleware,
        web::with_rate_limit(web::anonymous(), web::rate_limit_policy::login));
    check(login.reached, "policies keep independent budgets");

    for (int index = 0; index < 50; ++index)
        check(invoke(middleware, web::anonymous()).reached,
            "routes without a policy are never limited");
    check(invoke(middleware, std::nullopt).reached, "unmatched requests are not limited");

    web::edge_options broken;
    broken.rate_limits.login.rate_per_second = 0.0;
    check(!web::validate_edge_options(broken).has_value(), "a zero rate is rejected");
    broken = {};
    broken.trusted_proxies = {" "};
    check(!web::validate_edge_options(broken).has_value(), "blank proxy entries are rejected");

    std::println("framework_rate_limit: {} failures", failures);
    return failures == 0 ? 0 : 1;
}
