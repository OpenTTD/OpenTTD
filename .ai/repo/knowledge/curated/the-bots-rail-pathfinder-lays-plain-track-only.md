---
schema: knowledge/v1
id: the-bots-rail-pathfinder-lays-plain-track-only
kind: knowledge
class: constraint
title: "PrismaticAI's rail pathfinder: land, level crossings, bridges and tunnels, each taken only as the game allows it"
description: "When choosing which planned rail line the owner's bot builds, or reading why a bot company has no trains."
status: verified
epistemics: observed
date: 2026-11-17
tags: [bot, rail, pathfinding, planner, track-planning]
about: tools/research/mcp-panel/server.py
route: platform
provenance:
  origin: authored
  derived_from:
    - file:research/results/duel-20261116T074419-seed20208.md
    - file:tools/research/mcp-panel/server.py
    - file:tools/research/mcp-rhai/src/linecost.rs
    - file:src/rail_cmd.cpp
    - file:research/results/duel-20261116T082722-seed20208.md
    - file:research/results/duel-20261117T012840-seed31337.md
    - file:src/vehicle_base.h
---

# Constraint

The bridge repository's `ai/PrismaticAI/rail.nut` `FindRailPath` is an A* search over buildable
tiles. A water, road or rail tile ends a branch, and the search never places a bridge, a tunnel or
a level crossing. A planned line whose only routes need one of these fails: the bot builds both
stations, logs "no rail path between the stations; removing them", and falls back to buses.

In the 36-month duel on seed 20208 (2026-11-16) this left the planned company without a train.
AlgoGlyph track-planning had found routes for the same line, but they used bridges and a crossing.

Turns had a second limit, now fixed. A turn was allowed on flat land only, so a station whose exit
and neighbours were all sloped was unreachable even over plain land: a 180-tile coal line on the
same seed failed that way twice. Since bridge repository 2e3f116, `PieceFits` takes the game's own
rule (`GetRailFoundation` with build_on_slopes, `rail_cmd.cpp`): any piece on flat land, a straight
piece on any slope that is not steep, and a corner piece only where
`_valid_tracks_without_foundation` or `_valid_tracks_on_leveled_foundation` allow it. The pieces
on both exits are checked against their stations. Just allowing every slope that is not steep was
wrong: the game refused the first piece with ERR_LAND_SLOPED_WRONG.

Since bridge repository a0f2734 the bot crosses a straight road at right angles on flat land (a
level crossing, never turning on it). It takes a station site only when the exit leads on: on the
same seed, a station between houses had its exit facing a road junction. Since bridge repository d3c39ba the bot also bridges what blocks a straight run two tiles ahead
(water, a road it cannot cross, rail, a valley). The far head is the first land 3 to 11 tiles on
whose bridge the game accepts in test mode (`AITestMode` plus `AIBridge.BuildBridge`). On seed
20208 the passenger line Mefingpool–Sontburg was then built with two bridges. Since d3c39ba's
successor 34bd520 it may also tunnel where the next tile rises in the line's direction (the far
portal from `AITunnel.GetOtherTunnelEnd`, at most 15 tiles, accepted in test mode). No map so far
has needed one, so a built tunnel has not been seen.

Since bridge repository b4e20b5 the search takes **diagonal track**. Costs are in thirds of a straight
piece, the game's own travel measure (`vehicle_base.h`: a straight piece is 16 steps of
TILE_AXIAL_DISTANCE 192, a corner piece 8 steps of 2 × TILE_CORNER_DISTANCE). A corner piece that
continues a diagonal (alternating corners) costs 2 against 3, a turn is paid once where the diagonal
starts, and a U within two tiles is not taken. The estimate must stay above the diagonal cost (7 per
tile left). At 6 every staircase toward the goal had the same f, the search spread over the whole
rectangle between the stations, and lines that the old search found failed. On seed 31337, lines were
48–76% corner pieces, and company value went from 250716 to 454403 on the same plan
(duel-20261117T012840). A failed search logs whether the node budget ran out or the reachable tiles
did. "every reachable tile tried, 1 nodes" means the station exit has no usable neighbour. A failed
piece (ERR_AREA_NOT_CLEAR) still drops the whole line; re-planning from that piece is the next fix.

# How to apply

- Price rail lines with track-planning (`mcp-plan --track N`) before the bot gets one. The panel's
  bot start prices six of each list, hands over any line with a route, and does not hand over an
  unpriced line while others were priced.
- A bot company with T0 in `companies` after a rail plan: look for "no rail path" in its log
  before blaming the planner's scores. To find which rule blocks the line, replay the search
  offline on `openttd://map` and `openttd://map/terrain`, with the bot's own stations put back.
- The bot's own search may still miss a route that track-planning finds: the two searches differ.
  Replay the bot's search offline (above) to see which rule blocks it.
