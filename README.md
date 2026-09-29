# CNet Nexus

**CNet Nexus** is a back-end administration starter built on cnetmod. It adopts
the proven controller/service/repository separation without inheriting another
product's name or Java-specific abstractions.

## Dependency direction

```text
HTTP client
        |
        | HTTP + JSON + SSE
        v
backend/bootstrap
        |
        v
backend/modules/*/controller
        |
        v
backend/modules/*/service       business use cases and transaction boundaries
        |
        v
backend/modules/*/repository    cnetmod managed_repository<T>
        |
        v
cnetmod Application -> ORM Mapper -> session gateway -> MySQL/PostgreSQL
```

Controllers never construct database sessions. Services never write SQL.
Simple CRUD uses `managed_repository<T>`; complex reads use typed XML
projections through the same repository. Database-specific code remains inside
cnetmod gateways.

## Architecture

| Layer | Responsibility |
|---|---|
| `backend/bootstrap` | composition root and process entry |
| `backend/framework` | security and HTTP infrastructure |
| `backend/common` | stable cross-module contracts |
| `backend/modules/system` | users, roles, permissions and menus |
| controller | HTTP adaptation only |
| service | use-case orchestration and transaction boundaries |
| repository | cnetmod managed repository and typed XML projections |

Authentication runs once in middleware and binds an immutable principal to
the request scope. The signed JWT carries identity, not permissions: each
authenticated request reloads the active user and role/menu grants from the
database, so disabling a user or changing a role takes effect on the next
request. Route authorization is declared beside route registration
with `require_permission()` or `require_self_or_permission()`. Application
services never inspect HTTP permission strings; they only receive identity
when a business invariant, ownership rule, or audit record requires it.

## Back-end layout

```text
backend/
  bootstrap/                 composition root and main
  common/                    stable cross-module response contracts
  framework/                 security, web and configuration adapters
  modules/
    system/
      domain/                ORM models
      dto/                   input and output DTOs
      repository/            persistence port aliases/configuration
      service/               application use cases
      controller/            HTTP adapters
      mapper/                optional complex XML queries
  migrations/                versioned schema changes
  config/                    Application configuration
```

Do not create a generic `types.cppm`, `utils.cppm`, or a second ORM abstraction.
Each module owns its DTOs and policies.

## Build

Install cnetmod from the `feat/application-architecture` branch (commit
`9be14f50` or newer): the composition root uses application modules, build-time
components, typed options, endpoint metadata, `add_chat_model`, the
connection-affine multi-event-loop runtime, `jwt_auth_reason`, trusted-proxy
client addresses, `rate_limiter::on_limited`, `router::on_unmatched` and
`cnetmod.security.password`. Then:

```bash
cmake -S backend -B backend/build -DCMAKE_PREFIX_PATH=/path/to/cnetmod/install
cmake --build backend/build --config Release
```

Set `CNETNEXUS_DATABASE_PASSWORD` and `CNETNEXUS_JWT_SECRET` (at least 32
bytes), start MySQL and Redis, and apply migrations through V004 before
launching the updated `cnetnexus_server`; V003 must precede the
binary because previously unenforced scope value 1 becomes all-data access.
The checked-in YAML resolves the JWT secret through the configuration loader,
not from application code. `security.jwt.expires_in_seconds` bounds signed
token lifetime; `session_idle_seconds` is renewed on each authenticated request.

`application.io_threads` (default 1, or `CNETMOD_IO_THREADS`) sets the number of
business event loops. Connections stay on their loop; MySQL and Redis pools are
sharded per loop and `minimum_size`/`maximum_size` are application totals, so
every enabled pool's `maximum_size` must be at least `io_threads` or `build()`
fails. With the checked-in YAML the limit is 4 (`calculation-cache`).

## Implemented vertical slice

- cnetmod Application lifecycle, management endpoints and crash dumps
- auto-configured MySQL and Redis
- `sys_user` ORM model with logical delete and optimistic versioning
- role, menu, user-role and role-menu models
- JWT verification with request-scoped current-user propagation
- `POST /api/auth/login` with PBKDF2-SHA256 verification (cnetmod), a signed
  JWT and a Redis session; protected by the `login` rate-limit policy and by
  per-account / per-client failure locks (`AUTH.ACCOUNT_LOCKED`); unknown user
  names cost the same hash as known ones
- `POST /api/auth/refresh` rotates the session ID and reissues the JWT; the old
  token stops working immediately
- `POST /api/auth/logout` revokes the current session
- route-level permission and self-access guards
- ORM row-level filtering via a request-bound owner/department scope; the
  current `admin` role retains unrestricted access
- Authorization snapshots are cached in process for
  `identity.principal_cache_seconds` (default 30 s); sessions are checked on
  every request, so logout and refresh take effect immediately
- hierarchical menu construction
- paginated `GET /api/system/users`
- `GET /api/system/users/:id/authorization`
- one error envelope `{code, status, message, retryable}` for every failure,
  including 401/403, 404/405 and 429; success responses are the resource
  document itself
- LLM readings (`POST /api/v1/<algorithm>/reading`,
  `POST /api/v1/cases/:id/reading`) and `POST /api/v1/bazi/archive` (login
  required) are rate limited per client; configure `edge.trusted_proxies`
  behind a reverse proxy

Run `ctest --test-dir backend/build --output-on-failure` for request-principal,
mapper, authorization, rate-limit and identity-session tests (the Redis-backed
suites report Skipped when Redis is unreachable). Provision users with a unique username and a
`pbkdf2-sha256$210000$<32 hex salt>$<64 hex digest>` password hash; never
store a plaintext password. Login requires an active, non-deleted user.
`sys_role.data_scope` uses 1=all, 2=custom departments, 3=current department,
4=current department and descendants, 5=self. Multiple roles combine by union;
every authenticated user retains access to their own user row. Unknown values
fail closed. Migration V003 changes existing non-admin roles with the previously
unenforced default 1 to 5 and changes the default for new roles to 5. Grant
all-data access to a non-admin role explicitly only after reviewing that role.

The remaining system modules should repeat the same vertical slice for
departments, dictionaries, configuration, notices and operation logs.
