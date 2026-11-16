---
schema: adr/v1
id: adr-0023
kind: adr
title: The map is a flat array of tiles whose behaviour is dispatched by tile type through a table of tile procedures
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - map
  - tiles
related:
  - file:.ai/repo/workflows/substrate/map-and-tiles/README.md
  - file:.ai/repo/workflows/substrate/rail-road-and-infrastructure/README.md
  - file:.ai/repo/workflows/substrate/towns-industries-stations/README.md
  - file:src/tile_cmd.h
  - file:src/landscape.cpp
provenance:
  origin: extracted
  derived_from:
    - file:src/tile_cmd.h
    - file:src/landscape.cpp
    - file:src/map_func.h
    - issue:I0063
---

# 23. The map is a flat array of tiles whose behaviour is dispatched by tile type through a table of tile procedures

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Every tile is one of a small set of kinds (clear, rail, road, house, trees, station, water, void,
industry, tunnel/bridge, object), and generic code — drawing, clearing, terraforming, vehicles
entering, periodic tile loops — must act on any of them.

## Decision

1. **Flat storage.** The map is a fixed-size array of tiles with packed per-type bit fields,
   accessed through the `*_map.h` accessors.
2. **Per-type dispatch.** `src/tile_cmd.h:TileTypeProcs` is a table of function pointers (draw,
   slope, clear, accepted cargo, tile description, product, animation, tile loop, vehicle enter,
   foundation, terraform, ...); `src/landscape.cpp:_tile_type_procs` holds one entry per
   `TileType`, and generic code calls through it.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- A new tile behaviour is a proc in the owning `*_cmd.cpp`, never a `switch` over tile types in
  generic code.
- The tile loop runs over the whole map every 256 ticks in a deterministic order; per-tile work
  there is a performance and determinism hot spot.
- Bit layouts are part of the savegame (ADR 0018) and documented in `docs/landscape.html`.

## For this fork

- The PrismaticMCP map encoding follows the `TileType` order (candidate
  `the-mcp-map-encoding-follows-tiletype-order`): a new tile type upstream changes it.
- Maps: [map and tiles](../workflows/substrate/map-and-tiles/README.md),
  [infrastructure](../workflows/substrate/rail-road-and-infrastructure/README.md).
