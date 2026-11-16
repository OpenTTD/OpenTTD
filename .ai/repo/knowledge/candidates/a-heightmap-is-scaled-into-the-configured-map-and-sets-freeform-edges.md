---
schema: knowledge/v1
id: a-heightmap-is-scaled-into-the-configured-map-and-sets-freeform-edges
kind: knowledge
class: fact
title: "A heightmap is scaled and padded into the map size from the settings, and its borders decide freeform_edges"
description: "When starting a game from a heightmap headlessly (-g <file>) and expecting the map size or edge behaviour to follow the image."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [heightmap, world-generation, cli]
about: src/heightmap.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/heightmap.cpp
    - file:src/openttd.cpp
    - file:src/genworld_gui.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/world-generation/README.md
---

# Statement

`src/openttd.cpp:MakeNewGame` generates a heightmap game at `1 << map_x` by `1 << map_y` from the
game settings; only the GUI path reads the image size (`src/heightmap.cpp:GetHeightmapDimensions`
in `src/genworld_gui.cpp`) to offer it. `GreyscaleToMapHeights` in `src/heightmap.cpp` then scales
the image to fit (honouring `heightmap_rotation`) and centres it with padding.
`src/heightmap.cpp:LoadHeightmap` finally sets `construction.freeform_edges` to
`!IsMapSurroundedByWater()`, overriding the configured value. A headless `-g <file>` run therefore
needs `map_x`/`map_y` (and `heightmap_height`) in its config to get the intended map.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote a-heightmap-is-scaled-into-the-configured-map-and-sets-freeform-edges` with evidence, or `majordomus knowledge reject a-heightmap-is-scaled-into-the-configured-map-and-sets-freeform-edges --reason ...`.
