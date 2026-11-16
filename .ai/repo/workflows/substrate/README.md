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
| [Build and packaging: CMake options, add_files source lists, generated sources, version stamping, compilers, packaging and the fork's build profiles](build-and-packaging/README.md) | Read before changing a CMakeLists.txt or cmake/ module, adding or moving a source file, adding a CMake option or dependency, touching version stamping or packaging, or choosing which build configuration validates a change. |
| [Commands: the path every player, script and network change to game state takes, its traits, phases and costs](commands/README.md) | Read before adding or changing a command, a Cmd handler, a DEF_CMD_TRAIT line or any code that calls Command<>::Post or ::Do. |
| [Consists, groups and timetables: front-vehicle state, vehicle groups and their statistics, timetables, the build-vehicle window, disasters, effect vehicles and the original engine tables](consists-groups-and-timetables/README.md) | Read before changing BaseConsist fields, a group or timetable command, group statistics, the build-vehicle window, a disaster or an effect vehicle, or the original engine/sprite/road-drive tables. |
| [Core loop: process lifecycle, the real-time pump, the game tick, timers and pause](core-loop/README.md) | Read before touching startup/shutdown, GameLoop/StateGameLoop, a video driver's MainLoop, a timer, the date, world generation or pause handling. |
| [Core utilities: pools, strong IDs, enum bitsets, bit and integer math, random, strings, containers, debug output, safeguards and threads](core-utilities/README.md) | Read before changing src/core/ or src/misc/, adding a pool or strong ID type, drawing random numbers, adding Debug() output, starting a thread, or including a C function safeguards.h bans. |
| [Economy and companies: money, prices, inflation, loans, bankruptcy, mergers and cargodist](economy-and-companies/README.md) | Read before changing company finances, prices, periodic economy timers or the threaded link graph. |
| [Game windows: main window, toolbars, status bar, intro, query and error windows, pickers, smallmap, graphs, company windows, framerate and help](game-windows/README.md) | Read before adding or changing a feature window, a toolbar button or menu entry, a global hotkey, an error or query dialog, a graph, a picker, or a framerate measurement point. |
| [Goals, story, news and history: GS goals, story pages and league tables, the news queue, signs, cheats, the gamelog and highscores](goals-story-news-and-history/README.md) | Read before changing a goal, story page, league table, news item or sign (command, pool or window), a cheat, anything that writes or reads the gamelog, or the highscore table. |
| [Graphics and sprites: blitters, sprite cache and loaders, zoom, palette animation, dirty-rect redraw, fonts, text layout and base sets](graphics-and-sprites/README.md) | Read before changing a blitter, the sprite cache or a sprite loader, zoom or palette code, the dirty-block redraw, fonts or text layout, screenshots, or base set discovery. |
| [GUI and console: windows, widgets, viewport, input, hotkeys and the in-game console](gui-and-console/README.md) | Read before adding or changing a window, a widget tree, viewport behaviour, a hotkey or a console command, or when a feature must also work on a dedicated server. |
| [Map and tiles: the tile array, per-type tile procs, terrain, water, trees, bridges and tunnels](map-and-tiles/README.md) | Read before changing how a tile is stored, dispatched, looped, terraformed, cleared or flooded. |
| [Network: lockstep multiplayer transport, command distribution, desync checks, admin port and coordinator](network/README.md) | Read before changing anything under src/network/, or when a feature must work in a multiplayer game, over rcon or the admin port. |
| [NewGRF: GRF scanning, staged loading, per-action and per-feature handlers, callbacks and sprite-group resolution](newgrf/README.md) | When a change touches GRF loading or configuration, an action or Action 0 property handler, a callback/resolver, NewGRF storage, or GRF matching in savegames and multiplayer. |
| [Original data tables and game init: default industry, house and cargo specs, tile draw layouts, town-name parts, NewGRF inspect tables, and InitializeGame](original-data-and-game-init/README.md) | Read before editing a src/table/ data table of original TTD content (specs, draw layouts, town names, NewGRF debug variables) or the order of resets in InitializeGame. |
| [Platform and files: OS glue, search paths and Subdirectory, tar and file scanning, ini files, the driver registry, crash logs, threads, survey and plugins](platform-and-files/README.md) | Read before changing src/os/, file lookup or search paths, the save/load file browser, ini parsing, driver selection, crash handling, thread start-up, the survey or social-integration plugins, or how a headless run finds its files. |
| [PrismaticMCP (fork only): the embedded loopback MCP server, its view, UI and input tools, and the approval boundary](prismatic-mcp/README.md) | Read before changing src/prismatic_mcp/, its hooks in upstream files, or anything that lets an agent read or drive a running game. |
| [Rail, road and infrastructure: rail/road types, track bits, signals and path reservations, waypoints, airports, objects, tile areas](rail-road-and-infrastructure/README.md) | Read before changing track or road building, rail/road types, signals or PBS reservations, waypoints, airport layouts, objects (HQ, lighthouses), tile-area iterators, or the construction toolbars. |
| [Saveload: savegame chunks, field tables, versions, compat and after-load conversion](saveload/README.md) | Read before adding, changing or removing anything that is written to a savegame, or touching chunk handlers, SaveLoadVersion or AfterLoadGame. |
| [Scripting: the Squirrel VM, the generated AI/GS API, AI and Game Script lifecycles, and the regression suite](scripting/README.md) | When a change touches a Script API class, the script VM or scheduler, AI/GS loading and versioning, or the regression scripts. |
| [Settings: declaration tables, settingsgen, config files, savegame and network sync](settings/README.md) | Read before adding or changing a setting, its flags or callbacks, the config-file layout, or any code that writes a setting at runtime. |
| [Sound and music: sound and music drivers, the mixer thread, sound effects, sound loaders, base sound and music sets](sound-and-music/README.md) | Read before changing a sound or music driver, the mixer, a sound loader or resampler, how a game event plays a sound effect, the music player, or base sound/music set handling. |
| [Strings and languages: english.txt, strgen, StringID and parameters, plurals, genders and cases, translations, text input and currencies](strings-and-languages/README.md) | Read before adding or changing a player-facing string, a string parameter or control code, strgen, a language file, the language loader, a text box, autocompletion or a currency. |
| [Third-party code: vendored fmt, nlohmann json, monocypher, Catch2, md5, ICU scriptrun, OpenGL headers and the social integration API, their licences and build](third-party/README.md) | Read before editing, upgrading or newly including anything under src/3rdparty/ (except Squirrel), or before adding a vendored library. |
| [Towns, industries, stations and cargo: who produces, who holds and who moves cargo](towns-industries-stations/README.md) | Read before changing towns, industries, stations, road stops, cargo packets, cargo monitors or subsidies. |
| [Unit tests: the Catch2 openttd_test binary, its ctest registration, mocks, coverage and gate tiers](unit-tests/README.md) | Read before adding or changing a test under src/tests/, a mock, the Catch2 CMake glue, or before claiming a change is covered by a unit test. |
| [Vehicles and pathfinding: the vehicle pool, per-type controllers, orders, depots, autoreplace, engines and YAPF](vehicles-and-pathfinding/README.md) | When a change reads or moves vehicles, edits orders/engines/autoreplace, or touches a pathfinder or its caches. |
| [World generation: GenerateWorld phases, TGP and the original generator, heightmaps, seeds, the scenario editor](world-generation/README.md) | Read before changing map generation, a terrain generator, heightmap loading, the generation seed or its command-line flags, the scenario editor's new-map paths, or a research run that depends on a reproducible map. |
<!-- surfaces:end docs-index -->

## Decisions and knowledge

- ADR 0004 (research access to internals), ADR 0008 (embedded MCP server).
- Knowledge `gated-code-is-invisible-to-a-default-build`, `macos-missing-baseset-hangs-headless-runs`,
  `heightmap-flag-is-lowercase-g`.
- Architecture reference: the "Architecture" section of `CLAUDE.md`, `CODINGSTYLE.md`,
  `docs/savegame_format.md`, `docs/desync.md`.
