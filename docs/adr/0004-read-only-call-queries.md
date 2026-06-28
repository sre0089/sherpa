# ADR 0004: Keep graph queries read-only and deterministic

## Status

Accepted.

## Decision

`callers` and `callees` use a dedicated application service over the existing SQLite relationship
model. The service opens only an existing schema-version-3 index in read-only mode. It selects
definitions by exact qualified name, then exact short name, and rejects multiple matches.

The application result preserves resolution, confidence, provenance, source evidence, and
candidates. Text and JSON are renderings of that same result.

## Consequences

Queries cannot silently create, migrate, or refresh an index. This keeps reads predictable and
prevents a typo from producing an empty database. Overloaded symbols require future explicit
disambiguation support, but results never depend on SQLite row order. A single application model
also gives future API and editor integrations a path that does not depend on parsing CLI text.
