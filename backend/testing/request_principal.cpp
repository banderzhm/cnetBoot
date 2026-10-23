import std;
import cnetmod.coro.task;
import cnetmod.core.socket;
import cnetmod.io.io_context;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetmod.security.jwt;
import cnetboot.framework.security.authorization;
import cnetboot.framework.security.current_user;
import cnetboot.framework.security.jwt_security;

auto main() -> int
{
    const auto session_id = cnetmod::security::generate_secure_token(16);
    if (session_id.size() != 32 ||
        cnetboot::security::jwt_session_key(session_id) !=
            "cnetboot:jwt:session:{sessions}:" + session_id)
        return 4;

    const cnetboot::security::jwt_session_record session{
        .user_id = 42,
        .username = "alice",
        .department_id = 7,
    };
    const auto encoded = cnetmod::json::write(session);
    if (!encoded)
        return 5;
    const auto decoded = cnetmod::json::parse<
        cnetboot::security::jwt_session_record>(*encoded);
    if (!decoded || decoded->user_id != session.user_id ||
        decoded->username != session.username ||
        decoded->department_id != session.department_id)
        return 6;

    cnetmod::http::request_scope scope;
    scope.bind(std::make_shared<cnetboot::security::current_user>(
        cnetboot::security::current_user{
            .id = 42,
            .username = "alice",
            .permissions = {"system:user:list"},
        }));
    const auto* current = scope.find<cnetboot::security::current_user>();
    if (!current || current->id != 42 || current->username != "alice" ||
        current->permissions != std::vector<std::string>{"system:user:list"})
        return 1;


    const auto authorize = [current](std::string permission) -> std::pair<bool, int>
    {
        auto io = cnetmod::make_io_context();
        cnetmod::socket peer;
        cnetmod::http::response response;
        cnetmod::http::header_map headers;
        cnetmod::http::route_params params;
        cnetmod::http::endpoint endpoint;
        endpoint.method = cnetmod::http::http_method::GET;
        endpoint.pattern = "/api/system/users";
        endpoint.metadata =
            cnetboot::security::requires_permission(std::move(permission));
        params.matched = &endpoint;
        cnetmod::http::request_context request{
            *io, peer, "GET", "/api/system/users", headers, {}, response,
            std::move(params)};
        request.scope().bind(std::make_shared<cnetboot::security::current_user>(
            *current));
        bool called = false;
        auto middleware = cnetboot::security::authorization_middleware();
        cnetmod::sync_wait(middleware(request,
            [&called]() -> cnetmod::task<void>
            {
                called = true;
                co_return;
            }));
        return {called, response.status_code()};
    };
    if (const auto [allowed, status] = authorize("system:user:list"); !allowed)
        return 3;
    if (const auto [allowed, status] = authorize("system:user:remove");
        allowed || status != cnetmod::http::status::forbidden)
        return 7;

    const auto query = std::string_view{"page=2&userName=Alice+Smith%26Co"};
    if (cnetmod::http::parse_query_param(query, "page") != "2" ||
        cnetmod::http::url_decode(
            cnetmod::http::parse_query_param(query, "userName")) !=
            "Alice Smith&Co")
        return 2;
    return 0;
}
