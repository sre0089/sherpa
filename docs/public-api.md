# Public C++ API

Sherpa provides a C++20 library for embedding indexing and graph queries without invoking the CLI
or reading SQLite directly. The supported entry point is:

```cpp
#include <sherpa/api/client.hpp>
```

`sherpa::api::Client` supports indexing, symbol and file queries, callers and callees, impact
analysis, directed paths, and deterministic graph construction. Requests select a repository and
may override its database path:

```cpp
sherpa::api::Client client;
const auto result = client.symbol({
    .repository = {.path = "/path/to/repository"},
    .symbol = {.name = "namespace::function"},
});
```

Failures are reported as `sherpa::api::Error`. Callers should dispatch on `Error::code()` rather
than matching message text. Ambiguous symbol errors retain ordered candidates.

Embedding applications can construct a client with explicitly registered in-process plugins. The
plugin API is separately versioned and observational; see the [plugin API](plugin-api.md).

## CMake consumption

After installing Sherpa:

```cmake
find_package(Sherpa CONFIG REQUIRED)
target_link_libraries(my_tool PRIVATE Sherpa::Sherpa)
```

The package resolves Sherpa's implementation dependencies. Consumers do not include SQLite,
tree-sitter, CLI, application-service, storage, parser, or presentation headers.

## Compatibility

`sherpa::api::kApiVersion` identifies the public C++ contract and is currently `1`. The supported
surface consists of `sherpa/api/client.hpp`, `sherpa/api/plugin.hpp`, and the domain value headers
included by the client. Other headers are internal even when they are available in a source
checkout.

Sherpa is pre-1.0. Within one API version, existing names and value-field meanings remain stable;
additive methods, enum values, and fields may be introduced. A source-breaking change requires an
API-version increment, a project minor-version increment, and migration notes. Consumers should
still rebuild against every Sherpa release.

Binary compatibility is not guaranteed. The public types use the C++ standard library, and Sherpa
does not yet define a stable ABI or C interface.
