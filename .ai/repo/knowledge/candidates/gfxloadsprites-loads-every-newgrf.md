---
schema: knowledge/v1
id: gfxloadsprites-loads-every-newgrf
kind: knowledge
class: fact
title: "GfxLoadSprites is the NewGRF load path, not only graphics: it runs LoadNewGRF on every peer, dedicated servers included"
description: "When moving, skipping or guarding a GfxLoadSprites call (headless runs, null blitter, research hooks), or when looking for where NewGRF game data is (re)built."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [graphics-and-sprites, newgrf]
about: src/gfxinit.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/gfxinit.cpp
    - file:src/genworld.cpp
    - file:src/saveload/afterload.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/graphics-and-sprites/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/newgrf/README.md
---

# Statement

`src/gfxinit.cpp:GfxLoadSprites` resets the sprite cache and calls `LoadSpriteTables`, which loads the
base set GRFs and then `LoadNewGRF(SPR_NEWGRFS_BASE, 2)`: every enabled NewGRF, with the engines,
industries and other features it defines. Nothing in that path checks the blitter, so a dedicated
server or a `-v null` run executes it too. It is called from world generation
(`src/genworld.cpp`), `src/saveload/afterload.cpp:AfterLoadGame` and
`src/saveload/afterload.cpp:ReloadNewGRFData`. Skipping it to save time in a headless run
changes the game's content, not just its pictures.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote gfxloadsprites-loads-every-newgrf` with evidence, or `majordomus knowledge reject gfxloadsprites-loads-every-newgrf --reason ...`.
