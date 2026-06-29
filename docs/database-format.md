# Database format

The SQLite database is an internal cache, not Sherpa's public API. Its schema is versioned through
ordered files in `migrations/`.

Schema version 3 stores repositories, indexing runs, source files, syntax-level symbols, include
directives, parser diagnostics, relationships, and ambiguous relationship candidates. A successful
indexing transaction atomically replaces the repository's prior active run. Version 1 and version 2
databases are migrated automatically.

Graph queries open the database read-only and require schema version 3. They never create or migrate
an index; indexing remains the only write path.

The `content_fingerprint` column currently uses FNV-1a 64-bit for fast change detection. It is not
a cryptographic integrity check.

Sherpa's public interchange format is the graph export JSON document, not the SQLite schema. Tools
that need a stable integration surface should consume the versioned export contract instead of
reading database tables directly.
