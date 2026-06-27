# Architecture

Sherpa is a modular C++ monolith. The CLI calls application services, which coordinate repository
scanning, language analysis, graph algorithms, and SQLite persistence.

Milestone 1 implements only the CLI, index application service, scanner, and file-index storage.
Language parsing and graph queries are intentionally deferred.
