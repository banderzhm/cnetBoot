# Architecture rules

1. `bootstrap` is the only composition root. It is split into cnetmod
   `application_module`s (`platform`, `identity`, `reading`, `chart`, `chat`)
   that declare typed options, register components and compose routes and
   middleware in cnetmod's named build phases. Components are constructed by
   the container during `build()`; a missing data source, service instance or
   dependency fails the build with the component chain instead of the first
   request.
2. Controllers adapt HTTP to typed use cases and own no business rules.
3. Services orchestrate use cases and transactions but do not write SQL.
4. Simple persistence registers `add_repository<T>()` and injects
   `managed_repository<T>` (shared) or `repository_factory<T>` (owned handles
   via `make_repository()`, request-scoped via `for_request()`).
5. Complex SQL uses XML mapper statements through that same repository.
6. Domain modules never import MySQL or PostgreSQL protocol modules.
7. Cross-module contracts live with the owning module; there is no `types` bin.
8. CPU work uses `application_runtime::offload()` and critical coroutines use
   `spawn_managed()`.
9. Authentication, permission checks, request IDs and audit logging belong to
   the HTTP boundary. Access policy is endpoint metadata declared once when the
   route is registered (`anonymous()`, `optional_identity()`,
   `requires_permission()`), read by the middleware after route matching, and
   never copied into controllers, services or a path whitelist. Unmatched
   requests answer 404 without authentication.
10. Every external service is Application-managed and participates in health,
    recovery, telemetry and graceful shutdown.
11. JWT middleware verifies and decodes a token once, then binds an immutable
    `current_user` to `http::request_scope`. On `optional_identity()` routes an
    absent token is anonymous, but a presented invalid token is rejected with
    401. The authorization middleware consumes that identity. A service receives identity explicitly only for ownership,
    auditing, or another domain invariant; it never checks static route
    permission codes and never uses thread-local identity state.
12. JWTs carry identity and a random 128-bit session ID, not grants. Redis
    stores the session's user ID, username, and department ID; every
    authenticated request reads and renews it in one round trip (GET + EXPIRE
    pipeline), so session revocation is immediate. `POST /api/auth/refresh`
    rotates the session ID (the old token stops working at once) and
    `POST /api/auth/logout` revokes the session. The principal (user status,
    roles, permissions, data scope) is loaded from the database and cached in
    process for `identity.principal_cache_seconds` (default 30 s): disabling an
    account or changing roles takes effect within that bound, and a session
    whose user was deleted, disabled or renamed is revoked on first sight.
13. Row filtering is delegated to cnetmod ORM. The existing authorization
    service combines active roles' `data_scope` values and queries allowed
    departments through the XML mapper; the authentication boundary binds the
    resulting `data_permission_scope`. No separate data-scope service or SQL
    filter is added. Unknown scope values fail closed, and migration V003 keeps
    previously unenforced non-admin defaults owner-only.
14. Login is protected twice: the `login` rate-limit policy per client
    address, and `login_guard` counters in Redis per account and per client
    address that lock with `AUTH.ACCOUNT_LOCKED`. An unknown user name costs
    the same password hash as a known one, and account status is revealed only
    after a correct password.
15. Every response is either the resource document or the registered error
    envelope `{code, status, message, retryable}`; this includes 401/403 from
    the security middleware, 404/405 from the router and 429 from rate
    limiting. Storage details are logged, never returned.
16. Client addresses come from `cnetmod::http::resolve_client_ip` with
    `edge.trusted_proxies`; forwarding headers from any other peer are ignored.
    Expensive routes declare a closed `rate_limit_policy` next to their access
    policy.
17. CPU-bound chart algorithms run through the chart `compute_executor`
    (Application `offload()`); request event loops only do I/O.
18. Chart archive metadata and payload are one database aggregate. A save or
    delete touches one `chart_archive_index` row; filesystem storage and
    cross-resource compensation are forbidden.
19. Dynamic streaming routes enter through `request_context::with_sse()` and
    pass its `sse_stream` to the payload adapter. Nexus owns JSON payloads;
    cnetmod owns framing, write timeouts and the total-duration watchdog.
20. Owner-scoped aggregates (`chart_case`, `chart_tag`, `chat_session`) are
    marked with ORM `DATA_OWNER`. Controllers obtain repositories from
    `repository_factory::for_request()` after authentication has bound the
    immutable data scope; startup-owned repositories are reserved for global
    statistics and infrastructure work. `chat_message` remains a child of the
    authorized session aggregate rather than duplicating an owner column.
21. Reading generation is one application use case: cache lookup, keyed
    coroutine single-flight, LLM invocation and cache persistence live below
    HTTP. Equal user/chart/question requests share one model call; controllers
    only render JSON or SSE.
22. Audit events are append-only rows in `audit_log` (migration V010), written
    through an ORM-backed implementation of the shared audit contract. Logs
    are observability, not the audit system of record.
23. Fixed-shape HTTP payloads are named DTOs serialized by
    `framework::web::respond_json()`. Controllers may use a JSON document only
    when the shape is genuinely dynamic, such as merging an upstream chart
    projection; handwritten field-name objects are not a substitute for DTOs.
24. ZhouYiLab's nlohmann output ends at the `service/xuanxue` adapter boundary.
    The adapter validates it once and returns a Glaze `json::document` in the
    domain result. Domain, use-case and controller APIs never exchange encoded
    JSON strings or parse the same projection again.

## Planned system modules

```text
system/user
system/role
system/menu
system/department
system/post
system/dictionary
system/configuration
system/notice
monitor/login-log
monitor/operation-log
monitor/online-session
monitor/scheduler
tool/code-generator
```

Each module repeats `domain`, `service`, `controller`, and optional `mapper`.
Repository is supplied by cnetmod and is not reimplemented per module.

## Security request pipeline

```text
route matching (endpoint metadata; 404/405 in the error envelope)
  -> rate limiting by declared policy (platform module)
  -> body limit (platform module)
  -> JWT authentication middleware (session checked every request; principal cached ≤ principal_cache_seconds)
  -> request_scope<current_user, data_permission_scope>
  -> authorization middleware (required_permissions from endpoint metadata)
  -> controller (HTTP/DTO adaptation)
  -> service (business use case)
  -> cnetmod managed_repository<T>
```

Route policy remains adjacent to the route instead of living in a central
path-string table:

```cpp
routes.get("/api/system/users", handler,
    security::requires_permission("system:user:list"));

routes.get("/api/v1/capabilities", handler, web::anonymous());

routes.get("/api/system/users/:id/authorization",
    security::require_self_or_permission(
        "id", "system:user:query", handler));
```

The first route requires the named permission through endpoint metadata. The
last is a domain rule (resource ownership) and stays a handler decorator: a
caller may read its own authorization snapshot, while reading a different user
requires `system:user:query`. All permission matching delegates to cnetmod's
segment-wildcard semantics.
