# ADR 0011: Use a versioned stdio protocol for editor integration

## Status

Accepted.

## Decision

Editor integrations communicate with a separate installed `sherpa-server` process using JSON-RPC
2.0 and `Content-Length` framing over stdin/stdout. The protocol is versioned independently from
the C++ API, plugin API, query JSON, graph JSON, and SQLite schema.

One server instance owns one initialized workspace. Requests execute serially through the public
C++ client, while the input loop continues reading cancellation notifications. Existing query and
graph JSON contracts are embedded unchanged in RPC results.

Cancellation is cooperative at the server boundary. Queued work can be skipped, and responses for
in-flight work can be suppressed, but the current scanner, parser, and persistence APIs are not
forcibly interrupted. Atomic indexing may therefore complete after cancellation.

The VS Code foundation uses a dependency-free JavaScript client and launches a configurable
`sherpa-server` executable. It provides command-driven indexing, status, and symbol queries rather
than claiming full Language Server Protocol support.

## Consequences

Editors gain a stable process boundary without linking Sherpa's C++ library or reading SQLite.
Crashes and C++ ABI changes are isolated from the editor host, and protocol compatibility can
evolve deliberately.

The server introduces a private JSON dependency and another installed executable. Full LSP,
fine-grained cancellation, unsaved-buffer analysis, multi-root ownership, navigation providers,
diagnostic publication, and extension packaging remain future work.
