# Plugin API

Sherpa provides a versioned C++ plugin foundation for extensions linked directly into an embedding
application. Plugins are registered explicitly and observe public API operations without receiving
access to SQLite, tree-sitter, application services, or mutable requests and results.

```cpp
#include <memory>
#include <utility>

#include <sherpa/api/client.hpp>

class TelemetryPlugin final : public sherpa::api::Plugin {
 public:
  sherpa::api::PluginDescriptor descriptor() const override {
    return {
        .id = "example.telemetry",
        .name = "Example telemetry",
        .api_version = sherpa::api::kPluginApiVersion,
    };
  }

  void before_operation(const sherpa::api::OperationContext& context) override {
    // Record the start of context.operation.
  }

  void after_operation(const sherpa::api::OperationContext& context) override {
    // Record successful completion.
  }
};

sherpa::api::PluginRegistry plugins;
plugins.add(std::make_shared<TelemetryPlugin>());
sherpa::api::Client client(std::move(plugins));
```

## Contract

`sherpa::api::kPluginApiVersion` is currently `1`. A plugin descriptor must request that exact
version. Plugin IDs are stable identifiers that start with a lowercase ASCII letter or digit and
contain lowercase ASCII letters, digits, `.`, `_`, or `-`; duplicate IDs are rejected.
Registration failures throw `std::invalid_argument`. Descriptors are validated and cached when
registered, and registration order is preserved.

For each valid client request, all `before_operation` hooks run in registration order. The core
operation then runs. After a successful core operation, all `after_operation` hooks run in the same
order. Hook execution is fail-fast: later hooks in that phase are not called after one throws.
Invalid requests and failed core operations do not invoke `after_operation`.

Plugin exceptions become `sherpa::api::ErrorCode::kPluginFailure`. `Error::plugin_id()` identifies
the plugin. `Error::operation_completed()` is false for a failing before hook and true for a
failing after hook. The latter distinction matters for indexing: the atomic index may already have
been published even though a post-operation observer failed.

Hooks on one `Client` may be called concurrently if the host calls that client concurrently.
Plugins are responsible for synchronizing their own state.

The registry and client hold shared ownership of plugin instances. Sherpa provides no separate load
or unload callbacks; ordinary C++ construction and destruction define instance lifetime.

## Current boundary

Version 1 is intentionally observational. Plugins cannot mutate requests, results, graph ordering,
or persistence. This preserves Sherpa's atomicity and deterministic-output contracts.

Sherpa does not discover or load shared libraries. Plugins are compiled and linked with their host,
and all components must use compatible C++ toolchains and Sherpa builds. A stable C ABI, dynamic
discovery, sandboxing, CLI command contribution, parser registration, and result transformation are
future decisions rather than implied parts of this contract.
