# ADR 0012: Add Python through the built-in language frontend boundary

## Status

Accepted

## Decision

Sherpa indexes `.py` and `.pyi` with a pinned tree-sitter-python grammar and a dedicated extractor
behind the existing tree-sitter frontend facade. Python qualified names begin with their
repository-relative module and use `::` for lexical scopes. Imports reuse the existing
language-neutral dependency record and resolve only to indexed repository modules.

Call resolution uses language families. C and C++ remain one family; Python is separate.
Cross-language calls require future explicit evidence and are not inferred from matching names.
The schema-v4 cache and version-1 query, graph, public API, and editor contracts remain unchanged.

## Consequences

Mixed repositories retain deterministic selection and output without a migration. Relative and
absolute local imports can produce evidence-bearing file dependencies, while installed modules
remain unresolved. Python analysis is syntactic: aliases, environment-dependent import paths,
receiver types, decorators, dynamic dispatch, and runtime behavior are intentionally deferred.
