# ADR 0007: Publish a versioned deterministic graph export

## Status

Accepted.

## Decision

Sherpa publishes repository graph data through a versioned JSON export rather than treating the
SQLite database as an integration surface.

Version 1 export:

1. records a logical repository root of `"."` and repository-relative file paths;
2. assigns opaque node and edge identifiers from stable content facts instead of database row ids;
3. preserves relationship evidence, confidence, provenance, resolution, and ranked ambiguity
   candidates;
4. sorts nodes, edges, and candidates deterministically before rendering;
5. writes through a temporary sibling file and renames only after a complete successful render.

Absolute repository paths are intentionally excluded from the public contract.

## Consequences

Consumers receive a stable interchange format that is safer to diff, cache, and validate than raw
database access. Cross-machine exports for identical repositories remain byte-identical and do not
leak user-specific filesystem paths.

The strict schema means additive public fields require deliberate versioning work. Export ids are
stable for unchanged indexed content but are not a promise of eternal identity through major source
movement or schema evolution.
