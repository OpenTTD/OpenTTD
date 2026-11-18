---
schema: knowledge/v1
id: rail-lines-a-train-can-turn-on
kind: knowledge
class: constraint
title: "A rail line holds no 90-degree turn, anywhere: pf.forbid_90_deg is on by default and a train meeting one is lost"
description: "When building, re-planning or checking rail track for a bot, or reading why a bot's trains are lost or a rail line earns nothing."
status: verified
epistemics: observed
date: 2026-11-18
tags: [bot, rail, pathfinding, signals, stations, track-planning]
about: tools/research/prismatic-ai/PrismaticAI/rail.nut
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/table/settings/pathfinding_settings.ini
    - file:src/pathfinder/follow_track.hpp
    - file:src/train_cmd.cpp
    - file:src/pbs.cpp
    - file:src/script/api/script_rail.cpp
    - file:tools/research/prismatic-ai/PrismaticAI/rail.nut
    - file:research/results/duel-20261118T063729-seed4242.md
    - file:research/rail-steward.md
---

# Constraint

`pf.forbid_90_deg` defaults to `true` (`src/table/settings/pathfinding_settings.ini`), and every
research game runs with it. With the setting on, the track follower cuts every trackdir that
crosses the one the train arrives on (`follow_track.hpp` and `train_cmd.cpp`,
`TrackdirCrossesTrackdirs`).

- On the 4-neighbour grid a bot lays track on, that is two corner pieces on adjacent tiles that
  turn the same way: moves `d1, d2, d3` with `d3 == -d1`.
- Alternating corners (`d3 == d1`) are a 45-degree diagonal and allowed.
- A line holding one such turn is two dead ends to a train. The train finds no path, is flagged
  lost (`VehicleFlag::PathfinderLost`), turns at the end of the track and earns nothing.

PrismaticAI's search rejected the turn only mid-line, and three places escaped it:

- the first step beyond a station's exit;
- the step into the far station;
- the first step of a re-plan around a refused piece.

Depots were also joined on corners, where the joining pieces meet the line's corner at 90 degrees.

Since 2026-11-18 (`tools/research/prismatic-ai`), `Turns90` runs at every step, at both ends and
across a re-plan (`from_in`). Depots stand behind a platform's dead end, else beside a straight
piece only. `VerifyRailLine` walks the laid line with `AIRail.AreTilesConnected` before any train
is bought.

**A two-platform throat adds a route the main line does not show.** The second platform joins the
line by a sideways move (M2 to M). A corner on the line's first tile toward that side passes the
check on the main platform's route and is still a 90-degree turn on the second one. Since
2026-11-18 the line runs straight through each throat's first tile, and straight into the far one
(`FindRailPath` `from_straight`, `to_straight`). Codex's independent review of the branch found
this case.

A second constraint drove the terminus shape: **a single track carries one train unless trains
wait off it.**

- Path signals on each platform's exit make a train leave only when it can reserve its way into a
  free platform at the other end. A dead end is a safe waiting position (`pbs.cpp`
  `IsSafeWaitingPosition` with `include_line_end`).
- With two platforms per terminus and a two-tile throat, as many trains as the smaller end has
  platforms share the track with no passing loop.
- `AIRail.BuildSignal(tile, front, …)` signals trains moving from `tile` toward `front`, and a plain
  path signal can be passed from behind. The exit signals therefore stand on the straight tiles
  just beyond the platforms.

# Evidence

The 12-month duel on seed 4242 (`research/results/duel-20261118T063729-seed4242.md`):

- six rail lines, each with two-platform termini, path-signalled throats and a depot behind a
  platform;
- no "no rail path", broken line or refused throat in the log;
- 6 trains at month 12.

This proves the geometry builds and runs. It is not yet an earnings A/B: rule
`project.a-tuning-claim-needs-held-start-pairs`.

# How to apply

- Any rail builder here (the bot, `mcp-human`, an AlgoGlyph track plan handed to one) checks
  `Turns90` on the whole sequence, the station ends included, before laying.
- A lost train in the monitor (`openttd://vehicles` `lost`, `rail_monitor.rhai`) is first a track
  question: walk its line with `AIRail.AreTilesConnected` and look for two same-way corners in a row.
- Never add a second train to a single track without the throat's exit signals. `trains_max` in the
  line record says how many the line holds.
- Related: [[the-bots-rail-pathfinder-lays-plain-track-only]], [[auto-approve-is-the-owners-switch]].
