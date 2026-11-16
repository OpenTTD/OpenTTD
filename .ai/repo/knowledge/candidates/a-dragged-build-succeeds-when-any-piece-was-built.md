---
schema: knowledge/v1
id: a-dragged-build-succeeds-when-any-piece-was-built
kind: knowledge
class: lesson
title: "A dragged rail or road build reports success when any piece was built; with the GUI's arguments it stops silently at the first obstacle after that"
description: "When a caller (GUI, the fork's mcp-human player, research code) builds track or road with BuildRailLong or BuildRoadLong and treats success as 'the whole run exists'."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [rail, road, commands, mcp-human]
about: src/rail_cmd.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/rail_cmd.cpp
    - file:src/road_cmd.cpp
    - file:src/rail_gui.cpp
    - file:src/road_gui.cpp
    - file:src/script/api/script_rail.cpp
    - file:src/script/api/script_road.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/rail-road-and-infrastructure/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
---

# Statement

`src/rail_cmd.cpp:CmdRailTrackHelper` (behind `BuildRailLong`) and
`src/road_cmd.cpp:CmdBuildLongRoad` (behind `BuildRoadLong`) loop over the dragged tiles. With
`fail_on_obstacle` false (rail) or `is_ai` false (road), an error other than "already built" before
any success is skipped, and the first such error after a success ends the loop: the command
returns the cost of the pieces built so far and succeeds. "Already built" pieces count as success.
The GUI passes false (`src/rail_gui.cpp:DoRailroadTrack`, `src/road_gui.cpp:BuildRoadToolbarWindow`);
the script API passes true (`src/script/api/script_rail.cpp:ScriptRail::BuildRail`,
`src/script/api/script_road.cpp:ScriptRoad::_BuildRoadInternal`) and then fails on the first
obstacle. A caller that drives the GUI (the fork's `tools/research/mcp-human/src/play.rs`) must
verify the built run on the map (e.g. `openttd://map/ways`) rather than trust the result.

Also: the Doxygen of `src/rail_cmd.cpp:CmdBuildRailroadTrack` swaps the descriptions of
`auto_remove_signals` and `fail_on_obstacle`.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote a-dragged-build-succeeds-when-any-piece-was-built` with evidence, or `majordomus knowledge reject a-dragged-build-succeeds-when-any-piece-was-built --reason ...`.
