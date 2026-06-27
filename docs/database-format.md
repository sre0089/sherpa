# Database format

The SQLite database is an internal cache, not Sherpa's public API. Its schema is versioned through
ordered files in `migrations/`.

Milestone 1 stores repositories, indexing runs, and discovered source files. A successful indexing
transaction atomically replaces the repository's prior active run.

The `content_fingerprint` column currently uses FNV-1a 64-bit for fast change detection. It is not
a cryptographic integrity check.
