# Release process

Sherpa releases are immutable, tag-driven builds. A tag does not publish directly to Homebrew or
PyPI; it first creates and validates one GitHub release. The two external publishers are separate,
manual workflows so a missing credential cannot leave a partially published GitHub release.

## One-time external setup

Before the first public package release:

1. Create the public repository `sre0089/homebrew-tap` with a `Formula/` directory.
2. Create a fine-grained GitHub token with contents write access to that repository.
3. Create a `homebrew` environment in this repository and add the token as
   `HOMEBREW_TAP_TOKEN`.
4. Reserve the `sherpa-code` project on PyPI or configure a pending trusted publisher for:
   - owner: `sre0089`
   - repository: `sherpa`
   - workflow: `publish-pypi.yml`
   - environment: `pypi`
5. Create the matching `pypi` GitHub environment. No long-lived PyPI API token is used.

Package names are external state and must be checked again immediately before first publication.

## Prepare a release

1. Update `project(... VERSION ...)` in `CMakeLists.txt`.
2. Move user-facing entries from `Unreleased` into a matching `CHANGELOG.md` version section.
3. Run the full local build and test suite.
4. Merge to `main` and wait for Ubuntu, macOS, and Windows CI.
5. Create and push an annotated tag such as `v0.1.0`.

The release workflow rejects tags that do not exactly match the CMake version or lack changelog
notes.

## GitHub release artifacts

The tag workflow builds:

- `sherpa-VERSION-linux-x86_64.tar.gz`
- `sherpa-VERSION-macos-arm64.tar.gz`
- `sherpa-VERSION-macos-x86_64.tar.gz`
- `sherpa-VERSION-windows-x86_64.zip`
- one `sherpa_code` platform wheel for each target
- `sherpa.rb`
- `SHA256SUMS`

Each job installs into a clean staging prefix and executes `sherpa --version`. Wheels are installed
into a fresh virtual environment and executed before upload. The final job generates exact
checksums, creates GitHub build-provenance attestations, and publishes the release only after every
platform succeeds.

The Linux archive and wheel currently target glibc 2.35 or newer. macOS wheels target macOS 15 or
newer. Expanding compatibility requires older build images or a deliberate manylinux build.

## Publish package managers

After inspecting the GitHub release:

1. Run `Publish PyPI` with the exact release tag. PyPI trusted publishing uploads all released
   wheels under the `sherpa-code` distribution.
2. Run `Publish Homebrew` with the same tag. It copies the generated formula into
   `sre0089/homebrew-tap` and pushes one formula-update commit.
3. Verify installation in clean environments with `pipx install sherpa-code` and
   `brew install sre0089/tap/sherpa`.

Do not reuse or move release tags. A correction requires a new patch version.
