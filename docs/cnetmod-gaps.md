# cnetmod integration gaps

This file lists only capabilities that are still missing from cnetmod. Fixed
items are removed instead of being kept as stale architecture advice.

## Open: application-managed schema migration provider

`cnetmod::orm::schema_migration_runner` correctly owns ordering, checksums,
locking and semantic SQL rendering, but Application does not yet expose a
MySQL/PostgreSQL migration service that supplies the runner callbacks during
managed startup. Nexus currently has versioned SQL migrations, so wiring the
runner locally would require duplicating provider-specific lease, lock and
history-table code in the application.

The framework-level completion should provide an Application component that:

- runs before HTTP listeners accept traffic;
- leases the configured relational service on the current event loop;
- owns the migration history table and advisory lock;
- accepts semantic migrations and reports failures through `build_error` or
  managed-service startup diagnostics.

Until that provider exists, deployment must apply `backend/migrations` before
starting Nexus. Nexus must not introduce a second migration engine.

## Resolved upstream capabilities now used by Nexus

- Redis convenience operations honor pool timeouts, so an unavailable Redis
  cannot suspend requests forever.
- durable file writes are available through `file_write_durability::flushed`;
  Nexus no longer stores chart archives on disk.
- logical-delete policy is repository/model scoped; `deleted` and nullable
  `deleted_at` models coexist without global configuration.
- chat models support explicit multi-provider, multi-key composition.
- router endpoint metadata owns anonymous/optional/permission policy and the
  router emits 404/405 after route matching.
- dynamic SSE paths use `request_context::with_sse()` and the supplied
  `cnetmod::http::sse_stream`, including the total-duration watchdog.
