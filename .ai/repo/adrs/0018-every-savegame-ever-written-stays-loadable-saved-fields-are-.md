---
schema: adr/v1
id: adr-0018
kind: adr
title: "Every savegame ever written stays loadable: saved fields are gated by SaveLoadVersion and old layouts are converted after load"
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - saveload
  - compatibility
related:
  - file:.ai/repo/workflows/substrate/saveload/README.md
  - file:.ai/repo/workflows/substrate/settings/README.md
  - file:.ai/repo/workflows/substrate/newgrf/README.md
  - rule:project.savegame-fields-are-version-gated
  - file:src/saveload/saveload.h
  - file:src/saveload/afterload.cpp
  - file:docs/savegame_format.md
provenance:
  origin: extracted
  derived_from:
    - file:src/saveload/saveload.h
    - file:src/saveload/afterload.cpp
    - file:docs/savegame_format.md
    - issue:I0066
---

# 18. Every savegame ever written stays loadable: saved fields are gated by SaveLoadVersion and old layouts are converted after load

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Players keep games for decades and scenarios are shared; a savegame is also the join protocol of
a network game (ADR 0016). The format changes with almost every feature.

## Decision

1. **One monotonic version.** `src/saveload/saveload.h:SaveLoadVersion` names every format
   revision; a new revision is appended, never inserted or renumbered.
2. **Field tables carry version ranges.** Each chunk handler describes its fields with `SaveLoad`
   entries whose from/to versions say when a field exists; a removed field keeps a
   load-and-discard entry; `SaveLoadCompat` tables describe old layouts of table-based chunks.
3. **Conversion after load.** Semantic changes are applied once, after all chunks are read, in
   `src/saveload/afterload.cpp:AfterLoadGame`, guarded by `IsSavegameVersionBefore`.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- A saved field is never removed, reordered or renumbered
  (`project.savegame-fields-are-version-gated`); adding state that must survive a save means a
  new `SaveLoadVersion` and a version-gated entry.
- `AfterLoadGame` grows forever and its order matters: a conversion may rely on an earlier one.
- A setting the savegame does not carry is taken from the loading machine's config (candidate
  `a-loaded-game-takes-missing-settings-from-the-loaders-config`).
- A table-based chunk with a renamed key loses its value silently unless a compat entry maps it
  (candidate `a-renamed-table-key-loses-its-value-silently`).

## For this fork

- Research instrumentation is not saved: gated code (ADR 0004) adds no chunk and no field, so a
  research build's savegames load in a default build. Anything an experiment needs across a
  reload belongs in the research manifest, not the savegame.
- Map: [saveload](../workflows/substrate/saveload/README.md).
