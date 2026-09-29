# cnetNexus Repository Instructions

## Backend architecture

- Backend modules follow a strict Spring-style dependency direction: `controller -> service -> mapper -> domain`.
- Controllers own HTTP concerns only: request parsing, authentication context extraction, validation at the transport boundary, invoking one application service, and writing the response.
- Services own business use cases and orchestration. They must not import HTTP/router modules or write responses.
- Mappers own persistence contracts and persistence implementation. Mapper code must not import services or controllers.
- ORM entities and database query projections belong in `domain/`; HTTP request/response contracts belong in `dto/`.
- Framework-specific request-scoped repository creation belongs in `bootstrap/`, which injects a service resolver into the controller. Controllers must not access mapper registries, repositories, ORM APIs, or the application runtime directly.

## Business-first naming

- Name services after a concrete business use case or domain capability, for example `chart_generation_service`, `authorization_service`, or `reading_service`.
- Do not name services after transport or implementation mechanics. Generic names such as `batch_service`, `common_service`, `data_service`, `base_service`, `manager_service`, and `helper_service` are forbidden.
- A class name must still communicate its business responsibility when read without its directory path. If the name only says how data is transported, grouped, cached, or dispatched, rename it before merging.

## Architecture gate

- Any new architecture rule must be enforced in `backend/testing/framework_layering_boundaries.cpp` where it can be checked statically.
- Do not weaken an existing boundary test to make a build pass; repair the dependency direction instead.
