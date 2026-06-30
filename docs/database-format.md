# Database format

The SQLite database is an internal cache, not Sherpa's public API. Its schema is versioned through
ordered files in `migrations/`.

Schema version 4 stores repositories, indexing runs, source files, syntax-level symbols, include
directives, raw call sites, parser diagnostics, relationships, and ambiguous relationship
candidates. Raw per-file analysis allows unchanged files to skip parsing while relationships are
rebuilt for cross-file correctness. A successful indexing transaction atomically replaces the
repository's prior active run. Versions 1 through 3 are migrated automatically; version-3 snapshots
are reparsed once because they do not contain raw call sites.

Graph queries open the database read-only and require schema version 4. They never create or migrate
an index; indexing remains the only write path.

The `content_fingerprint` column currently uses FNV-1a 64-bit for change detection. Every supported
file is read to calculate it during a scan; it is not a cryptographic integrity check.

Sherpa's public interchange format is the graph export JSON document, not the SQLite schema. Tools
that need a stable integration surface should consume the versioned export contract instead of
reading database tables directly.
