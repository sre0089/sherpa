# Architecture

Sherpa is a modular C++ monolith. The CLI calls application services, which coordinate repository
scanning, language analysis, graph algorithms, and SQLite persistence.

The index application service scans files, dispatches them to the built-in tree-sitter C/C++
frontend, and atomically persists files, syntax-level symbols, include directives, and diagnostics.
The relationship resolver then creates file-to-symbol definitions, symbol-to-symbol calls, and
file-to-file includes with evidence, confidence, provenance, and retained ambiguity. User-facing
graph queries remain intentionally deferred.
