---
schema: adr/v1
id: adr-0028
kind: adr
title: Graphics, sounds and music come from replaceable base sets and NewGRFs, and the engine ships no proprietary data
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - graphics
  - newgrf
  - content
related:
  - file:.ai/repo/workflows/substrate/graphics-and-sprites/README.md
  - file:.ai/repo/workflows/substrate/sound-and-music/README.md
  - file:.ai/repo/workflows/substrate/newgrf/README.md
  - file:src/base_media_base.h
  - file:docs/directory_structure.md
provenance:
  origin: extracted
  derived_from:
    - file:src/base_media_base.h
    - file:docs/directory_structure.md
    - issue:I0066
---

# 28. Graphics, sounds and music come from replaceable base sets and NewGRFs, and the engine ships no proprietary data

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

OpenTTD began as a re-implementation of a commercial game whose data it may not distribute, and
content creators replace or extend almost every sprite, sound and vehicle.

## Decision

1. **Base sets.** Graphics, sounds and music are base sets described by metadata files and
   discovered on the search paths (`src/base_media_base.h:BaseSet`, `docs/directory_structure.md`);
   open sets (OpenGFX, OpenSFX, OpenMSX) or the original data can be used.
2. **NewGRF on top.** NewGRFs redefine or add sprites, vehicles, industries, houses and more at
   load time (newgrf map); the engine's own tables are defaults.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- The binary cannot run without a base graphics set; a headless run with none hangs on macOS
  (knowledge `macos-missing-baseset-hangs-headless-runs`).
- Sprite numbers above the base range belong to NewGRFs; code must not assume a sprite exists.
- The set of base sets and NewGRFs is part of a game's identity: peers must match (ADR 0016).

## For this fork

- Research manifests record the base sets and NewGRFs used.
- Maps: [graphics](../workflows/substrate/graphics-and-sprites/README.md),
  [NewGRF](../workflows/substrate/newgrf/README.md).
