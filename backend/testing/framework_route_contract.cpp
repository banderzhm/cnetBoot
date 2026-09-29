import std;
import cnetmod.protocol.http;
import cnetboot.chat.controller.chat_route_context;
import cnetboot.framework.config.llm_settings;
import cnetboot.ai.controller.llm_routes;
import cnetboot.system.controller.auth_controller;

namespace {

enum class access_kind
{
    authenticated,
    anonymous,
    optional,
    permission,
};

struct route_expectation
{
    cnetmod::http::http_method method;
    std::string_view path;
    access_kind access;
    std::string_view permission;
};

[[nodiscard]] auto find_endpoint(const cnetmod::http::router& routes,
    cnetmod::http::http_method method, std::string_view path)
    -> std::shared_ptr<const cnetmod::http::endpoint>
{
    for (const auto& endpoint : routes.endpoints())
        if (endpoint->method == method && endpoint->pattern == path)
            return endpoint;
    return {};
}

[[nodiscard]] auto matches_access(const cnetmod::http::endpoint& endpoint,
    const route_expectation& expected) -> bool
{
    const auto anonymous =
        endpoint.metadata.contains<cnetmod::http::allow_anonymous>();
    const auto optional =
        endpoint.metadata.contains<cnetmod::http::optional_authentication>();
    const auto* permissions =
        endpoint.metadata.find<cnetmod::http::required_permissions>();

    switch (expected.access)
    {
    case access_kind::authenticated:
        return !anonymous && !optional && permissions == nullptr;
    case access_kind::anonymous:
        return anonymous && !optional && permissions == nullptr;
    case access_kind::optional:
        return !anonymous && optional && permissions == nullptr;
    case access_kind::permission:
        return !anonymous && !optional && permissions != nullptr &&
            permissions->all_of.size() == 1 &&
            permissions->all_of.front() == expected.permission &&
            permissions->any_of.empty();
    }
    return false;
}

} // namespace

auto main() -> int
{
    cnetmod::http::router routes;

    cnetboot::system::detail::declare_auth_handlers(routes, {});
    cnetboot::ai::declare_llm_routes(routes,
        std::make_shared<const cnetboot::framework::config::llm_directory>());
    cnetboot::chat::detail::declare_chat_handlers(routes, {});

    using enum cnetmod::http::http_method;
    const std::vector<route_expectation> expected{
        {POST, "/api/auth/login", access_kind::anonymous, {}},
        {POST, "/api/auth/refresh", access_kind::authenticated, {}},
        {POST, "/api/auth/logout", access_kind::authenticated, {}},
        {GET, "/api/v1/llm/providers", access_kind::anonymous, {}},
        {POST, "/api/v1/chat/sessions", access_kind::permission, "chat:session:create"},
        {GET, "/api/v1/chat/sessions", access_kind::permission, "chat:session:list"},
        {GET, "/api/v1/chat/sessions/:id", access_kind::permission, "chat:session:list"},
        {PATCH, "/api/v1/chat/sessions/:id", access_kind::permission, "chat:session:update"},
        {DELETE_, "/api/v1/chat/sessions/:id", access_kind::permission, "chat:session:delete"},
        {GET, "/api/v1/chat/sessions/:id/messages", access_kind::permission, "chat:session:list"},
        {POST, "/api/v1/chat/sessions/:id/messages", access_kind::permission, "chat:message:send"},
    };

    if (routes.endpoints().size() != expected.size())
        return 1;
    for (const auto& route : expected)
    {
        const auto endpoint = find_endpoint(routes, route.method, route.path);
        if (!endpoint)
            return 2;
        if (!matches_access(*endpoint, route))
            return 3;
    }

    std::set<std::pair<cnetmod::http::http_method, std::string>> unique;
    for (const auto& endpoint : routes.endpoints())
        if (!endpoint->method ||
            !unique.emplace(*endpoint->method, endpoint->pattern).second)
            return 4;
    return 0;
}
