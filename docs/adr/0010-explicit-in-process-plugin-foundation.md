# ADR 0010: Begin with explicit in-process plugins

## Status

Accepted.

## Decision

Sherpa's first plugin boundary is a separately versioned C++ interface registered explicitly by an
embedding host. A registry validates stable plugin IDs, duplicate registration, and exact plugin
API compatibility before constructing `sherpa::api::Client`.

The registry caches validated descriptors and holds shared ownership of plugin instances. Ordinary
C++ object construction and destruction define plugin lifetime; no additional load/unload protocol
is introduced.

Version-1 plugins receive deterministic before and after notifications containing only the public
operation kind and repository paths. Hooks execute in registration order. They cannot mutate
requests, results, parser state, persistence, or graph output.

A before-hook exception aborts the core operation. An after-hook exception is reported after the
core operation has completed. Both become a public plugin-failure error carrying the plugin ID and
completion state. Hook phases are fail-fast, and failed core operations do not invoke after hooks.

Plugins are linked into the host process. Sherpa does not load shared libraries or promise a plugin
ABI. Dynamic discovery and stable binary loading require a future C ABI or equivalent boundary and
are deferred.

## Consequences

Embedding applications can add telemetry, auditing, and operation-level integration without
depending on internal modules. Deterministic ordering, index atomicity, and existing result
contracts remain under Sherpa's control.

The initial surface is intentionally narrow. Parser replacement, custom CLI commands, graph
mutation, process isolation, and dynamic installation are not supported by this version.
