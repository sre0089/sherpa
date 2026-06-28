# Architecture

Sherpa is a modular C++ monolith. The CLI calls application services, which coordinate repository
scanning, language analysis, graph algorithms, and SQLite persistence.

The index application service scans files, dispatches them to the built-in tree-sitter C/C++
frontend, and atomically persists files, syntax-level symbols, include directives, and diagnostics.
Graph relationship resolution and queries remain intentionally deferred.
