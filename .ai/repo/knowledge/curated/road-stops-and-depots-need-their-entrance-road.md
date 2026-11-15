---
schema: knowledge/v1
id: road-stops-and-depots-need-their-entrance-road
kind: knowledge
class: lesson
title: "A road depot or bay stop faces its front tile but is not connected to it; drive-through stops run along the road's axis"
description: "When a bot, a Rhai script or an MCP caller builds a road station, a road depot, a rail depot or a train station."
status: verified
epistemics: observed
date: 2026-11-15
tags: [script-api, road, rail, depot, station, orientation, bot]
about: src/script/api/script_road.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0043
    - file:src/script/api/script_road.cpp
    - file:src/script/api/script_road.hpp
---

# Lesson

`ScriptRoad::BuildRoadDepot(tile, front)` and `BuildRoadStation(tile, front, ...)`
(`src/script/api/script_road.cpp`, about lines 546-575) only derive the entrance direction from
`tile` towards `front`: `DiagdirBetweenTiles(tile, front)` for stations, the explicit NE/SE/SW/NW
choice for depots. Neither builds the half road piece on `front` that leads in. A depot or bay stop
built beside a road therefore faces it without being connected, and vehicles cannot reach it. Both
also require `front` to be in a straight line with `tile` (same X or same Y), and flat land.

`BuildDriveThroughRoadStation(tile, front, ...)` takes its axis from the same pair: on an existing
road tile, `front` must lie along the road (the tile must be a straight piece, not a junction), or
the call fails with `ERR_ROAD_DRIVE_THROUGH_WRONG_DIRECTION`.

The same shape holds for rail: a rail depot faces its front tile and needs track on the front tile
that points into it, and a train station's axis (`RAILTRACK_NE_SW` or `RAILTRACK_NW_SE`) has to line
up with the track that reaches it.

# How to apply

- Prefer a drive-through stop on a flat, straight piece of existing road. Find the axis from
  `AreRoadTilesConnected(t, t±1)` and `(t, t±width)`, with exactly one pair connected and the other
  not, and pass the neighbour along that axis as `front`.
- After a bay stop or depot, call `BuildRoad(front, tile)` and check
  `AreRoadTilesConnected(front, tile)`. If they are not connected, demolish the piece and try the
  next site. Never return an unconnected site.
- PrismaticAI does this in `ai/PrismaticAI/roadstop.nut` (bridge repository):
  `StraightRoadFront`, `BuildBusStopNear`, `BuildDepotNear`. Observed live on 2026-11-15: every stop
  built as drive-through, depots connected with buses leaving them, and a 26-tile intercity road
  served by 4 buses.
