# Xuanxue backend migration

The xuanxue-algorithm backend was migrated into CNet Nexus under the confirmed
strategy: CNet Nexus owns identity, and only business capability moved.

## Scope

| Source (xuanxue-algorithm) | Destination (backend/) |
|---|---|
| `xuanxue-common/error` | `framework/error/` |
| `xuanxue-system/domain/page` | `framework/pagination.cppm` |
| `xuanxue-framework/web/{query,error,sse}` | `framework/web/{query,error_response,sse_stream}` |
| `xuanxue-framework/config/identity_settings` | `framework/config/identity_settings` |
| `xuanxue-system/service/audit_service` | `framework/audit/audit_log.cppm` |
| `xuanxue-chart/**` | `modules/chart/{domain,service,mapper,client,controller}/` |
| `xuanxue-chat/**` | `modules/chat/{domain,service,mapper,controller}/` |
| `xuanxue-reading/**` | `modules/reading/{domain,service,mapper}/` |
| `xuanxue-admin/controller/{chart,chat,reading}` | `modules/{chart,chat}/controller/` |

The xuanxue identity slice (user/role/permission/session stores and its
`*_port` abstractions) was **not** migrated. CNet Nexus already implements that
capability; business modules consume `cnetnexus.framework.error`,
`cnetnexus.framework.pagination`, `cnetnexus.framework.audit` and
`cnetnexus.framework.security` instead.

## Namespaces

Business modules use `cnetnexus::<module>` and, for the six divination
contracts, `cnetnexus::chart::<algorithm>`. The algorithm segment is required:
`qi_men` and `zi_wei` each declare `calendar_kind`, `chart_input` and
`chart_result`, and `da_liu_ren`, `liu_yao` and `mei_hua` each declare
`cast_input` and `cast_result`, so a flat namespace would redefine types.

Module names follow `cnetnexus.<module>.<layer>.<name>`.

## ZhouYiLab integration

`chart` compiles the ZhouYiLab module set. Two facts drive the wiring and are
easy to get wrong:

1. `ZhouYiLabConfig.cmake` **excludes `json.cppm` from `ZhouYiLab_MODULE_FILES`**
   and serves it through the `nlohmann_json_modules` archive. Linking that
   archive alone produces no `nlohmann.json` BMI, so every consuming target must
   add `${ZhouYiLab_MODULE_BASE_DIR}/json.cppm` to its `FILE_SET CXX_MODULES`.
   The `ZHOYI` option of `cnetnexus_module()` does this.
2. `MAGIC_ENUM_USE_STD_MODULE` and `${ZhouYiLab_INCLUDE_DIR}` are required, and
   `ZhouYiLab::ZhouYiLabCore` must be linked via `$<TARGET_FILE:...>`.

`import nlohmann.json;` therefore stays as written in the migrated sources; it is
not replaced by `cnetmod.json`.

## Schema

`migrations/V005__chat_and_reading.sql` adds `chat_session`, `chat_message` and
`reading_cache`. `migrations/V006__chart_workspace.sql` adds
`chart_archive_index`, `chart_case`, `chart_tag` and `chart_case_tag`.

Both are ported from the xuanxue migrations with two deliberate changes:

- Timestamp columns are `DATETIME(6)` to match `V001__system_schema.sql`.
- Permission codes live in `sys_menu.permission` with `menu_type='F'`, because
  this project uses the RuoYi menu-as-permission model rather than xuanxue's
  separate `sys_permission` table.

`V007__chart_route_permissions.sql` seeds the five codes the chart controllers
declare but no earlier migration created (`chart:tag:manage`, `chart:batch:run`,
`chart:reading:run`, `chart:case:read`, `chart:archive:delete`).

`V008__grant_chart_chat_permissions.sql` grants chart/chat codes to `admin` and
`practitioner`. It is separate from the seeding migrations on purpose: no
migration seeds roles (the bootstrap creates them on first start), so grant
statements inside V005-V007 match zero rows at migration time. V008 is
re-runnable and only fills in missing links.

### Migrations must be re-runnable

Every migration is idempotent, because they may be replayed by hand:

- `V002`/`V004` guard their `ALTER TABLE` statements with `information_schema`
  lookups. Without that, replaying fails with `Duplicate column 'dept_id'` and
  `Can't DROP COLUMN 'ancestors'`.
