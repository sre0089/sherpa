# Architecture

Sherpa is a modular C++ monolith. The CLI and embedded consumers call the public API facade, which
coordinates application services for repository scanning, language analysis, graph algorithms,
and SQLite persistence.

Embedded consumers and the CLI enter through the versioned `sherpa::api::Client` facade. The
facade exposes repository requests, selectors, evidence-bearing result values, and stable error
categories while keeping application services, SQLite, tree-sitter, and presentation details
internal. Installed consumers link the exported `Sherpa::Sherpa` CMake target.

The index application service scans and fingerprints files, reuses complete cached analysis for
unchanged files, and dispatches added or modified files to the built-in tree-sitter C/C++ frontend.
The relationship resolver rebuilds file-to-symbol definitions, symbol-to-symbol calls, and
file-to-file includes across the combined analysis with evidence, confidence, provenance, and
retained ambiguity. The new snapshot is published atomically.

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
