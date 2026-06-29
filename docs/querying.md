# Querying the graph

Index a repository before querying it:

```sh
sherpa index /path/to/repository
sherpa callees namespace::function --repo /path/to/repository
sherpa callers namespace::function --repo /path/to/repository
sherpa impact src/component.cpp --repo /path/to/repository
sherpa impact namespace::function --repo /path/to/repository
sherpa path namespace::caller namespace::callee --repo /path/to/repository
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

## Impact analysis

`impact` accepts an exact repository-relative file path or a symbol. An exact file match takes
precedence; otherwise symbol selection uses the qualified-name and short-name rules above. Absolute
paths inside the repository are also accepted.

Symbol impact walks resolved incoming calls breadth-first. File impact performs two independent
walks: resolved incoming includes, and resolved incoming calls seeded by every definition in the
target file. Results contain the shortest discovered dependency step and depth. Visited sets make
cycles safe.

Ambiguous incoming calls and includes are shown under `possible`, but Sherpa does not continue
through them. This prevents uncertain relationships from being presented as confirmed transitive
impact. Resolved edges retain their confidence and provenance. Sherpa reports evidence and counts
rather than assigning an arbitrary risk rating.

## Dependency paths

`path <source> <target>` finds a directed function-call path from the source definition to the
target definition. Both endpoints use the standard exact symbol-selection rules.

Sherpa first performs breadth-first search using only resolved calls. If a confirmed path exists,
the shortest such path is returned even when a shorter ambiguous route also exists. Only when no
confirmed path exists does Sherpa search resolved calls plus retained ambiguity candidates and
label the result `possible`. Unresolved calls are excluded because they cannot be tied to a target
node.

Each step contains its source and target symbols, resolution, confidence, provenance, and source
location. Cycles are safe. A source queried against itself is a confirmed zero-step path. A
disconnected pair returns `not_found` with exit status 0 because absence of a path is a valid query
result.

Successful empty results exit with status 0. Errors use these statuses:

| Status | Meaning |
| ---: | --- |
| 2 | Invalid command usage |
| 3 | Repository unavailable |
| 4 | Index unavailable |
| 5 | File or symbol not found |
| 6 | Symbol ambiguous |
| 10 | Internal failure |
