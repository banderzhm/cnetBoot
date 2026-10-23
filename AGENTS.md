# cnetBoot Repository Instructions

## Mandatory AI startup

Before searching, designing, or changing code, an AI agent must:

1. Read this file in full.
2. Initialize `3rdparty/cnetmod` when necessary.
3. You must read the latest `3rdparty/cnetmod/AGENTS.md` in full. It is authoritative for cnetmod APIs, modules, concurrency, security, ORM, Redis, HTTP, SSE, AI, and build rules.
4. Read the relevant upstream `skill/**/*.md` files named there.
5. Verify the exact cnetmod revision consumed by this repository; never guess APIs from memory.

If the submodule or its instructions are missing, report it. Do not recreate framework capabilities inside cnetBoot.

## Product boundary

cnetBoot is an international, general-purpose C++23 backend starter built on cnetmod. It is not a vertical-industry application.

The built-in surface is limited to:

- RBAC: users, roles, permissions, menus, departments, and data scopes;
- identity: login, JWT verification, Redis sessions, refresh, revocation, and abuse protection;
- infrastructure: stable errors, pagination, auditing, rate limiting, body limits, SSE, and observability;
- generic AI: multiple providers, models, keys, explicit model registration, and catalogs;
- generic chat: conversations, messages, categories, reference context, and sync/stream generation.

Never add industry algorithms, entities, prompts, samples, cache keys, errors, routes, tables, DTOs, permissions, or roles. Repository-owned source, tests, migrations, configuration, and public documentation must be English-only. The nested cnetmod submodule follows its own repository rules.

New product domains belong in `backend/modules/<bounded-context>/` and are registered explicitly by their own Application module. Foundation modules must not depend on added product domains.

## Architecture

Built-in Application modules are:

- `platform`: entry middleware, stable 404/405 handling, XML statements, auditing, and cross-cutting infrastructure;
- `identity`: RBAC, authentication, authorization, JWT/Redis sessions, and system routes;
- `ai`: typed options, catalogs, explicit `add_chat_model`, and the application-facing LLM service;
- `chat`: conversation/message repositories, use cases, HTTP routes, and SSE routes.

`backend/bootstrap/` is the only composition root. Only it may read top-level configuration, declare typed options, register implementations, choose lifetimes, and connect bounded contexts.

Business code must not depend on `application_runtime`, use runtime as a service locator, or use two-phase `attach()` initialization. Inject dependencies through constructors or explicit factories. Missing dependencies must fail Application construction before traffic is accepted.

Each module uses sibling directories:

```text
controller/  HTTP routes and transport adaptation
dto/         request/response DTOs and transport projections
service/     named use cases, transactions, and orchestration
domain/      entities, value objects, rules, and ORM records
mapper/      repository implementations, XML, and persistence mapping
```

Dependency direction is `controller -> service -> domain <- mapper`; bootstrap may depend on all layers only for composition.

- Controllers parse transport input, invoke one use case, and write a response. No SQL, permission algorithms, cache policy, or multi-component orchestration.
- DTOs belong in `dto/`, never `controller/*_dto.cppm`.
- ORM records belong in `domain/`, never `mapper/`.
- Mappers handle persistence and conversion and must not hold runtime objects.
- Services use domain names. `batch_service`, `common_service`, `base_service`, `manager_service`, and `helper_service` are forbidden.
- Shared HTTP, time, security, and audit facilities belong in `backend/framework/`.
- `.cppm` exports interfaces; non-template implementation belongs in matching `.cpp` files.

## Prefer cnetmod

Check current cnetmod instructions and source before adding infrastructure. Prefer upstream repositories, ORM/XML facilities, logical deletion, automatic fill, tenant/data scopes, Redis templates, pool timeouts, pipelines, JWT middleware, endpoint metadata, optional authentication, authorization, routers, 405 handling, body limits, rate limiting, SSE, cancellation, timeout handling, event-loop restoration, chat models, file I/O, durability, and executor offload.

If a gap is framework-generic, implement it in cnetmod and update its skill documentation and generated `AGENTS.md`. Do not hide it behind a cnetBoot resolver, wrapper, service locator, or error-string parser.

## AI, chat, JSON, and DTO rules

