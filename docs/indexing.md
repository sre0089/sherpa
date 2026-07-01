# Indexing

Run `sherpa index <repository>` to create or refresh the local index. The first run parses every
supported C, C++, and Python file. Later runs compare content fingerprints with the active
snapshot:

- unchanged files reuse stored symbols, includes, call sites, and diagnostics;
- added and modified files are parsed;
- deleted files are omitted from the new snapshot;
- relationships are rebuilt across the complete repository for cross-file correctness.

The CLI reports added, modified, unchanged, deleted, parsed, and reused file counts. It also reports
elapsed milliseconds for scanning, cache loading, parsing, relationship resolution, persistence,
and the complete operation. These timings are diagnostic measurements rather than a stable
machine-readable interface.

Index publication is atomic. Parsing and relationship resolution occur before persistence, and the
new completed snapshot replaces the prior active snapshot in one SQLite transaction. Queries never
observe a partial update.

Sherpa currently fingerprints file contents during every scan and rewrites the completed snapshot,
even when all files are unchanged. See the [index benchmark](../benchmarks/README.md) for the
reproducible baseline used to evaluate future optimizations.

Python qualified names begin with the repository-relative module name, followed by lexical scopes
using Sherpa's existing `::` separator. For example, `app/service.py` produces
`app.service::Service::process`. Package `__init__.py` files use the package name. This keeps mixed
repositories deterministic without changing query or graph contracts.
