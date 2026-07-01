# Editor integration

Sherpa installs `sherpa-server`, a long-running JSON-RPC 2.0 process intended for editor and tool
integration. It reads and writes standard `Content-Length` framed messages over stdin/stdout.
Protocol logs and diagnostics must use stderr so stdout remains machine-readable.

The independently versioned protocol is currently version 1. Clients discover the version and
capabilities through `initialize`:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "repository_path": "/workspace",
    "database_path": "/optional/index.sqlite"
  }
}
```

The result contains `protocol_version`, `server_version`, and advertised methods. One server owns
one initialized workspace and executes workspace operations serially.

## Methods

| Method | Required parameters | Result |
| --- | --- | --- |
| `initialize` | `repository_path` | Version and capabilities |
| `workspace/status` | None | Initialization and workspace state |
| `workspace/index` | None | Index counts, timings, diagnostics count, and warnings |
| `query/symbol` | `name` | Version-1 symbol query JSON |
| `query/file` | `path` | Version-1 file query JSON |
| `query/callers` | `name` | Version-1 call query JSON |
| `query/callees` | `name` | Version-1 call query JSON |
| `query/impact` | `target` | Version-1 impact JSON |
| `query/path` | `source`, `target` | Version-1 path JSON |
| `graph/get` | None | Version-1 deterministic graph JSON |
| `shutdown` | None | `null` |

Symbol methods accept optional `signature` and `file`. Path accepts optional
`source_signature`, `source_file`, `target_signature`, and `target_file`.

After a successful `shutdown` request, clients send an `exit` notification. Exiting without
shutdown produces a nonzero server exit status.

## Cancellation

Clients cancel a request with the `$/cancelRequest` notification and the original JSON-RPC `id`.
The server reads cancellation concurrently while executing operations on one serial worker.

Sherpa's current public API does not provide cooperative cancellation inside scanning, parsing, or
SQLite work. Cancellation therefore prevents queued work or suppresses the response to in-flight
work; it does not forcibly interrupt an active core operation. In particular, a cancelled indexing
request may still atomically publish its completed snapshot. Cancelled requests return JSON-RPC
error code `-32800`.

## Errors and limits

Standard JSON-RPC parse, request, method, parameter, and internal error codes are used. Sherpa
failures use the range `-32010` through `-32014` and include a stable `sherpa_code` in error data.
Requests before initialization return `-32001`. Messages are limited to 64 MiB.

| Code | Meaning |
| --- | --- |
| `-32001` | Server not initialized |
| `-32010` | Repository unavailable |
| `-32011` | Index unavailable |
| `-32012` | Symbol, file, or impact target not found |
| `-32013` | Ambiguous symbol; candidates are included in error data |
| `-32014` | In-process plugin failure |

The protocol does not expose SQLite, C++ application services, or plugin objects. Query result
objects reuse the existing versioned JSON contracts.

## VS Code

[`editors/vscode/`](../editors/vscode/) contains the initial dependency-free VS Code extension. It
supports workspace indexing, status inspection, and symbol queries. Configure
`sherpa.serverPath` when `sherpa-server` is not on `PATH`; `sherpa.databasePath` optionally selects
an explicit index.

This milestone does not implement LSP navigation, unsaved document overlays, incremental
document synchronization, source diagnostics publication, multi-root workspace ownership, or
marketplace packaging.
