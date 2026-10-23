# cnetBoot

cnetBoot is a reusable C++23 backend starter built on cnetmod. It provides a focused
application foundation:

- RBAC for users, roles, permissions, departments, and data scopes;
- JWT and Redis sessions with login, refresh, revocation, idle renewal, and brute-force protection;
- audit logging, a stable error contract, pagination, rate limiting, and SSE;
- explicitly enabled multi-provider, multi-model, multi-key AI integration;
- provider-neutral multi-turn chat with application-defined categories and reference context.

The repository intentionally contains no industry-specific algorithms, workflows, entities,
prompts, or vocabulary. Applications add their own bounded contexts under
`backend/modules/<business>/` without coupling them back into `system`, `ai`, or `chat`.

## Repository layout

```text
backend/
  bootstrap/        Application modules and the single composition root
  framework/        Errors, configuration, security, audit, and web infrastructure
  modules/
    system/         RBAC and identity use cases
    ai/             Model catalog, provider selection, and cnetmod model adapters
    chat/           Generic conversations and messages
  migrations/       MySQL migrations
  testing/          Contract, architecture, and component tests
3rdparty/
  cnetmod/          Pinned cnetmod Git submodule
script/             Windows and WSL dependency/backend build entry points
```

Business modules use peer Spring-style layers: `controller/`, `dto/`, `service/`, `domain/`,
and `mapper/`. DTOs do not live under controllers, ORM records live in the domain layer, and
mappers only implement persistence boundaries.

## Configuration

Review `backend/config/application.yaml` and provide at least:

```bash
export CNETBOOT_DATABASE_PASSWORD='replace-me'
export CNETBOOT_JWT_SECRET='replace-with-at-least-32-characters'
```

AI is explicitly disabled when `llm.providers` is empty, while the rest of the service remains
available. See `backend/config/llm.providers.example.yaml` to enable it. `ai_module` explicitly
registers model capabilities; merely adding a configuration file or route never enables AI.

## Fetch dependencies

Clone with the pinned cnetmod submodule:

```bash
git clone --recurse-submodules --shallow-submodules https://github.com/banderzhm/cnetBoot.git
# For an existing checkout:
git submodule update --init --depth 1 -- 3rdparty/cnetmod
```

cnetBoot never reads cnetmod from a sibling workspace and does not use vcpkg. The prebuild
scripts build cnetmod and its required bundled dependencies from `3rdparty/cnetmod`, then install
an isolated CMake package for this repository. Dependencies for disabled components, such as
PostgreSQL and Raft, are not fetched.

## WSL / Clang build

```bash
./script/wsl_clang_3rdparty_build.sh
CNETBOOT_BUILD_TESTING=ON ./script/wsl_clang_build.sh
ctest --test-dir backend/build-clang --output-on-failure -j2
```

The first command is mandatory and installs cnetmod into `3rdparty/wsl-clang`. The backend build
only consumes that package and fails clearly when it is missing; it never falls back to another
cnetmod checkout.

## Windows / MSVC build

```powershell
.\script\windows_3rdparty_build.ps1
.\script\windows_build.ps1 -BuildTests
ctest --test-dir backend/build-windows -C Release --output-on-failure
```

Windows uses MSVC with Ninja and installs cnetmod into `3rdparty/windows`. The default cnetmod
build concurrency is one to control peak C++ Modules memory usage; pass `-Jobs` when the machine
has enough capacity. WSL defaults to two jobs and accepts `CNETBOOT_JOBS` as an override.

`cnetboot_architecture_gate` is a mandatory `ALL` target, so every default backend build validates
layering, naming, the local cnetmod layout, internationalized source contracts, and build scripts.
GitHub Actions executes the same prebuild, backend build, architecture gate, and tests on Linux
and Windows for `main` and `develop` pushes and pull requests.

## Extension rules

1. Add each business capability as a named module; avoid generic names such as `batch_service`
   and `common_service`.
2. Controllers adapt protocols, services orchestrate use cases, and domains own entities and rules.
3. Prefer cnetmod Application, ORM, JWT, SSE, Redis, and AI capabilities instead of rebuilding
   infrastructure in the application.
4. Run `cnetboot_framework_layering_boundaries` after architecture changes. It rejects vertical
   business vocabulary, invalid directories, anti-layer naming, and non-internationalized literals.
