# Architecture

Sherpa is a modular C++ monolith. The CLI calls application services, which coordinate repository
scanning, language analysis, graph algorithms, and SQLite persistence.

The index application service scans files, dispatches them to the built-in tree-sitter C/C++
frontend, and atomically persists files, syntax-level symbols, include directives, and diagnostics.
The relationship resolver then creates file-to-symbol definitions, symbol-to-symbol calls, and
file-to-file includes with evidence, confidence, provenance, and retained ambiguity.

All symbol-based application services use one shared selector over the immutable graph snapshot.
It applies deterministic exact qualified-name or short-name rules, then optional exact signature
and repository-relative file filters. The call query service retrieves incoming or outgoing call
relationships for the selected graph node. The CLI renders the same query result as text or JSON;
presentation logic does not issue SQL.

Impact analysis loads the active repository graph into an immutable snapshot and performs
deterministic breadth-first traversals in application memory. Symbol impact follows reverse call
edges. File impact independently follows reverse include edges and seeds call traversal from
definitions in the target file. Resolved edges are traversed; ambiguous edges are retained as
unexpanded possible-impact boundaries.

Path tracing reuses the same graph-loading and symbol-selection support. It searches the directed
resolved call graph first, then searches resolved plus ambiguous candidate edges only when no
confirmed path exists. Both searches use deterministic breadth-first traversal and reconstruct
source evidence from predecessor edges.

Graph export reuses the same immutable query graph and transforms it into a versioned public JSON
document. Export nodes and edges receive opaque, content-derived identifiers built from stable
repository-relative facts rather than SQLite row ids or absolute paths. Output is sorted
deterministically before rendering, and the CLI writes through a same-directory temporary file
before renaming so callers never observe a partial export.
