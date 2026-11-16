---
schema: knowledge/v1
id: the-mcp-map-encoding-follows-tiletype-order
kind: knowledge
class: constraint
title: "The fork's openttd://map tile codes index a fixed string by TileType's numeric value; an upstream reorder mislabels tiles silently"
description: "When rebasing onto an upstream change to src/tile_type.h."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [map-and-tiles, prismatic-mcp]
about: src/prismatic_mcp/prismatic_mcp_world.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_world.cpp
    - file:src/tile_type.h
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/map-and-tiles/README.md
---

# Statement

`src/prismatic_mcp/prismatic_mcp_world.cpp:TypeCode` maps a tile type to `TYPE_CODES[i]` (one letter
per value, `'?'` past the end). An appended type shows as `'?'`; a reordered one is mislabelled with no
error, and a default build does not compile this file. Re-read `src/tile_type.h:TileType` on every rebase
that touches it.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote the-mcp-map-encoding-follows-tiletype-order` with evidence, or `majordomus knowledge reject the-mcp-map-encoding-follows-tiletype-order --reason ...`.
