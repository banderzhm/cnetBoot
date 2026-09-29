# Architecture review of `backend_master` — verified

This supersedes the earlier review. Every claim below was re-derived from source with
`file:line` evidence by five independent reviewers; nothing was taken on trust. The tree
builds in WSL (clang 22, cmake 4.4.3) — configure and `--target cnetnexus_chart` both exit 0.

**Headline: the original review's layering analysis is sound, but it gets the wire contract
backwards, mislocates the real P0, overstates two costs, and misses four defects — including a
cross-tenant IDOR and a confirmed UB.**

---

## 1. The most important correction: the error contract is inverted

The review states the framework envelope is `{code:"CASE.NOT_FOUND", status, message, retryable}`.
It is the opposite (`framework/web/error_response.cpp:26-29`):

```cpp
.code    = std::to_string(entry->http_status),   // "404"   <- HTTP status as a STRING
.status  = std::string{entry->name},             // "CASE.NOT_FOUND"
.message = std::string{entry->default_message},
.retryable = entry->retryable,
```

Any client written against the review's shape breaks on every chart/chat error. The mapping is
frozen in an executable test — `testing/framework_sse_frames.cpp:28-31` says so explicitly
("code 是 HTTP 状态码字符串，status 才是注册码") and `:43-51` pins the literal bytes.

The divergence is also **wider than "two formats"**. Four shapes coexist:

| # | Shape | Where |
|---|---|---|
| 1 | `{code:<int>, message}` | system controllers — 14 sites; `common/api_response.cpp:17-18` |
| 2 | `{code:"<http>", status:"REG.NAME", message, retryable}` | chart/chat controllers — 112 sites |
| 3 | shape 2 embedded per item **inside HTTP 200** | batch — `chart_dto.cpp:199-221` |
| 4 | shape 2 in an SSE `event: error` frame | `sse_stream.cpp:32-35`, `chat_route_handlers.cpp:399-411` |

And shape 1 leaks onto chart/chat routes: `require_permission` and `jwt_security` both reply
through `common::respond_failure` (`authorization.cpp:37-42`, `jwt_security.cpp:120-125`), so a
403 on `/api/v1/cases` is numeric while a 404 on the same module is symbolic.

Corrections to the proposed fix:
- **Do not "delete" `common/api_response`.** `common::respond_failure` is the *only* reply path for
  both security layers, and `security` sits **below** `framework_web` (`CMakeLists.txt:103-115` vs
  `:71-83`). Deleting it needs a new dependency edge or moving `respond_error` down a layer.
- The numeric code is **not mechanically mappable** to a registry name: the enum has no assigned
  values and there is no `to_string`; 9 codes collide on 404, 4 on 401, 3 on 409.
- **Nothing in `testing/` pins the system error body** (`request_principal.cpp:56-64` never inspects
  the response). Write that test *before* changing anything, or the migration is CI-invisible.
- Migrations V007/V008 are irrelevant here (they seed `sys_menu.permission`); that link was a
  non-sequitur.
- The claim "the frontend has to handle both" is unverifiable — there is **no frontend in this repo**.

---

## 2. P0 — corrected and reordered

### P0-1 [HIGH, security] Cross-tenant IDOR on chart archives
Not in the original review. Permissions are checked; **ownership never is**.

```cpp
// modules/chart/controller/archive_routes.cpp
routes.get("/api/v1/archive/:algorithm/:id",  archive_lookup_handler{archive});              // :269 — NO GUARD AT ALL
routes.del("/api/v1/archive/:algorithm/:id",
    require_permission("chart:archive:delete", archive_remove_handler{archive}));           // :271
```

- `handle_archive_remove` (`:165-179`) never passes the caller's id.
- `chart_archive_service::remove` takes **no principal**, while `archive()` *does* take `owner_id`
  and stores it (`chart_archive_service.cpp:28-30,54`).
- Search is unfiltered: `ChartMapper.xml:78-91` has no `owner_id` predicate, and `:72` returns
  `a.owner_id` to the client — so every archive id is discoverable.
