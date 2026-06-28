# Architecture

Sherpa is a modular C++ monolith. The CLI calls application services, which coordinate repository
scanning, language analysis, graph algorithms, and SQLite persistence.

The index application service scans files, dispatches them to the built-in tree-sitter C/C++
frontend, and atomically persists files, syntax-level symbols, include directives, and diagnostics.
The relationship resolver then creates file-to-symbol definitions, symbol-to-symbol calls, and
file-to-file includes with evidence, confidence, provenance, and retained ambiguity.

The call query application service opens an existing index read-only, selects one definition using
deterministic exact-name rules, and retrieves incoming or outgoing call relationships. The CLI
renders the same query result as text or JSON; presentation logic does not issue SQL.

Impact analysis loads the active repository graph into an immutable snapshot and performs
deterministic breadth-first traversals in application memory. Symbol impact follows reverse call
edges. File impact independently follows reverse include edges and seeds call traversal from
definitions in the target file. Resolved edges are traversed; ambiguous edges are retained as
unexpanded possible-impact boundaries.
