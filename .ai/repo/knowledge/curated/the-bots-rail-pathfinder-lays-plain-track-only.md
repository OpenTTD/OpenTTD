---
schema: knowledge/v1
id: the-bots-rail-pathfinder-lays-plain-track-only
kind: knowledge
class: constraint
title: "PrismaticAI lays rail over land and level crossings only: a line that needs a bridge or tunnel fails"
description: "When choosing which planned rail line the owner's bot builds, or reading why a bot company has no trains."
status: verified
epistemics: observed
date: 2026-11-16
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
same seed, a station between houses had its exit facing a road junction. Bridges and tunnels are
still out of reach. The passenger line Ganingbury–Sontburg on that seed needs a bridge over the
river south of the town.

# How to apply

- Price rail lines with track-planning (`mcp-plan --track N`) before the bot gets one. The panel's
  bot start prices six of each list, passes over a line whose counts include a bridge or tunnel,
  and does not hand over an unpriced line while others were priced.
- A bot company with T0 in `companies` after a rail plan: look for "no rail path" in its log
  before blaming the planner's scores. To find which rule blocks the line, replay the search
  offline on `openttd://map` and `openttd://map/terrain`, with the bot's own stations put back.
- Lines that need bridges stay out of reach until the bot learns to place them, or until an
  executor (openttd-07's `mcp-human rail`) lays track-planning's pieces for the bot's company.
