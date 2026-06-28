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

## Use

```sh
./build/dev/sherpa index /path/to/repository
```

Use `--database <path>` to override the platform cache location.

## Status

Sherpa currently discovers `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, and `.inc`
files. It performs syntax-based analysis only: overload resolution, types,
preprocessor expansion, and compiler-accurate semantics are not implemented. Call and include
resolution is intentionally conservative and every result carries status, confidence, and
provenance.

## License

Sherpa is available under the [MIT License](LICENSE).
