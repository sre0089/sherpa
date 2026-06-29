# Query JSON contract

Sherpa's machine-readable query output is selected with `--format json`. The contract is versioned
independently from Sherpa releases so scripts can reject incompatible output explicitly.

## Successful results

Every successful query returns one JSON object on standard output followed by a newline:

```json
{
  "schema_version": 1,
  "ok": true,
  "query": "symbol"
}
```

The remaining fields depend on the query:

| `query` | Result fields |
| --- | --- |
| `symbol` | `symbol`, `callers`, `callees` |
| `file` | `path`, `definitions`, `incoming_includes`, `outgoing_includes` |
| `callers`, `callees` | `symbol`, `calls` |
| `impact` | `target`, `confirmed`, `possible` |
| `path` | `source`, `target`, `status`, `steps` |

Empty results use empty arrays and still exit with status 0. A path result with
`"status":"not_found"` is also a successful query.

## Errors

When JSON output was requested, a failure returns one JSON object on standard output followed by a
newline. Standard error remains empty:

```json
{
  "schema_version": 1,
  "ok": false,
  "error": {
    "code": "not_found",
    "message": "symbol not found: missing",
    "candidates": []
  }
}
```

`candidates` contains complete symbol objects for `ambiguous_symbol` and is empty for other errors.
The process also returns a nonzero status:

| Status | Error code | Meaning |
| ---: | --- | --- |
| 2 | `invalid_usage` | Invalid command syntax or options |
| 3 | `repository_unavailable` | Repository path cannot be used |
| 4 | `index_unavailable` | No completed readable index exists |
| 5 | `not_found` | Requested file, symbol, or impact target does not exist |
| 6 | `ambiguous_symbol` | More than one definition matches |
| 10 | `internal_failure` | Unexpected query failure |

Human-readable output keeps errors on standard error. Exit statuses are identical in both formats.

## Compatibility policy

Within schema version 1, existing fields keep their meaning and type. New optional fields may be
added. Consumers should ignore unknown fields and use `schema_version`, `ok`, and either `query` or
`error.code` for dispatch. Removing a field, changing a field's type or meaning, or restructuring
the document requires a new schema version.
