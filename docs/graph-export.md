# Graph export

Sherpa's graph export is the public, versioned interchange format for repository graph data.

## Command

```sh
sherpa export graph.json --repo /path/to/repository
```

`--repo` defaults to the current directory. `--database` selects an explicit index file. The output
path is required. Existing files are rejected unless `--force` is provided.

## Contract

Version 1 exports:

- `schema_version`
- generator metadata
- a logical repository root of `"."`
- file and definition-symbol nodes
- `defines`, `calls`, and `includes` edges
- relationship evidence, confidence, provenance, and ranked ambiguity candidates

File paths are repository-relative. Nodes and edges use opaque identifiers derived from stable graph
facts so consumers do not depend on SQLite row ids or host-specific absolute paths.

## Determinism

For an unchanged index, export bytes are deterministic:

- file paths remain repository-relative;
- nodes and edges are sorted by stable ids;
- ambiguous candidates retain stable rank ordering;
- the CLI writes to a temporary sibling file and renames on success.

This makes exported graphs suitable for diffing, caching, and reproducible downstream processing.

## Compatibility

The JSON schema for version 1 is defined in [graph-export-schema-v1.json](graph-export-schema-v1.json).
Future breaking changes require a schema-version increment rather than silent contract drift.
