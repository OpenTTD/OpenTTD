---
schema: adr/v1
id: adr-0027
kind: adr
title: CMake is the only build system, and every directory declares its sources with add_files under explicit conditions
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - build
related:
  - file:.ai/repo/workflows/substrate/build-and-packaging/README.md
  - file:cmake/SourceList.cmake
  - file:CMakeLists.txt
  - file:cmake/Options.cmake
provenance:
  origin: extracted
  derived_from:
    - file:cmake/SourceList.cmake
    - file:CMakeLists.txt
    - file:cmake/Options.cmake
    - issue:I0063
---

# 27. CMake is the only build system, and every directory declares its sources with add_files under explicit conditions

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

OpenTTD builds on many compilers and platforms, with optional libraries and several product
variants (client, dedicated server, tools only), and generates code at build time (strings,
settings, script bindings).

## Decision

1. **CMake only** (3.16 or newer; `COMPILING.md`).
2. **Per-directory source lists.** Each `CMakeLists.txt` lists its files with
   `cmake/SourceList.cmake:add_files`, optionally under `CONDITION` expressions over options and
   found libraries.
3. **Generators are build tools.** strgen, settingsgen and the script API exporter run as custom
   commands, so generated tables are never committed.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- A new source file is invisible until it is listed in its directory's `CMakeLists.txt`.
- A feature that needs a library or option is a `CONDITION`, not an `#ifdef` alone.

## For this fork

- The fork's research code is a CMake option (`OPTION_RESEARCH_INSTRUMENTATION`, and the MCP
  server option, in `cmake/Options.cmake`) and its sources are added under that condition
  (ADR 0004): a default build does not compile it (knowledge
  `gated-code-is-invisible-to-a-default-build`).
- Map: [build and packaging](../workflows/substrate/build-and-packaging/README.md).
