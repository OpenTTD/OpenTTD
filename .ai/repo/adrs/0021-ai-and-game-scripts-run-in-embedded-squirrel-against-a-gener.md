---
schema: adr/v1
id: adr-0021
kind: adr
title: AI and Game Scripts run in embedded Squirrel against a generated API, and old API versions are kept through compatibility scripts
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - scripting
  - compatibility
related:
  - file:.ai/repo/workflows/substrate/scripting/README.md
  - file:.ai/repo/workflows/substrate/commands/README.md
  - file:src/ai/ai_info.cpp
  - file:bin/ai/compat_0.7.nut
  - file:cmake/scripts/SquirrelExport.cmake
provenance:
  origin: extracted
  derived_from:
    - file:src/ai/ai_info.cpp
    - file:bin/ai/compat_0.7.nut
    - file:cmake/scripts/SquirrelExport.cmake
    - file:src/script/squirrel.cpp
    - issue:I0063
---

# 21. AI and Game Scripts run in embedded Squirrel against a generated API, and old API versions are kept through compatibility scripts

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Third parties write company AIs and Game Scripts that outlive many OpenTTD releases, and scripts
must not be able to break determinism or crash the game.

## Decision

1. **Embedded VM.** Scripts run in a vendored Squirrel VM (`src/3rdparty/squirrel/`, wrapped by
   `src/script/squirrel.cpp`), with an operation budget per tick and suspension when a command is
   issued.
2. **Generated bindings.** The script API classes in `src/script/api/` are exported to Squirrel by
   generated glue (`cmake/scripts/SquirrelExport.cmake`), so the documented API and the binding
   cannot drift.
3. **API versions.** A script declares `GetAPIVersion`; `src/ai/ai_info.cpp` accepts only
   versions in `AIInfo::ApiVersions`, and `bin/ai/compat_<version>.nut` (and `bin/game/`) restore
   older behaviour on top of the current API.
4. **Same commands as players.** A script mutates the game only by issuing commands (ADR 0016).

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- Changing an API function's behaviour needs a compat shim for older API versions; widget enums
  exposed through the API become part of it (candidate `widget-enums-are-part-of-the-script-api`).
- Scripts run only on the server; the Game Script also runs while paused (candidate
  `scripts-run-only-on-the-server-and-the-game-script-runs-while-paused`).
- A script command outside a suspendable context is fatal (candidate
  `a-script-command-outside-a-suspendable-context-is-fatal`).

## For this fork

- The Prismatic bridge drives a Company AI over rcon (ADR 0007), inheriting these constraints.
- Map: [scripting](../workflows/substrate/scripting/README.md).
