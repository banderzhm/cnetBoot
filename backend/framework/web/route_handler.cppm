/**
 * @brief Named route handler adapters.
 *
 * Controllers register named free functions plus a named dependency struct
 * instead of anonymous closures. Route policy (anonymous, optional
 * authentication, required permissions) is declared with cnetmod endpoint
 * metadata at the registration site.
 */
export module cnetnexus.framework.web.route_handler;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;

export namespace cnetnexus::framework::web {

/**
 * @brief Adapts "named function + dependency struct" to handler_fn.
 */
template <typename Dependency>
class route_handler
{
public:
    using invoker_type = cnetmod::task<void> (*)(
        cnetmod::http::request_context&, const Dependency&);

    route_handler(Dependency dependency, invoker_type invoker) noexcept
        : dependency_{std::move(dependency)},
          invoker_{invoker}
    {
    }

    auto operator()(cnetmod::http::request_context& context) const
        -> cnetmod::task<void>
    {
        return invoker_(context, dependency_);
    }

private:
    Dependency dependency_;
    invoker_type invoker_;
};

/**
 * @brief Adapts a named function without dependencies.
 */
class stateless_route_handler
{
public:
    using invoker_type = cnetmod::task<void> (*)(cnetmod::http::request_context&);

    explicit stateless_route_handler(invoker_type invoker) noexcept
        : invoker_{invoker}
    {
    }

    auto operator()(cnetmod::http::request_context& context) const
        -> cnetmod::task<void>
    {
        return invoker_(context);
    }

private:
    invoker_type invoker_;
};

/** @brief Adapts a named handler that operates on one long-lived service. */
template <typename Service>
class service_route_handler
{
public:
    using invoker_type = cnetmod::task<void> (*)(
        cnetmod::http::request_context&, Service&);

    service_route_handler(Service& service, invoker_type invoker) noexcept
        : service_{&service}, invoker_{invoker}
    {
    }

    auto operator()(cnetmod::http::request_context& context) const
        -> cnetmod::task<void>
    {
        return invoker_(context, *service_);
    }

private:
    Service* service_;
    invoker_type invoker_;
};

/**
 * @brief Metadata of a route reachable without credentials.
 */
[[nodiscard]] inline auto anonymous() -> cnetmod::http::endpoint_metadata
{
    return cnetmod::http::endpoint_metadata{cnetmod::http::allow_anonymous{}};
}

/**
 * @brief Metadata of a public route that binds the caller when a valid token
 *        is presented (archive ownership, reading cache partition).
 */
[[nodiscard]] inline auto optional_identity() -> cnetmod::http::endpoint_metadata
{
    return cnetmod::http::endpoint_metadata{
        cnetmod::http::optional_authentication{}};
}

} // namespace cnetnexus::framework::web
