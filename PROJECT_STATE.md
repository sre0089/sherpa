# Project State

Last verified: 2026-06-30

This file is the concise handoff between development sessions. The repository and linked detailed
documentation remain authoritative. Update this file after every completed milestone or major work
session.

## Current milestone

- Milestone 11, atomic incremental indexing, is complete.
- The next proposed milestone is Milestone 12, public API foundation.
- Milestone 12 has not been approved or started.

## Repository status

- Repository: `https://github.com/sre0089/sherpa`
- Active branch: `main`
- State baseline: `e60b13c` (`feat: add atomic incremental indexing`)
- At verification time, `main` matched `origin/main` and the working tree was clean.
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

## Latest relevant commits

- `e60b13c` — atomic incremental indexing, schema v4, metrics, tests, and benchmark tooling.
- `228ba03` — default optional symbol selectors for portable aggregate initialization.
- `1814591` — portable selector argument lifetimes.
- `1a22dbc` — deterministic signature/file symbol disambiguation.
- `d498abb` — versioned query JSON contract.
- `91661c1` — grouped query engine MVP.
- `29cf9af` — deterministic graph export.

## Open bugs

- No open GitHub issues were present when this file was verified.
- No known failing local tests or CI jobs.
- Documented analysis and scalability limitations below are known constraints, not currently tracked
  bug reports.

## Next priorities

1. Present and agree on the exact Milestone 12 public API scope before implementation.
2. Define the supported C++ library boundary without exposing SQLite, tree-sitter, or CLI details.
3. Decide API compatibility/versioning, install/export targets, CMake package configuration, and
   consumer-level tests.
4. Keep CLI behavior and JSON contracts backward-compatible while moving orchestration behind the
   public boundary.
5. Update this file and add an ADR if Milestone 12 establishes a durable compatibility policy.

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

Detailed decisions are recorded in [`docs/adr/`](docs/adr/), currently ADRs 0001–0008.

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
- No installed public C++ SDK, exported CMake package, stable library ABI/API policy, plugin system,
  editor extension, network API, or LLM integration exists yet.
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

- Local verification: `ctest --preset dev --output-on-failure` passed 65/65 tests on 2026-06-30.
- Coverage includes unit, integration, CLI, migration, ambiguity, deterministic output,
  incremental reuse, deletion, relationship re-resolution, and rollback behavior.
- Latest feature CI: GitHub Actions run
  [`#21`](https://github.com/sre0089/sherpa/actions/runs/28422471402) completed successfully for:
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
