# Sherpa for VS Code

This directory contains the initial Sherpa VS Code integration. It starts `sherpa-server` for the
first workspace folder and provides commands to index the workspace, inspect server status, and
query a symbol.

For development, install Sherpa so `sherpa-server` is on `PATH`, or set
`sherpa.serverPath` to an explicit executable. Open this directory in VS Code and launch an
Extension Development Host.

Run the dependency-free protocol client tests with:

```sh
npm test
```

The extension is not yet packaged for the marketplace and does not provide LSP navigation,
unsaved-buffer overlays, or editor diagnostics.
