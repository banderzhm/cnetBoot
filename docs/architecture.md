# Architecture

cnetBoot is assembled by four explicit Application modules in this order:

1. `platform` — mapper statements, audit storage, request body limit, rate limiting and unmatched responses;
2. `identity` — repositories, RBAC services, JWT/Redis sessions, authentication and authorization;
3. `ai` — typed LLM options, explicitly registered provider models, application LLM service and catalog route;
4. `chat` — owner-scoped session/message repositories, chat service and protected routes.

The order is part of the contract. Each module owns its options and registrations. The composition
root resolves dependencies only after validation, and handlers receive stable component references.

## Dependency direction

```text
controller + dto -> service -> domain <- mapper
        |             |
        +------ framework contracts

bootstrap -> every module (composition only)
chat -> ai service contract
system -> security/framework
```

Domain does not import HTTP, Application runtime, provider clients or mapper implementation types.
Mapper converts ORM records to domain values. Controllers convert DTOs to use-case inputs and domain
outputs back to DTOs. Cross-cutting request identity, errors, audit and transport helpers live in
`framework`.

## Explicit AI enablement

The `llm` section is decoded as typed options. During registration, `ai_module` resolves the declared
provider/key bindings and explicitly calls cnetmod model registration. An empty provider list means
AI is disabled; it does not create a hidden default client. Chat only sees `llm_service`.

## Data and time

- MySQL `DATETIME(6)` values contain UTC wall-clock values.
- Conversion between Unix seconds and calendar datetimes occurs at mapper boundaries.
- `deleted_at` logical deletion is configured once in the composition root for Chat repositories.
- Owner-scoped Chat repositories use cnetmod request data-permission context.
- JWT sessions are stored in Redis and checked for every authenticated request.

## Streaming

Chat enters streaming mode through `request_context::with_sse()`. The request remains on its owning
event loop, cnetmod enforces stream duration/write timeouts, and model deltas are translated to typed
SSE payloads at the transport boundary.
