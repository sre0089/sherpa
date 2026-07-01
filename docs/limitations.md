# Analysis limitations

Sherpa's C/C++ and Python frontends use tree-sitter and report syntactic evidence. They do not run a
preprocessor, compiler, or Python interpreter.

The public C++ API is source-versioned but does not provide binary compatibility. Sherpa currently
installs a static library, exposes C++ standard-library types, and requires consumers to rebuild
against each release. The plugin API supports explicitly linked in-process observers only. There is
no stable C interface, plugin ABI, shared-library discovery, process isolation, parser extension,
or request/result mutation.

The editor server processes one initialized workspace serially. Cancellation does not interrupt
active scanner, parser, or SQLite work; it skips queued work or suppresses an in-flight response.
The VS Code extension is command-driven and does not yet provide LSP navigation, unsaved-buffer
analysis, source diagnostic publication, multi-root ownership, or marketplace packaging.

Current results therefore do not resolve:

- overloaded functions or methods;
- templates and instantiated symbols;
- macro-expanded declarations;
- compiler conditionals and build flags;
- types, linkage, or virtual dispatch;
- compiler-accurate function calls or include search paths.

Python analysis extracts functions, async functions, classes, methods, displayed signatures,
calls, imports, ranges, and parser diagnostics. Local absolute and relative imports resolve against
repository module paths. Sherpa does not model environment-dependent `sys.path`, installed
packages, namespace-package configuration, import aliases, decorators, inferred receiver types,
monkey patching, or runtime dispatch. Attribute calls therefore use conservative name matching
unless `self` or `cls` provides an exact lexical method scope. `.pyi` files are indexed as
syntax-level definitions; a matching `.py` and `.pyi` module can create deliberate ambiguity.

Malformed files are indexed when possible. Parser errors are retained as diagnostics alongside any
symbols that could still be extracted.

Incremental indexing still reads every supported file to calculate its fingerprint, rebuilds all
relationships, and writes a complete replacement snapshot. It skips tree-sitter parsing for
unchanged files but does not yet use filesystem metadata shortcuts or partial graph invalidation.

Call resolution currently prefers exact lexical or qualified names, then unique repository-wide
names. Member calls without type information are low confidence. Overloads remain ambiguous when
syntax alone cannot select one definition. Quoted includes use relative, repository-root, or unique
suffix matching; system includes remain unresolved.

Call resolution is restricted to the source language family: C and C++ can resolve together, while
Python never resolves to C/C++ merely because a name matches. `callers` and `callees` select
definitions by exact qualified name first and exact short name
second. A query that matches multiple definitions, including overloads, fails with a candidate list
instead of choosing arbitrarily. Exact displayed signatures and definition files can disambiguate
queries. Sherpa does not currently normalize equivalent C++ type spellings, omit parameter names,
or resolve an overload from argument types at a call site.

Impact analysis reflects only relationships present in the current index. It cannot account for
runtime dispatch, build-system configuration, macros, or unresolved external code. Resolved
low-confidence edges are traversed but retain their confidence label. Ambiguous edges are reported
as possible impact and are never traversed transitively.

`path` currently traces directed symbol-to-symbol call relationships only. It does not find paths
between files, mix include and call edges, infer paths through unresolved calls, or enumerate every
possible route. It returns one deterministic shortest path, with fully resolved paths preferred
over paths containing ambiguity.

`export` currently emits file nodes, definition symbol nodes, and `defines`, `calls`, and
`includes` edges only. Python imports use `includes` edges to preserve the version-1 graph
contract. Export does not expose source text, diagnostics, or declarations without definitions.
Identifiers are stable for unchanged indexed content, but they are not intended to preserve
identity across substantive symbol movement or future schema versions.
