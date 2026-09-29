# cnetBoot Repository Instructions

## Scope

cnetBoot is a reusable base application. Its built-in business surface is limited to RBAC,
authentication/session management, audit, generic AI provider/model integration, and generic Chat.
Do not add vertical-domain algorithms, entities, prompts, routes, caches, or terminology to the base.

## Architecture gate

- Business modules live under `backend/modules/<bounded-context>/`.
- Within a module, `controller`, `dto`, `service`, `domain`, and `mapper` are sibling directories.
- DTO declarations and projections belong in `dto/`, never in `controller/`.
- ORM records are database entities and belong in `domain/`, never in `mapper/`.
- Mapper code only implements persistence boundaries. It must not depend on Application runtime or
  use it as a service locator.
- Controllers adapt HTTP only. Business validation and orchestration belong in a named Service.
- Services must be named after a concrete use case or capability. Generic names such as
  `batch_service`, `common_service`, `base_service`, and `manager_service` are forbidden.
- Cross-cutting HTTP/security helpers belong in `backend/framework`, not in a business controller.
- The only composition root is `backend/bootstrap`; dependencies are constructor-injected.

## cnetmod usage

- Prefer cnetmod Application modules, typed options, component resolution, ORM repositories, JWT,
  Redis templates, endpoint metadata, `request_context::with_sse()`, and AI model composition.
- AI models are enabled explicitly by `ai_module` through `add_chat_model`; configuration alone must
  not silently activate a capability.
- Keep provider SDK and `cnetmod.ai` types behind `modules/ai/service/model`. Chat depends on the
  application-owned `llm_service` contract, not provider clients.
- Never implement key rotation in cnetmod-facing infrastructure. Provider/key selection is an
  application policy expressed by the explicitly configured composed model.
- Never hand-build raw JSON strings. Use DTOs plus cnetmod JSON documents/codecs.
- Streaming endpoints must enter through `request_context::with_sse()` so total-duration and write
  watchdogs remain active.

## C++ rules

- Use C++23 modules and `import std;`; do not add standard-library includes.
- Prefix standard-library symbols with `std::`.
- Use `cnetmod::task<T>` and `co_await` for asynchronous work; never block an event-loop thread.
- Use coroutine synchronization primitives, never `std::mutex` or condition variables in coroutine paths.
- Use `std::expected`/registered application error codes for failures.
- Use cnetmod logger only; do not add `printf`, iostreams, or ad-hoc console output.
- Keep module interfaces in `.cppm` and non-template implementations in `.cpp`.

## Required verification

Before committing, build with the same Clang/libc++ toolchain as cnetmod and run CTest. The
`cnetboot_framework_layering_boundaries` test is mandatory and must continue to reject vertical
business leakage, DTOs in controllers, ORM records in mappers, and generic service names.
