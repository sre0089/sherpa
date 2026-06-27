# Contributing

Sherpa uses short-lived branches and pull requests into `main`.

Before submitting a change:

```sh
cmake --build --preset dev
ctest --preset dev
```

Behavior changes require tests. Architecture, database compatibility, and public-format changes
require an architecture decision record under `docs/adr/`.
