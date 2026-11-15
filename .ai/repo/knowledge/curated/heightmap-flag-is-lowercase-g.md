---
schema: knowledge/v1
id: heightmap-flag-is-lowercase-g
kind: knowledge
class: fact
title: "-g <file> loads a heightmap, savegame or scenario headlessly; -G is the map-generation random seed"
description: "When seeding a world from real elevation data, or reading a command line that passes -G."
status: verified
epistemics: observed
date: 2026-07-24
tags: [cli, heightmap, world-seeding]
about: src/openttd.cpp
route: documentation
provenance:
  origin: extracted
  derived_from:
    - commit:68d15d4ee31b582821b4ab9abc927ea1c1da0fbb
    - file:src/openttd.cpp
relations:
  - type: documents
    target: file:research/prismatic-bridge/ARCHITECTURE.md
---

# Fact

`src/openttd.cpp`'s usage text says `-G seed = Set random seed`. `-g <file>` auto-detects
savegame, scenario or heightmap and starts a new game without a GUI (`SwitchMode::StartHeightmap`
leads to `MakeNewGame`). An earlier revision of the bridge design used `-G` for the heightmap
path, and the bridge repository's own tooling prompted the correction.
