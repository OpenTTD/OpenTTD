---
schema: knowledge/v1
id: a-missing-type-label-loads-as-the-base-type
kind: knowledge
class: fact
title: "A savegame whose rail or road type label no loaded NewGRF provides loads that track as plain rail, road or tram, without an error"
description: "When loading a savegame with a different NewGRF set, or reasoning about rail/road types read from a loaded map."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [rail, road, newgrf, saveload]
about: src/newgrf_railtype.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/newgrf_railtype.cpp
    - file:src/newgrf_roadtype.cpp
    - file:src/saveload/labelmaps_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/rail-road-and-infrastructure/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/newgrf/README.md
---

# Statement

Tiles store a rail/road type index; the `RAIL` and `ROTT` chunks
(`src/saveload/labelmaps_sl.cpp:RAILChunkHandler`) save the label of every index.
`src/saveload/labelmaps_sl.cpp:AfterLoadLabelMaps` calls `src/newgrf_railtype.cpp:ConvertRailTypes`
and `src/newgrf_roadtype.cpp:ConvertRoadTypes`, which look each saved label up and, when no loaded
type has it (or a road label now names a tram type), map it to `RAILTYPE_RAIL`, `ROADTYPE_ROAD` or
`ROADTYPE_TRAM`, then rewrite every railway, crossing and road tile. No message is raised there, so a
loaded network can silently change gauge, power and speed limits.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote a-missing-type-label-loads-as-the-base-type` with evidence, or `majordomus knowledge reject a-missing-type-label-loads-as-the-base-type --reason ...`.
