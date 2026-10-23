module cnetboot.framework.web.rate_limit;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetmod.protocol.http.middleware.rate_limiter;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;

namespace cnetboot::framework::web {
namespace {

    [[nodiscard]] auto validate_rule(const rate_limit_rule& rule, std::string_view name)
        -> std::expected<void, std::string>
    {
        if (!(rule.rate_per_second > 0.0) || !(rule.burst >= 1.0))
            return std::unexpected{std::format(
                "rate_limits.{} requires rate_per_second > 0 and burst >= 1", name)};
        return {};
    }

    [[nodiscard]] auto make_limiter(const rate_limit_rule& rule,
        const std::vector<std::string>& trusted_proxies) -> cnetmod::http::middleware_fn
    {
        return cnetmod::rate_limiter({
            .rate = rule.rate_per_second,
            .burst = rule.burst,
            .trusted_proxies = trusted_proxies,
            .on_limited = [](cnetmod::http::request_context& request, std::chrono::seconds)
            {
                respond_error(request,
                    cnetboot::framework::error::error_code::request_rate_limited);
            },
        });
    }


    struct policy_limiters
    {
        cnetmod::http::middleware_fn login;
        cnetmod::http::middleware_fn ai;

        [[nodiscard]] auto select(rate_limit_policy policy) const
            -> const cnetmod::http::middleware_fn&
        {
            switch (policy)
            {
            case rate_limit_policy::login:
                return login;
            case rate_limit_policy::ai:
                return ai;
            }
            throw std::logic_error{"unknown rate limit policy"};
        }
    };

} // namespace

auto validate_edge_options(const edge_options& options)
    -> std::expected<void, std::string>
{
    for (const auto& proxy : options.trusted_proxies)
        if (proxy.find_first_not_of(" \t") == std::string::npos)
            return std::unexpected{std::string{"trusted_proxies must not contain empty entries"}};
    if (auto valid = validate_rule(options.rate_limits.login, "login"); !valid)
        return valid;
    return validate_rule(options.rate_limits.ai, "ai");
}

auto with_rate_limit(cnetmod::http::endpoint_metadata metadata,
    rate_limit_policy policy) -> cnetmod::http::endpoint_metadata
{
    metadata.add(rate_limited{.policy = policy});
    return metadata;
}

auto client_ip(const cnetmod::http::request_context& request,
    const edge_options& options) -> std::string
{
    return cnetmod::http::resolve_client_ip(request, options.trusted_proxies);
}

auto rate_limit_middleware(const edge_options& options) -> cnetmod::http::middleware_fn
{
    auto limiters = std::make_shared<const policy_limiters>(policy_limiters{
        .login = make_limiter(options.rate_limits.login, options.trusted_proxies),
        .ai = make_limiter(options.rate_limits.ai, options.trusted_proxies),
    });
    return [limiters](cnetmod::http::request_context& request,
               cnetmod::http::next_fn next) -> cnetmod::task<void>
    {
        const auto* endpoint = request.endpoint();
        const auto* declared = endpoint == nullptr
            ? nullptr
            : endpoint->metadata.find<rate_limited>();
        if (declared == nullptr)
            return next();
        return limiters->select(declared->policy)(request, std::move(next));
    };
}

} // namespace cnetboot::framework::web
