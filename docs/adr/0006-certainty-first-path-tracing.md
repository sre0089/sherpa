# ADR 0006: Prefer certainty before path length

## Status

Accepted.

## Decision

`path <source> <target>` searches directed call relationships in two phases:

1. breadth-first search over resolved calls only;
2. if no path exists, breadth-first search over resolved calls and ambiguous candidate targets.

The first phase guarantees that any fully resolved path outranks every ambiguity-bearing path,
even a shorter one. Within each phase, breadth-first search returns a shortest path and visited-node
tracking makes cycles safe. Deterministically sorted adjacency lists make equal-length choices
repeatable.

A path containing an ambiguous edge is labeled `possible`. Unresolved calls are not traversable.
A disconnected pair is represented as `not_found`, not as a command failure.

## Consequences

Results do not imply that a short speculative route is more trustworthy than a longer
evidence-backed route. Users receive one concise path rather than a potentially exponential list.
The current command is symbol-to-symbol and call-only; mixed file/include path tracing can be added
later with explicit edge-type controls.
