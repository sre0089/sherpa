# Project State

Last verified: 2026-06-30

This file is the concise handoff between development sessions. The repository and linked detailed
documentation remain authoritative. Update this file after every completed milestone or major work
session.

## Current milestone

- Milestone 12, public API foundation, is complete.
- The next proposed milestone is Milestone 13, plugin and extensibility model.
- Milestone 13 has not been approved or started.

## Repository status

- Repository: `https://github.com/sre0089/sherpa`
- Active branch: `main`
- State baseline: `1d46576` (`fix: constrain installed consumer configurations`)
- Before this handoff update, `main` matched `origin/main` and the working tree was clean.
- License: MIT

## Completed features

- C++20 modular-monolith foundation using CMake and vcpkg.
- Cross-platform CI for Ubuntu, macOS, and Windows/MSVC.
- Deterministic scanning of supported C and C++ source/header extensions.
- Tree-sitter C/C++ extraction of symbols, includes, calls, source ranges, and diagnostics.
- SQLite schema migrations through version 4.
- Evidence-bearing `defines`, `calls`, and `includes` relationships with resolution, confidence,
  provenance, and ranked ambiguity candidates.
- Read-only callers and callees queries.
- Evidence-bounded transitive impact analysis.
- Certainty-first directed call-path queries.
- Deterministic, versioned graph JSON export with atomic file replacement.
- Grouped `query symbol`, `query file`, `query callers`, and `query callees` commands.
- Version-1 query JSON success/error contract with stable exit statuses.
- Exact symbol disambiguation by qualified/short name, displayed signature, and definition file.
- Atomic incremental indexing:
  - unchanged files reuse cached parser output;
  - added and modified files are parsed;
  - deleted files leave the new snapshot;
  - relationships are rebuilt from complete raw analyses;
  - failed persistence preserves the prior active snapshot.
- Index phase/count metrics and an optional CSV benchmark executable.
- Version-1 public C++ API through `sherpa::api::Client`, covering indexing, symbol/file queries,
  callers/callees, impact analysis, directed paths, and graph export without exposing storage,
  parser, CLI, or presentation internals.
- Installable headers and static library with exported `Sherpa::Sherpa` CMake target,
  `find_package(Sherpa CONFIG REQUIRED)` support, build-tree and installed-package consumer tests,
  and CLI orchestration through the public facade.

## Latest relevant commits

- `1d46576` — make installed-package consumer tests correct for multi-config generators.
- `e288fab` — make public API selector tests portable under strict GCC warnings.
- `a4cdc9b` — public C++ API facade, package export, documentation, ADR, and consumer coverage.
- `e60b13c` — atomic incremental indexing, schema v4, metrics, tests, and benchmark tooling.

## Open bugs

- No open GitHub issues were present when this file was verified.
- No known failing local tests or CI jobs.
- Documented analysis and scalability limitations below are known constraints, not currently tracked
  bug reports.

## Next priorities

1. Present and agree on the exact Milestone 13 plugin/extensibility scope before implementation.
2. Define extension points and lifecycle boundaries without weakening the versioned public API or
   exposing storage/parser internals.
3. Decide plugin discovery, compatibility, isolation, registration, and failure semantics.
4. Keep CLI, public C++ API, JSON contracts, atomicity, and deterministic output backward-compatible.

## Key design decisions

- Sherpa is a modular monolith with explicit scanner, parser, analysis, application, storage,
  domain, CLI, and presentation responsibilities.
- Results preserve uncertainty instead of guessing: resolved, ambiguous, and unresolved
  relationships remain distinguishable and evidence-bearing.
- SQLite is an internal cache, not a public integration contract.
- Graph/query JSON formats are explicitly versioned and deterministic.
- Query readers open completed indexes read-only.
- Symbol selection is exact and deterministic; ambiguity requires explicit disambiguation.
- Confirmed dependency paths take precedence over possible paths.
- Incremental indexing reuses parser output but globally rebuilds relationships for cross-file
  correctness.
- Index publication and graph-file export use atomic replacement.
- The supported C++ entry point is `sherpa::api::Client`; infrastructure and application services
  remain internal.
- Public API version 1 carries a source-compatibility policy within the `0.x` line; no stable binary
  ABI is promised. Package consumers use the exported `Sherpa::Sherpa` CMake target.

Detailed decisions are recorded in [`docs/adr/`](docs/adr/), currently ADRs 0001–0009.

## Known technical debt and limitations

- Analysis is syntactic. Sherpa does not preprocess or compile code and lacks compiler-accurate
  types, overload resolution, templates, macros, conditionals, linkage, and virtual dispatch.
- Member-call and include resolution is intentionally conservative; external/system includes can
  remain unresolved.
- Only C and C++ are supported.
- Incremental scans still read and fingerprint every supported file, rebuild all relationships, and
  write a complete replacement snapshot.
- Query services load immutable graph snapshots into memory; large-repository scaling has not yet
  been characterized.
- The public API currently exposes concrete result/domain types and a static library; binary ABI
  stability, shared-library symbol visibility, and deprecation tooling are not yet provided.
- No plugin system, editor extension, network API, or LLM integration exists yet.
- No committed large-repository performance baseline, fuzzing, sanitizer matrix, or long-running
  stress suite exists.
- GitHub Actions currently warns that the pinned `lukka/run-vcpkg` action targets deprecated
  Node.js 20, although the workflow is still successful under GitHub's forced Node.js 24 runtime.
- Packaging and release automation for GitHub releases, Homebrew, and PyPI remain future work.

See [`docs/limitations.md`](docs/limitations.md) and
[`docs/database-format.md`](docs/database-format.md) for detailed constraints.

## Performance notes

- Unchanged files skip tree-sitter parsing by reusing schema-v4 cached symbols, includes, call
  sites, and diagnostics.
- Relationships are always rebuilt to prevent stale cross-file resolution.
- CLI indexing reports scan, cache-load, parse, relationship, persistence, and total timings.
- `SHERPA_BUILD_BENCHMARKS=ON` builds `sherpa_index_benchmark`, which emits CSV for one full and
  repeated unchanged indexes.
- Small-fixture verification confirms unchanged runs parse zero files; a representative
  large-repository baseline has not been recorded.

## Testing and CI status

- Local verification: `cmake --build --preset dev` and
  `ctest --preset dev --output-on-failure` passed 69/69 tests on 2026-06-30.
- Coverage includes unit, integration, CLI, migration, ambiguity, deterministic output,
  incremental reuse, deletion, relationship re-resolution, rollback behavior, public API behavior,
  build-tree consumption, and installed-package consumption.
- Latest feature CI: GitHub Actions run
  [`#29`](https://github.com/sre0089/sherpa/actions/runs/28441148071) completed successfully for:
  - `ubuntu-latest`
  - `macos-latest`
  - `windows-2022`
- The optional benchmark target builds locally but is not part of the default CI matrix.

## Remaining milestone roadmap

The roadmap is provisional; approve and complete one milestone at a time.

12. Public API foundation.
13. Plugin and extensibility model.
14. Editor/VS Code integration foundation.
15. Additional language support.
16. Packaging and release hardening for GitHub, Homebrew, and PyPI.
17. LLM and assistant integration.
