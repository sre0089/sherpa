# Database format

The SQLite database is an internal cache, not Sherpa's public API. Its schema is versioned through
ordered files in `migrations/`.

Schema version 3 stores repositories, indexing runs, source files, syntax-level symbols, include
directives, parser diagnostics, relationships, and ambiguous relationship candidates. A successful
indexing transaction atomically replaces the repository's prior active run. Version 1 and version 2
databases are migrated automatically.

The `content_fingerprint` column currently uses FNV-1a 64-bit for fast change detection. It is not
a cryptographic integrity check.
