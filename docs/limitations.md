# Analysis limitations

Sherpa's current C/C++ frontend uses tree-sitter and reports syntactic evidence. It does not run a
preprocessor or compiler.

The public C++ API is source-versioned but does not provide binary compatibility. Sherpa currently
installs a static library, exposes C++ standard-library types, and requires consumers to rebuild
against each release. The plugin API supports explicitly linked in-process observers only. There is
no stable C interface, plugin ABI, shared-library discovery, process isolation, parser extension,
or request/result mutation.

Current results therefore do not resolve:

- overloaded functions or methods;
- templates and instantiated symbols;
- macro-expanded declarations;
- compiler conditionals and build flags;
- types, linkage, or virtual dispatch;
- compiler-accurate function calls or include search paths.

Malformed files are indexed when possible. Parser errors are retained as diagnostics alongside any
symbols that could still be extracted.

Incremental indexing still reads every supported file to calculate its fingerprint, rebuilds all
relationships, and writes a complete replacement snapshot. It skips tree-sitter parsing for
unchanged files but does not yet use filesystem metadata shortcuts or partial graph invalidation.

Call resolution currently prefers exact lexical or qualified names, then unique repository-wide
names. Member calls without type information are low confidence. Overloads remain ambiguous when
syntax alone cannot select one definition. Quoted includes use relative, repository-root, or unique
suffix matching; system includes remain unresolved.

`callers` and `callees` select definitions by exact qualified name first and exact short name
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
`includes` edges only. It does not expose source text, diagnostics, declarations without
definitions, or non-C/C++ language constructs. Export identifiers are stable for unchanged indexed
content, but they are not intended to preserve identity across substantive symbol movement or future
schema versions.