- Cases have the same gap (`ChartMapper.xml:25-39`).

**Scenario:** user B, holding ordinary `chart:archive:delete`, lists archives, reads A's id, and
permanently deletes A's archive. The system module *already models* this
(`authorization_service.cpp:193-204`, `role_data_scope` all/custom/department/department_tree/self)
— the chart subsystem simply never applies it. **This is the most severe finding in the codebase.**

### P0-2 [HIGH, availability] No request body limit — the real DoS
The review's "no batch cap" is a symptom; the root cause is that **nothing bounds request size**.
cnetmod ships an opt-in `body_limit` middleware (default 1 MB, `body_limit.cppm:30`) and the
backend **never installs it** (zero production hits). The only bound is the parser constant
`max_body_size = 32 MB` (`semantics.cppm:501`).

Measured, not assumed (no benchmark existed; every chart test uses a stub):
**ba_zi 1.13 ms/chart**, qi_men 0.46 ms, liu_yao 0.38 ms. So `~1 ms/chart`, i.e. real but
*modest* — the review's "CPU-heavy" is directionally right, not dramatic. A minimal 60-byte item
into a 32 MB body is ~559k items ≈ **10.5 minutes of uninterrupted CPU**.

The handler is genuinely blocking: cnetmod runs **one** io_context (`application_host.cpp:53,799-809`),
so a stalled handler stalls every connection including SSE. And `request_timeout_ms: 15000` **cannot
save you** — it is cooperative (`with_deadline` → `co_await when_all`, `timer.cppm:245`), and a
synchronous loop over synchronous `std::expected` methods has no suspension point. The timeout fires
after the damage is done.

