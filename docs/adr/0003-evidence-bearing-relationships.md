# ADR 0003: Preserve uncertainty in graph relationships

## Status

Accepted

## Decision

Sherpa stores each `defines`, `calls`, and `includes` relationship with its source evidence,
resolution status, confidence, and resolver provenance. Ambiguous relationships retain ranked
candidate targets; unresolved relationships retain the original target text.

Call resolution considers only syntax-level definitions. It prefers qualified and lexical-scope
matches before repository-wide names. Member calls are low confidence without type information.
Local include resolution uses explicit path rules and does not guess system include paths.

## Consequences

Later graph queries can explain why an edge exists and avoid presenting heuristic guesses as exact.
The database contains more records than a resolved-only graph, and callers must explicitly decide
whether to include ambiguous or unresolved evidence.
