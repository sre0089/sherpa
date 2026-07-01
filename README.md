# Sherpa

Sherpa is a local-first codebase intelligence tool. It will turn source code into an
evidence-backed graph of files, symbols, and relationships.

The project is under active development. Sherpa indexes C and C++ files and uses tree-sitter to
extract functions, methods, classes, structs, include directives, source locations, and parser
diagnostics into a local SQLite database. It also records evidence-backed `defines`, `calls`, and
`includes` relationships while preserving ambiguous and unresolved references.

## Build

Prerequisites: a C++20 compiler, CMake 3.25+, Ninja, and vcpkg.

```sh
cmake --preset dev -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset dev
ctest --preset dev
```

Sherpa also installs a C++20 library and CMake package. Embedded consumers use
`<sherpa/api/client.hpp>` and link `Sherpa::Sherpa`; see the
[public C++ API](docs/public-api.md). Hosts can also register explicitly linked observers through
the versioned [plugin API](docs/plugin-api.md).

## Use

```sh
./build/dev/sherpa index /path/to/repository
cd /path/to/repository
sherpa query symbol qualified::symbol
sherpa query symbol overloaded --signature 'overloaded(int value)'
sherpa query file src/file.cpp
sherpa query callers qualified::symbol
sherpa query callees qualified::symbol
sherpa callers qualified::symbol
sherpa callees qualified::symbol
sherpa impact src/file.cpp
sherpa impact qualified::symbol
sherpa path caller::symbol callee::symbol
sherpa export graph.json
```

Queries use the current directory as the repository by default. `sherpa query ...` is the grouped
read-only query surface; the top-level `callers` and `callees` commands remain as compatibility
aliases. Use `--repo <path>` to query another repository, `--database <path>` to override the
platform cache location, and `--format json` for machine-readable output. See
[Indexing](docs/indexing.md) for incremental refresh behavior,
[Querying the graph](docs/querying.md) for lookup rules, and the
[Query JSON contract](docs/query-json.md) for automation. See
[Graph export](docs/graph-export.md) for the versioned interchange format.

## Status

Sherpa currently discovers `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, and `.inc`
files. It performs syntax-based analysis only: overload resolution, types,
preprocessor expansion, and compiler-accurate semantics are not implemented. Call and include
resolution is intentionally conservative and every result carries status, confidence, and
provenance.

## License

Sherpa is available under the [MIT License](LICENSE).
