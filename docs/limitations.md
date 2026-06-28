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
