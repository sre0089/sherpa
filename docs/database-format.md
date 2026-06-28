# Database format

The SQLite database is an internal cache, not Sherpa's public API. Its schema is versioned through
ordered files in `migrations/`.

Schema version 2 stores repositories, indexing runs, source files, syntax-level symbols, include
directives, and parser diagnostics. A successful indexing transaction atomically replaces the
repository's prior active run. Version 1 databases are migrated automatically.

The `content_fingerprint` column currently uses FNV-1a 64-bit for fast change detection. It is not
a cryptographic integrity check.
