
export module cnetboot.framework.web.rate_limit;

import std;
import cnetmod.protocol.http;

export namespace cnetboot::framework::web {


enum class rate_limit_policy
{
    login,
    ai,
};


struct rate_limited
{
    rate_limit_policy policy{};
};


struct rate_limit_rule
{
    double rate_per_second{1.0};
    double burst{5.0};
};


struct rate_limit_rules
{
    rate_limit_rule login{.rate_per_second = 0.2, .burst = 5.0};
    rate_limit_rule ai{.rate_per_second = 0.05, .burst = 3.0};
};


struct edge_options
{

    std::vector<std::string> trusted_proxies;
    rate_limit_rules rate_limits;
};


inline constexpr std::string_view edge_section_name{"edge"};


[[nodiscard]] auto validate_edge_options(const edge_options& options)
    -> std::expected<void, std::string>;


[[nodiscard]] auto with_rate_limit(cnetmod::http::endpoint_metadata metadata,
    rate_limit_policy policy) -> cnetmod::http::endpoint_metadata;


[[nodiscard]] auto client_ip(const cnetmod::http::request_context& request,
    const edge_options& options) -> std::string;


[[nodiscard]] auto rate_limit_middleware(const edge_options& options)
    -> cnetmod::http::middleware_fn;

} // namespace cnetboot::framework::web
