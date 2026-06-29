# Analysis limitations

Sherpa's current C/C++ frontend uses tree-sitter and reports syntactic evidence. It does not run a
preprocessor or compiler.

Current results therefore do not resolve:

- overloaded functions or methods;
- templates and instantiated symbols;
- macro-expanded declarations;
- compiler conditionals and build flags;
- types, linkage, or virtual dispatch;
- compiler-accurate function calls or include search paths.

Malformed files are indexed when possible. Parser errors are retained as diagnostics alongside any
symbols that could still be extracted.

Call resolution currently prefers exact lexical or qualified names, then unique repository-wide
names. Member calls without type information are low confidence. Overloads remain ambiguous when
syntax alone cannot select one definition. Quoted includes use relative, repository-root, or unique
suffix matching; system includes remain unresolved.

`callers` and `callees` select definitions by exact qualified name first and exact short name
second. A query that matches multiple definitions, including overloads, fails with a candidate list
instead of choosing arbitrarily. Signature- and source-location-based disambiguation is not yet
available.

Impact analysis reflects only relationships present in the current index. It cannot account for
runtime dispatch, build-system configuration, macros, or unresolved external code. Resolved
low-confidence edges are traversed but retain their confidence label. Ambiguous edges are reported
as possible impact and are never traversed transitively.

`path` currently traces directed symbol-to-symbol call relationships only. It does not find paths
between files, mix include and call edges, infer paths through unresolved calls, or enumerate every
possible route. It returns one deterministic shortest path, with fully resolved paths preferred
over paths containing ambiguity.
