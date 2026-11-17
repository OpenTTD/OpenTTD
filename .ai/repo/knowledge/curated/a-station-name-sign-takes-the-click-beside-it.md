---
schema: knowledge/v1
id: a-station-name-sign-takes-the-click-beside-it
kind: knowledge
class: lesson
title: "A station's name sign takes a click on the tiles beside the station"
description: "When a person-like player (mcp-human) clicks a tile next to an existing station (a second platform, a depot) and nothing happens and the game says nothing."
status: verified
epistemics: observed
date: 2026-11-17
tags: [mcp-human, gui, station, yard, click]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-human/src/rail.rs
    - file:tools/research/mcp-human/src/play.rs
    - file:research/mcp-human.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/the-bots-rail-pathfinder-lays-plain-track-only.md
---

# Lesson

The game hits signs before the ground, at any zoom. A station's name sign is wider than the
station and lies over the tiles beside it, so a click there opens nothing and builds nothing, and no
error window appears. On seed 4242 (2026-11-17) the yard's second platform at 73,217, right beside
the first line's station at 72,217, failed twice this way while the server validated the very same
build (`openttd.decision.validate`: would succeed, cost 2070). The same happened earlier to depots
behind a station.

# How to apply it

When a click beside a station has no effect and the game says nothing, do not conclude the build is
impossible: validate it on the server (`openttd.decision.validate` with the command and its
arguments), and if it would succeed, hide station names (Options menu, "station names displayed"),
re-arm the tool, click, and show the names again. `mcp-human` does this on its third try for
stations (`rail.rs` `build_rail_station`) and on its second for depots (`play.rs` `open_depot`).
