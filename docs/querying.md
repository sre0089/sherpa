# Querying the graph

Index a repository before querying it:

```sh
sherpa index /path/to/repository
sherpa callees namespace::function --repo /path/to/repository
sherpa callers namespace::function --repo /path/to/repository
```

`--repo` defaults to the current directory. `--database` selects an explicit index instead of the
platform cache. Queries are read-only and fail if the repository has no completed index.

## Symbol selection

Sherpa first searches definition qualified names for an exact match. If none exist, it searches
definition short names for an exact match. Zero matches is an error. Multiple matches is an
ambiguity error that lists candidates; Sherpa never chooses by row order.

## Results

`callees` reports every stored call site from the selected definition, including ambiguous and
unresolved targets. `callers` reports relationships whose resolved target is the selected
definition, plus ambiguous relationships that retain it as a candidate.

Text is the default output. `--format json` emits a stable top-level object containing `query`,
`symbol`, and `calls`. Each call includes its caller, optional resolved callee, original target
text, resolution, confidence, provenance, source evidence, and ambiguity candidates.

Successful empty results exit with status 0. Errors use these statuses:

| Status | Meaning |
| ---: | --- |
| 2 | Invalid command usage |
| 3 | Repository unavailable |
| 4 | Index unavailable |
| 5 | Symbol not found |
| 6 | Symbol ambiguous |
| 10 | Internal failure |
