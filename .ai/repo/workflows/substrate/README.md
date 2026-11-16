---
schema: context/v1
id: fork.substrate
kind: context
title: Working in OpenTTD's own code
description: What binds a change to src/, the build system and the regression suite of the system under test.
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 200
tracks:
  - 'src/**'
  - 'regression/**'
  - 'CMakeLists.txt'
  - 'cmake/**'
---

# Working in OpenTTD's own code

`src/`, `regression/` and the build system are the system under test (ADR 0001). Upstream's
technical constraints bind every change here, and its contribution process does not.

## Rules that apply

- `project.game-state-mutates-only-through-commands`: every player, script or network mutation
  goes through a registered command; handlers stay deterministic.
- `project.savegame-fields-are-version-gated`: no saved field is removed, reordered or
  renumbered.
- `project.research-code-is-gated-and-labelled`: research hooks compile only under
  `OPTION_RESEARCH_INSTRUMENTATION` (see `cmake/Options.cmake`).
- `project.never-report-unrun-validation-as-pass`: validate with `tools/gate.sh` at the tier the
  change needs.

## Subsystem maps

Each subsystem has a map below this directory whose `tracks` attach it to the files it owns:
`majordomus context resolve <path>` adds the right one to this document, so a worker reads the map
of what it is about to change and no other (skill `load-focused-context-for-a-change`). Every
`path:Symbol` a map cites is checked by `tools/knowledge/refs check`, and
`tools/knowledge/refs impact <range>` names the maps a diff makes stale. A **[fork]** bullet marks
fork code; everything else is upstream OpenTTD as read at the commit the map states.

<!-- surfaces:begin docs-index dir=.ai/repo/workflows/substrate depth=2 -->
| Document | What it is |
|---|---|
| [Commands: the path every player, script and network change to game state takes, its traits, phases and costs](commands/README.md) | Read before adding or changing a command, a Cmd handler, a DEF_CMD_TRAIT line or any code that calls Command<>::Post or ::Do. |
| [Core loop: process lifecycle, the real-time pump, the game tick, timers and pause](core-loop/README.md) | Read before touching startup/shutdown, GameLoop/StateGameLoop, a video driver's MainLoop, a timer, the date, world generation or pause handling. |
| [Economy and companies: money, prices, inflation, loans, bankruptcy, mergers and cargodist](economy-and-companies/README.md) | Read before changing company finances, prices, periodic economy timers or the threaded link graph. |
| [GUI and console: windows, widgets, viewport, input, hotkeys and the in-game console](gui-and-console/README.md) | Read before adding or changing a window, a widget tree, viewport behaviour, a hotkey or a console command, or when a feature must also work on a dedicated server. |
| [Map and tiles: the tile array, per-type tile procs, terrain, water, trees, bridges and tunnels](map-and-tiles/README.md) | Read before changing how a tile is stored, dispatched, looped, terraformed, cleared or flooded. |
| [Network: lockstep multiplayer transport, command distribution, desync checks, admin port and coordinator](network/README.md) | Read before changing anything under src/network/, or when a feature must work in a multiplayer game, over rcon or the admin port. |
| [NewGRF: GRF scanning, staged loading, per-action and per-feature handlers, callbacks and sprite-group resolution](newgrf/README.md) | When a change touches GRF loading or configuration, an action or Action 0 property handler, a callback/resolver, NewGRF storage, or GRF matching in savegames and multiplayer. |
| [PrismaticMCP (fork only): the embedded loopback MCP server, its view, UI and input tools, and the approval boundary](prismatic-mcp/README.md) | Read before changing src/prismatic_mcp/, its hooks in upstream files, or anything that lets an agent read or drive a running game. |
| [Saveload: savegame chunks, field tables, versions, compat and after-load conversion](saveload/README.md) | Read before adding, changing or removing anything that is written to a savegame, or touching chunk handlers, SaveLoadVersion or AfterLoadGame. |
| [Scripting: the Squirrel VM, the generated AI/GS API, AI and Game Script lifecycles, and the regression suite](scripting/README.md) | When a change touches a Script API class, the script VM or scheduler, AI/GS loading and versioning, or the regression scripts. |
| [Settings: declaration tables, settingsgen, config files, savegame and network sync](settings/README.md) | Read before adding or changing a setting, its flags or callbacks, the config-file layout, or any code that writes a setting at runtime. |
| [Towns, industries, stations and cargo: who produces, who holds and who moves cargo](towns-industries-stations/README.md) | Read before changing towns, industries, stations, road stops, cargo packets, cargo monitors or subsidies. |
| [Vehicles and pathfinding: the vehicle pool, per-type controllers, orders, depots, autoreplace, engines and YAPF](vehicles-and-pathfinding/README.md) | When a change reads or moves vehicles, edits orders/engines/autoreplace, or touches a pathfinder or its caches. |
<!-- surfaces:end docs-index -->

## Decisions and knowledge

- ADR 0004 (research access to internals), ADR 0008 (embedded MCP server).
- Knowledge `gated-code-is-invisible-to-a-default-build`, `macos-missing-baseset-hangs-headless-runs`,
  `heightmap-flag-is-lowercase-g`.
- Architecture reference: the "Architecture" section of `CLAUDE.md`, `CODINGSTYLE.md`,
  `docs/savegame_format.md`, `docs/desync.md`.
