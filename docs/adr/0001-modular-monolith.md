# ADR 0001: Use a modular monolith

## Status

Accepted

## Decision

Sherpa begins as one C++ executable with explicit application, scanner, storage, parsing, and
analysis boundaries.

## Consequences

The project remains simple to build and distribute while preserving boundaries needed by later
language frontends and API adapters. No stable in-process plugin ABI is created.
