# Analysis limitations

Sherpa's current C/C++ frontend uses tree-sitter and reports syntactic evidence. It does not run a
preprocessor or compiler.

Current results therefore do not resolve:

- overloaded functions or methods;
- templates and instantiated symbols;
- macro-expanded declarations;
- compiler conditionals and build flags;
- types, linkage, or virtual dispatch;
- function calls or other graph relationships.

Malformed files are indexed when possible. Parser errors are retained as diagnostics alongside any
symbols that could still be extracted.
