# Installation

Sherpa publishes native archives for Linux x86-64, macOS arm64 and x86-64, and Windows x86-64.
Each archive contains `sherpa`, `sherpa-server`, the static C++ library, public headers, CMake
package metadata, license, README, and changelog.

## Homebrew

After the Homebrew tap has been created and the first release published:

```sh
brew install sre0089/tap/sherpa
sherpa --version
```

The formula installs the two native executables from GitHub release archives. It does not build
Sherpa or vcpkg dependencies on the user's machine.

## pipx or pip

After the `sherpa-code` PyPI project has been configured and the first wheels published:

```sh
pipx install sherpa-code
sherpa --version
```

`python -m pip install sherpa-code` also works, but `pipx` keeps command-line applications isolated.
The wheel bundles the native `sherpa` and `sherpa-server` executables and adds small Python
launchers. It does not expose a Python API. The package requires Python 3.9 or newer to run its
launchers.

The distribution name cannot be `sherpa`: that name is owned by an unrelated astronomy package on
PyPI.

## GitHub archives

Download the archive matching the operating system and architecture from the GitHub release.
Extract it and place the executables from `bin/` on `PATH`.

Every release contains `SHA256SUMS`. Verify a downloaded artifact before using it:

```sh
sha256sum --check SHA256SUMS
```

GitHub also records build provenance for the release artifacts. With the GitHub CLI:

```sh
gh attestation verify sherpa-0.1.0-linux-x86_64.tar.gz --repo sre0089/sherpa
```

The provenance proves that GitHub Actions built the artifact from this repository and tag. Sherpa
does not yet apply Apple Developer ID or Windows Authenticode signatures.

## Build from source

Building requires a C++20 compiler, CMake 3.25 or newer, Ninja, and vcpkg:

```sh
cmake --preset release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset release
cmake --install build/release --prefix /desired/prefix
```