- Permission inserts use `WHERE NOT EXISTS` rather than
  `INSERT ... ON DUPLICATE KEY UPDATE`. `sys_menu` has **no unique key on
  `permission`**, so `ON DUPLICATE KEY UPDATE` never fires and each replay would
  append another row. `sys_role_menu` does have a composite primary key, so
  `INSERT IGNORE` suffices there.

Verified on MariaDB 12.3: seven migrations replayed three times keep
`sys_menu` at 16 rows with no duplicates and 14 tables.

## XML statements

Complex queries stay in XML and load from one directory. CMake copies these into
`${CMAKE_CURRENT_BINARY_DIR}/mapper`:

| File | Owner | Notes |
|---|---|---|
| `ChartMapper.xml` | `chart_mapper_registry` | 9 statements validated at start-up |
| `ChatMapper.xml` | chat | 6 statements |
| `ReadingCacheMapper.xml` | reading | 2 statements |

## Dependencies that did not move

- **OpenAI completion adapter** (`xuanxue-reading/client/llm/openai_adapter.cpp`).
  `reading` exposes the abstract `llm_service`, and only
  `unavailable_llm_service` (`configured() == false`) exists, so every
  reading/chat endpoint currently answers `DEPENDENCY.UNAVAILABLE` without
  writing partial rows.
- **xuanxue identity stores**, by strategy.
- **In-memory backends**. CNet Nexus has no `storage.backend=memory` concept;
  the ORM mappers are the single implementation.
- **`/stats/overview` today-login count.** The count lives in identity, so chart
  exposes a `today_login_provider` seam; the composition root may inject one, and
  `nullptr` leaves the field at 0.
- **xuanxue `controller/system` endpoints** (logout, `/auth/me`, password
  change, audit logs, sessions, settings). CNet Nexus has no equivalent yet.

## Verification

```bash
cmake -S backend -B backend/build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/cnetmod/install;/path/to/zhouyilab/install" \
  -Dyaml_cpp_modules_DIR=/path/to/yaml_cpp_modules
cmake --build backend/build -j 24
ctest --test-dir backend/build --output-on-failure
```

Result on the migration branch: **0 errors, 28/28 tests passing**, and
`cnetnexus_server` links.

The 28 suites are the four original CNet Nexus suites plus 24 migrated ones
(7 framework, 13 chart, 4 reading). Suites were **not** weakened to pass: where a
xuanxue assertion encoded behaviour this codebase does not have, the expectation
was rewritten against the real contract and the change is recorded below.

End-to-end, against a live MariaDB 12.3 and Redis: the server starts, listens on
`0.0.0.0:8080`, and every migrated route answers its real status code rather
than 404 — `/health` 200, `/api/v1/capabilities` 200, `/api/v1/error_codes`
200, guarded routes 401 without a token. A `chart_case` row writes through to
`chart_case_tag` and reads back.

### Baseline defects found during the migration

- **The server could not start at all before this work.**
  `configure_authentication()` resolved `runtime.repository<T>("primary")`
  inside the middleware *factory*. cnetmod runs the middleware factory before
  auto-configuration, so MySQL was not registered yet, all five repositories
  failed, and `build()` returned `errc::invalid_argument`. The fix defers
  identity wiring until after `build()`; MySQL is still required.
- **`pool_size` is not a valid cnetmod MySQL key** (only `minimum_size` /
  `maximum_size`), so `build()` failed even with MySQL running.
- **`mysql_chart_case_mapper::set_tags` was declared but never defined.** It is
  called by the create and update paths. The memory mapper covered it, so tests
  passed and nothing constructed the MySQL mapper — the omission only surfaced
  when the composition root wired the real mapper. Ported from the xuanxue
  original.

### Assertions rewritten rather than relaxed

- **JWT expiry.** `cnetmod::security::verify_jwt` rejects a token whose `exp`
  has passed and returns no claims, so "read claims from an expired token" cannot
  hold. The suite now asserts rejection plus a separate `is_jwt_expired` check.
- **`zi_wei` target parameters.** Supplying only `target_year` is valid (it
  computes decade/annual cycles); month/day/hour must appear as a complete set.
  The suite covers all four directions.
- **Password hashing.** The iteration count is fixed, so the "non-positive
  iterations" case became "empty password".
- **Token estimation.** `prompt_render` uses `chat_domain::estimate_tokens`.

### Driving coroutines in tests

`cnetmod::io_context` must be `restart()`ed after `stop()` before the next
`run()`. Without it the second `run()` returns immediately and the coroutine
silently never executes, which makes assertions fail for the wrong reason.
