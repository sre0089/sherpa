# ADR 0005: Bound impact traversal by relationship certainty

## Status

Accepted.

## Decision

Impact analysis operates on an immutable snapshot of the active repository graph and uses
breadth-first search with visited-node sets.

For a symbol, Sherpa follows resolved call relationships in reverse. For a file, Sherpa follows
resolved include relationships in reverse and separately follows reverse calls from definitions in
that file. The two traversals are not implicitly bridged through every symbol in an affected file;
doing so would rapidly overstate impact.

Ambiguous relationships that name a visited node as a candidate are returned as possible-impact
boundaries. They are not placed on the traversal queue. Unresolved relationships cannot be tied to
the selected target and are excluded.

Sherpa does not assign a low/medium/high risk score in this milestone. The current graph lacks the
runtime, ownership, test-coverage, and change-frequency data needed for a defensible score.

## Consequences

Results are deterministic, cycle-safe, evidence-bearing, and conservative about uncertainty. They
can miss impact hidden behind unresolved syntax or build configuration, and they intentionally
avoid multiplying uncertainty through ambiguous paths. The graph snapshot is reusable by future
path tracing and export milestones.
