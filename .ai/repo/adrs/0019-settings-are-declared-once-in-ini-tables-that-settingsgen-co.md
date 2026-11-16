---
schema: adr/v1
id: adr-0019
kind: adr
title: Settings are declared once in ini tables that settingsgen compiles into the tables used by the config file, the savegame, the GUI and network sync
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - settings
  - build
related:
  - file:.ai/repo/workflows/substrate/settings/README.md
  - file:.ai/repo/workflows/substrate/build-and-packaging/README.md
  - file:.ai/repo/workflows/substrate/saveload/README.md
  - file:.ai/repo/workflows/substrate/network/README.md
  - file:src/settingsgen/settingsgen.cpp
  - file:src/table/settings/CMakeLists.txt
provenance:
  origin: extracted
  derived_from:
    - file:src/settingsgen/settingsgen.cpp
    - file:src/table/settings/CMakeLists.txt
    - issue:I0063
---

# 19. Settings are declared once in ini tables that settingsgen compiles into the tables used by the config file, the savegame, the GUI and network sync

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

A setting is read from `openttd.cfg`, shown in the settings window, stored in savegames when it
belongs to the game, and kept equal on every peer when it influences the simulation. Writing
those four places by hand for each of hundreds of settings would let them drift.

## Decision

1. **Declarative source.** Each setting is an entry in an ini file under `src/table/settings/`
   with its type, flags, default, range, callbacks and the savegame versions it exists in.
2. **Generated tables.** `src/settingsgen/settingsgen.cpp` is a build-time tool (its own ini
   loader over `src/ini_type.h`) that turns those files into C++ tables; the custom command in
   `src/table/settings/CMakeLists.txt` regenerates them when an ini file changes.
3. **One table, several consumers.** The generated tables drive config load/save, the settings
   GUI, savegame chunk handling and network synchronisation of game settings.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- A new setting is an ini entry, not code in several places; its savegame presence is a version
  range like any saved field (ADR 0018).
- A game setting changed at runtime must go through the setting-change command so every peer
  applies it (ADR 0016); writing the global directly desyncs.
- Changing a setting from the console in a research run rewrites the user's `openttd.cfg` unless
  the run used `-x` (candidate `a-console-setting-change-rewrites-the-users-config`).

## For this fork

- Research runs record the settings that matter in `manifest.json` (ADR 0005) rather than trusting
  the machine's config.
- Map: [settings](../workflows/substrate/settings/README.md).
