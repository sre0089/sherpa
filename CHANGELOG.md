# Changelog

All notable user-facing changes are recorded here. Sherpa follows semantic versioning while it is
pre-1.0: minor releases may include deliberate source-level API changes documented in release
notes, while patch releases preserve the current public API version.

## [Unreleased]

## [0.1.0] - 2026-07-02

### Added

- Local C, C++, and Python repository indexing with tree-sitter.
- Evidence-bearing definitions, calls, includes/imports, diagnostics, and uncertainty.
- Symbol, file, callers, callees, impact, directed-path, and deterministic graph queries.
- Atomic schema-v4 incremental indexing with parser-output reuse and timing metrics.
- Versioned public C++ API, in-process plugin observers, JSON-RPC editor server, and VS Code
  integration foundation.
- Tag-driven native release archives, checksums, provenance, Homebrew formula generation, and
  `sherpa-code` Python wheel packaging.

[Unreleased]: https://github.com/sre0089/sherpa/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/sre0089/sherpa/releases/tag/v0.1.0
