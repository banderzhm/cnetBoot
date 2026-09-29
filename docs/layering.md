# Layering rules

Every bounded context uses sibling layers:

```text
modules/<context>/
  controller/  HTTP routing and protocol adaptation
  dto/         request/response transport types and projection
  service/     named use cases and business orchestration
  domain/      entities, value objects, rules and ORM records
  mapper/      repository implementation and XML statements
```

The dependency rule is `controller + dto -> service -> domain <- mapper`. Bootstrap may see every
layer only to compose objects. A mapper must never resolve its own dependencies from Application.

## Naming

Service names describe a concrete capability: `authorization_service`, `chat_service`,
`login_guard`. Mechanical grouping names such as `batch_service`, `common_service`, `base_service`
or `manager_service` are rejected because they hide ownership and grow into unrelated collections.

## AI boundary

Provider/model types stay in `modules/ai/service/model`. The rest of the application uses the
provider-neutral `llm_service`, `llm_request`, and `llm_response` contracts. Provider registration,
multi-key strategy and fallback are explicit composition decisions.

## Automated enforcement

`testing/framework_layering_boundaries.cpp` scans the production tree and fails when:

- an unexpected built-in business module appears;
- vertical-domain terminology leaks into the base template;
- a DTO is placed inside a controller directory;
- an ORM record is placed inside a mapper directory;
- a service uses a forbidden generic name;
- required system/chat DTO or AI model boundaries disappear.

Run it through CTest after every architecture change.