- AI is explicitly enabled: `ai_module` validates typed `llm` options and explicitly calls `add_chat_model`.
- Multiple providers/models/keys are explicit composition policy. Provider SDK types stay under `modules/ai/service/model/`.
- Other modules depend only on cnetBoot's `llm_service`, `llm_request`, and `llm_response` contracts.
- Chat categories, reference context, prompts, and samples remain generic and English-only.
- Streaming routes enter through `request_context::with_sse()`.
- Client disconnect, cancellation, upstream rate limits, and upstream failure are distinct states.
- Never hand-write or concatenate raw JSON, including tests and fallbacks.
- Define DTOs for stable contracts; use cnetmod JSON objects/arrays for dynamic structures.
- Do not introduce a second JSON library or scatter repeated transport keys through controllers.

## Persistence rules

- Prefer ORM repositories/wrappers; use XML only for semantics the current ORM cannot express.
- ORM and XML share tenant, permission, logical-deletion, and automatic-fill semantics.
- Configure logical deletion per entity/table; do not mix boolean and nullable timestamp policies globally.
- Use automatic fill for creation/update timestamps unless an explicit atomic or preservation requirement exists.
- Store MySQL `DATETIME(6)` as UTC wall-clock values. Convert Unix time at mapper boundaries; avoid session-time-zone SQL functions.
- XML must not duplicate interceptor-injected predicates.
- Add ordered migrations; never rewrite released history.
- Use structured database errors, never message parsing.
- Complex tenant, hierarchy, deletion, and fill changes require real database tests.

## Identity, Redis, and concurrency

- JWT issuer, secret, lifetimes, and Redis session settings come from typed YAML/environment configuration.
- Protected requests validate both JWT and Redis session state.
- Redis acquisition respects pool timeout. Authentication failures are explicit; optional caches degrade quickly.
- Login limits use trusted-proxy client resolution.
- Password hashing uses cnetmod security APIs only.
- Use `cnetmod::task<T>` and `co_await`; never block an event-loop thread.
- Offloaded work resumes on the request's event loop. Request sockets, SSE streams, and scoped repositories stay on their owner loop.
- Coroutine paths use cnetmod locks/semaphores, not blocking standard synchronization.

## C++23 and naming

- Use C++23 Modules and `import std;`; do not add standard-library includes.
- Prefix standard-library names with `std::` and directly import every used cnetmod module.
- Use `std::expected`, registered errors, and cnetmod logger.
- Environment variables use `CNETBOOT_`.
- New errors update the enum, registry, `backend/contracts/error_codes.yaml`, and tests together.
- Names must express ownership. Do not add ambiguous `misc`, `utils2`, or `common_manager` components.
- Do not add root directories or top-level modules without updating CMake, docs, and gates.

## Required workflow and gates

1. Read these rules and current cnetmod instructions.
2. Use CodeGraph before grep when `.codegraph/` exists.
3. Inspect Git state and preserve user changes.
4. Place changes in cnetmod, framework, module, or bootstrap according to ownership.
5. Update interfaces, implementations, CMake, configuration, migrations, contracts, tests, and docs together.
6. After a cnetmod revision change, prebuild/install cnetmod before building the backend.
7. Run mandatory gates, complete builds, and tests. Do not commit failed builds.
8. Run `git diff --check` and check for generated output, secrets, personal paths, non-English text, and vertical-domain material.

`cnetboot_framework_layering_boundaries` must reject unregistered modules, non-English repository text, vertical vocabulary, misplaced DTOs/records, ambiguous services, leaked provider types, and missing required boundaries. `cnetboot_architecture_gate` remains an `ALL` target in local and GitHub Actions builds.

Build cnetmod from bundled source without vcpkg, then consume its installed CMake package:

```powershell
powershell -ExecutionPolicy Bypass -File script/windows_3rdparty_build.ps1 -Config Release
powershell -ExecutionPolicy Bypass -File script/windows_build.ps1 -Config Release -BuildTests
```

```bash
./script/wsl_clang_3rdparty_build.sh Release
CNETBOOT_BUILD_TESTING=ON ./script/wsl_clang_build.sh Release
```

Install prefixes are `3rdparty/windows/` and `3rdparty/wsl-clang/`; build/install output is never committed. Real MySQL/Redis and tenant/security changes require real-service success, failure, timeout, concurrency, revocation, deletion, cross-tenant, and hierarchy tests. Report skipped tests explicitly.

## Pre-commit checklist

- [ ] Current cnetmod instructions and relevant skills were read.
- [ ] cnetmod was prebuilt after revision changes.
- [ ] Architecture gate passed.
- [ ] Required Windows/WSL builds and CTest suites passed; skips are reported.
- [ ] `git diff --check` passed.
- [ ] Repository-owned text is English-only and domain-neutral.
- [ ] No secrets, generated files, build output, or personal paths are staged.
- [ ] Framework changes updated cnetmod skills and generated `AGENTS.md`.
