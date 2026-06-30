# ADR 0009: Publish a versioned C++ facade

## Status

Accepted.

## Decision

Sherpa's supported in-process boundary is the C++20 `sherpa::api::Client` facade. It accepts
repository and selector request values, returns evidence-bearing domain values, and translates
internal failures into stable API error categories.

The facade covers existing indexing and graph-query capabilities. It does not expose SQLite,
tree-sitter, CLI, rendering, or concrete application-service types. The CLI uses the same facade so
embedded and command-line behavior share one orchestration boundary.

The public contract has an explicit API version independent of the SQLite and JSON schema versions.
During pre-1.0 development, source-breaking changes increment both the API version and project
minor version. ABI compatibility is not promised.

Sherpa installs the supported headers, library, and a CMake package exporting `Sherpa::Sherpa`.
Consumer tests validate both the build-tree alias and the installed package.

## Consequences

Library consumers can index and query repositories without parsing CLI output or depending on the
database schema. Internal modules remain free to evolve behind one translation layer.

The initial public values use C++ standard-library types, so consumers must rebuild across releases
and use a compatible C++ toolchain. Plugin ABI design, a C interface, and long-term binary
compatibility remain separate future decisions.