**Correct fix order (the review's order is wrong):**
1. Install `body_limit` in `bootstrap/application.cpp` (~`:192-202`) — one line, protects *every*
   route.
2. Cap `body->items->size()` at `batch_routes.cpp:284` and return an error. 100 items ≈ 113 ms is
   defensible; no frozen numeric limit exists in `contracts/` or the migration doc, so nothing is
   inherited. `contracts/error_codes.yaml` has no "too many items" code — either reuse
   `request_invalid_argument` or register one; that is a contract decision.
3. *Only then* optionally `offload` per item — defence in depth, **not** the fix: offloading a
   559k batch still monopolises one pool worker for 10 minutes. **Offload relocates the stall; only
   a cap removes it.**
4. Making the service layer return `task<expected<...>>` is **not** a drop-in: all six
   `*_algorithm` methods are deliberately synchronous (`algorithm_api.cppm:14-15`), and converting
   them forces every `*_service` and all six bootstrap adapters to change.

### P0-3 [MEDIUM, correctness] Confirmed UB in the chart executor
Missed by the review, **compiler-diagnosed**:

```cpp
// modules/chart/mapper/chart_mysql_executor.cppm:115-116
orm_scope scope{std::move(*lease),
    raw_session{scope.lease.get(), cnetmod::orm::sql_dialect::mysql}};  // reads scope before init
```

Clang 22: *"variable 'scope' is uninitialized when used within its own initialization
[-Wuninitialized]"*. Live code — 10 call sites across `mysql_chart_case_mapper.cpp`,
`mysql_chart_tag_mapper.cpp`, `mysql_chart_archive_mapper.cpp`. Silent because there is no
`-Werror`. Fix: bind `lease` first, then construct `orm_scope`.

### P0-4 [MEDIUM] Per-request auth cost — the review overstates it
- "1 Redis GET + 1 Redis EXPIRE" — **count confirmed**, but they are **not pipelined** (each takes
  its own pool lease, `redis_template.cpp:571-585,636`), and the EXPIRE **follows** `load_principal`,
  so on a MySQL outage the idle TTL does not slide.
- "3–5 MySQL queries" — **refuted at the ceiling**. Fixed cost is exactly **3**
  (`application.cpp:92`; `authorization_service.cpp:83`, `:102`); reachable max is **6**. 5 is
  unreachable.
- **No N+1 exists.** R roles come back in one `INNER JOIN` (`RoleMapper.xml:6`), M menus in one
  `DISTINCT ... role_id IN (...)` (`MenuMapper.xml:8-12`); permissions are derived in memory
  (`:106-111`).
- Hidden multiplier the review missed: each statement is a **separate pool lease**.

**The proposed cache is the wrong first move.** Its invalidation surface *does not exist*: only four
system routes are registered, `role_service` and `department_service` are **dead code**, there is
no `sys_role_menu` writer and **zero triggers**. A per-user `authz_version` also cannot express
role-keyed edits (no reverse role→users index), so only a single **global epoch** is safe. And the
disabled-user guarantee lives in the `sys_user` status/deleted read (`application.cpp:98-99`) —
cache the whole principal and a disabled user keeps a working session, and the `:94-97` identity
check compares against a stale snapshot.

**Do this instead, first:** add one XML statement joining
`sys_user_role→sys_role→sys_role_menu→sys_menu` on `user_id` to replace queries #2+#3. Fixed cost
3→2, one fewer lease, **zero invalidation surface**, README stays literally true. Fold GET+EXPIRE
into one pipeline (`redis_template.cppm:389`) for 2→1 round trips. Only if measurement still demands
it, cache **only** `principal_authorization` in Redis keyed by `user_id` + one global epoch, and
keep the `sys_user` read per request.

---

## 3. P1 — what holds, what needs correcting

| Claim | Verdict | Correction |
|---|---|---|
| 4. Two persistence styles | **CONFIRMED** | — |
| 4b. "if `managed_repository` lacks transactions" | **REFUTED** | `application_repository::transaction<Result>` **exists** (`orm_repository.cppm:438-448`). The premise is false; the conditional framing collapses. |
| 5. Test-only mappers shipped in prod | **CONFIRMED** | Referenced only by `testing/chart_case_service.cpp:6,116-117`, compiled into `cnetnexus_chart` (`CMakeLists.txt:205,207`). `memory_chart_cache_mapper` **is** production (`application.cppm:84`). |
| 6. Module ownership mixed | **CONFIRMED** | Precision: the chat→chart `naming` import is in `chat_domain.**cpp**:4, not the `.cppm`. |
| 7. Six algorithms duplicated | **PARTLY** | The duplication is **bigger and messier** than stated: ~12 edits across ~15 files in **six** independent lists, and `naming.cpp:8-11` vs `catalog.cpp:8-15` **already disagree on order**. The identical 1900..2100 range block is copy-pasted across four service `.cpp` files; `liu_yao` also normalizes, so the six are not uniform. |
| 7b. The `{name,parse,validate,compute,project}` registry | **NOT FEASIBLE as stated** | Six unrelated input structs, seven unrelated result structs — any uniform signature needs type erasure (json-in/json-out), trading ~120 type-checked lines for a schema + dispatcher. Workable alternative: share the range validator and calendar parsers, keep one ordered `algorithm_descriptor` table as the single source of truth, replace the 12 hand-rolled handler wrappers with one template, and use `std::variant` + `std::visit` **only** for batch. |
| 8. Composition root fragile | **mechanics CONFIRMED, causation REFUTED** | Raw pointers in all 12 services, non-movable assembly whose order is guaranteed only by a comment (`application.cppm:62-65`), in-place bundle fill, raw router capture — all true. But "forced by cnetmod ordering" is **wrong**: `application_builder::service_factory` runs *after* auto-configuration with a live `application_runtime` (`application_host.cppm:149-151`, `application_host.cpp:103-129`), and `auto_configuration_context` hands out the host's real `router&` (`auto_configuration.cppm:17-24`). The only genuinely forced constraint is that `system_mapper_registry` publishes in `start()`. **The fragility is self-inflicted and cheaply removable.** |
| 8b. Split into chart/chat/reading modules | **REFUTED** | `chat` depends on `reading` and `chart` (`CMakeLists.txt:272-287`) and `chat_service` borrows reading's `llm_service` — three `create()` functions create no ordering between the same objects. It preserves every hazard and adds indirection. |
| 8c. `public_()` on routes | **CONFIRMED, but the fix is a trap** | cnetmod's router has **no** public-route API (`router.cppm:396-479`); the only hook is `authorization_options::skip`. System and chart routes are registered from two places into one router (`application.cpp:242-246`), so "declare it at the route" cannot work without an app-level route registry. |

**Also from the public-path whitelist (review missed):** whitelisting **bypasses token parsing
entirely** (`jwt_auth.cppm:70-74`), so any handler under a whitelisted prefix silently loses its
principal. And the six chart routes plus all seven `/reading` routes carry **no guard at all**
(`bazi_routes.cpp:189-193`, `reading_route_bindings.cpp:520-526`) — the whitelist prefixes are
their entire access control.

---

## 4. P2 — corrected

- **Archive storage.** Write order is file→index **with file rollback on index failure**
  (`chart_archive_service.cpp:44,57-63`) — the write path is *safe*. The review put the drift on the
  wrong operation: **`remove()` deletes the file first (`:127`) then the index (`:134`) with no
  rollback**, so an index failure permanently orphans a row that search lists and fetch 404s.
  Multi-instance: the **index scales** (MySQL-backed), the **blob store does not** (local path).
  The file store is otherwise well built (temp+atomic rename, per-instance salt, short-write loop).
- **`CNETNEXUS_ARCHIVE_DIR` via `getenv`** — confirmed (`business_assembly.cpp:136-142`) and it *is*
  expressible in YAML; the only blocker is cnetmod's unknown-key validation.
- **Stubbed seams** — confirmed: `log_audit_sink` (`business_assembly.cpp:64-78`),
  `today_login_provider = nullptr` (`business_routes.cpp:60`), no LLM adapter.
- **Build split** — the review's premise is wrong: `backend_master/build` was configured from the
  **sibling `backend/`** tree (`CMakeCache.txt: CMAKE_HOME_DIRECTORY=/mnt/e/github/cnetNexus/backend`).
  Two near-identical trees share one build dir, so those binaries were never built from
  `backend_master` sources. **Decide which tree is canonical before optimizing the build.**
- Latent trap worth a comment: `memory_chart_*_mapper` takes `std::scoped_lock` inside coroutine
  bodies. Safe today (no `co_await` inside), but the mutex is a plain `std::mutex` — the first
  future `await` added inside deadlocks the event loop.

---

## 5. Recommended order

1. **IDOR** (`archive_routes.cpp:269`, `:271`) — add ownership/data-scope filtering, and guard the unguarded lookup route.
2. **`body_limit`** — one line, global.
3. **Batch item cap** at `batch_routes.cpp:284`.
4. **The UB** at `chart_mysql_executor.cppm:115-116` — and turn on `-Werror` for this target; it is the only compiler-diagnosed defect in `cnetnexus_chart`.
5. **Pin the system error body in a test**, then fix the docs (the wire contract currently exists only in a test literal) and unify the error envelope — with the dependency direction solved downward, not by deleting `common`.
6. **One XML statement** to collapse queries #2+#3, and pipeline the Redis pair — before any principal cache.
7. **Extract the shared range validator** and the ordered `algorithm_descriptor` table.
8. **Own the borrowed members** (`unique_ptrs`, as already done for `audit`/`user_count`) so declaration order stops being load-bearing; move identity resolution and route mounting into `service_factory`.

Items 1–4 are small, local and high-value. Items 5–8 are the real restructuring and should follow
measurement, not precede it.

---

*Evidence: `.dsh-tmp/evaluation/claim1.md`, `claim2_p2.md`, `claim3.md`, `claim456.md`,
`claim78.md`. No file under `backend_master` was modified; all verification builds ran in WSL
scratch copies.*
