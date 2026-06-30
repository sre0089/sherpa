# ADR 0008: Atomic incremental indexing

## Status

Accepted.

## Decision

Sherpa stores raw per-file parser output, including call sites, in each active index snapshot.
During the next index:

1. every discovered file receives a content fingerprint;
2. unchanged files reuse their stored parser output;
3. added and modified files are parsed;
4. missing prior files are treated as deleted;
5. relationships are rebuilt from the complete combined parser output;
6. a new snapshot is written and activated in one SQLite transaction.

An analysis-cache version on each run prevents reuse when a schema migration cannot reconstruct the
complete parser cache.

## Rationale

Cross-file changes can alter call and include resolution even when the source file containing a
relationship did not change. Reusing final relationships would therefore risk stale answers.
Rebuilding relationships is cheaper than reparsing every file and preserves correctness.

Snapshot replacement retains the existing consistency model: readers see either the previous
completed index or the new completed index, never a partially updated graph.

## Consequences

Unchanged indexing still scans and fingerprints every supported file, rebuilds relationships, and
writes a new snapshot. More advanced metadata shortcuts, partial relationship invalidation, and
in-place persistence remain possible future optimizations after benchmarks justify their cost.
