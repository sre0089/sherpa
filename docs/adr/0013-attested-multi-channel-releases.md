# ADR 0013: Use attested GitHub releases as the package source of truth

## Status

Accepted

## Decision

Sherpa creates releases only from semantic-version tags that exactly match the CMake project
version and a non-empty changelog section. GitHub Actions builds native archives and platform
wheels on the target operating systems, validates the installed commands, publishes SHA-256
checksums, and records GitHub build-provenance attestations.

GitHub releases are the source of truth for downstream distribution. The Homebrew tap consumes the
released native archives. PyPI publishes platform wheels under the distribution name
`sherpa-code`; each wheel contains the same native commands and thin launchers, not a Python API.
Homebrew and PyPI publishing remain separate manual workflows after release inspection.

## Consequences

Cross-platform artifacts are produced consistently without introducing a stable binary ABI
promise. A failed platform prevents publication. Package-manager failures cannot create a partial
GitHub release, and external credentials are isolated in protected environments. The project must
maintain a separate Homebrew tap and PyPI trusted-publisher registration. GitHub provenance does
not replace future Apple Developer ID or Windows Authenticode signing.
