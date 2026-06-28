# ADR 0002: Use pinned tree-sitter frontends for syntax extraction

## Status

Accepted

## Decision

Sherpa uses the tree-sitter C API with pinned C and C++ grammars. Grammar packages are maintained as
small vcpkg overlay ports that verify upstream archives and export stable CMake targets.

## Consequences

Builds remain reproducible without committing generated parser sources into Sherpa. Extraction is
fast and resilient to malformed files, but results are syntactic rather than compiler-semantic.
Every later relationship resolver must preserve that limitation and its evidence.
