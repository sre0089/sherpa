# Sherpa

Sherpa is a local-first codebase intelligence tool. It will turn source code into an
evidence-backed graph of files, symbols, and relationships.

The project is under active development. Milestone 1 indexes C and C++ files into a local SQLite
database; symbol extraction and graph queries follow in later milestones.

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
files. It does not yet parse or resolve symbols.

## License

Sherpa is available under the [MIT License](LICENSE).
