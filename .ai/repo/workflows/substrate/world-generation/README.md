---
schema: context/v1
id: fork.substrate.world-generation
kind: context
title: "World generation: GenerateWorld phases, TGP and the original generator, heightmaps, seeds, the scenario editor"
description: "Read before changing map generation, a terrain generator, heightmap loading, the generation seed or its command-line flags, the scenario editor's new-map paths, or a research run that depends on a reproducible map."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/tgp.cpp'
  - 'src/tgp.h'
  - 'src/heightmap.*'
  - 'src/genworld*'
  - 'src/table/genland.h'
  - 'src/table/heightmap_colours.h'
  - 'docs/importing_town_data.md'
---

# World generation

Read at f8b1d52e80.

## Purpose and ownership

Owns the creation of a new map: the `GenerateWorld` driver and its phases, the TerraGenesis
Perlin generator (TGP), heightmap import (PNG/BMP), the flat world of the scenario editor, the
generation seed and its command-line and console entry points, the progress window, and abort
handling. It calls but does not own the subsystems that populate the map: terrain helpers and the
original generator in `src/landscape.cpp` ([map and tiles](../map-and-tiles/README.md)), towns and
industries ([towns, industries, stations](../towns-industries-stations/README.md)), objects
([rail, road and infrastructure](../rail-road-and-infrastructure/README.md)), and the switch-mode
plumbing in `src/openttd.cpp` ([core loop](../core-loop/README.md), which also tracks
`src/genworld.cpp`). No fork hunk in the tracked files (checked with
`git log --oneline upstream/master..HEAD`).

## Entry points

- `src/genworld.cpp:GenerateWorld` - public driver (`GenWorldMode` from `src/genworld.h`:
  `GWM_NEWGAME`, `GWM_EMPTY`, `GWM_RANDOM`, `GWM_HEIGHTMAP`); runs `_GenerateWorld` synchronously.
- `src/genworld.cpp:GenerateWorldSetCallback` (done callback, e.g. `src/openttd.cpp:MakeNewGameDone`),
  `src/genworld.cpp:AbortGeneratingWorld`, `src/genworld.cpp:HandleGeneratingWorldAbortion`.
- Callers in `src/openttd.cpp`: `MakeNewGame` (new game or heightmap), the `SwitchMode` cases
  `GenerateRandomLand` (`GWM_RANDOM`) and `LoadHeightmap` (editor), and `GWM_EMPTY` for the editor,
  the intro game and a failed load.
- GUI: `src/genworld_gui.cpp:ShowGenerateLandscape`, `src/genworld_gui.cpp:ShowHeightmapLoad`,
  `src/genworld_gui.cpp:ShowCreateScenario`, `src/genworld_gui.cpp:StartScenarioEditor`; headless:
  `src/genworld_gui.cpp:StartNewGameWithoutGUI` (console `newgame [seed]` and `restart` in
  `src/console_cmds.cpp:ConNewGame`, `src/console_cmds.cpp:ConRestart`).
- Generators: `src/tgp.cpp:GenerateTerrainPerlin`, `src/tgp.cpp:GetEstimationTGPMapHeight`;
  original: `src/landscape.cpp:GenerateTerrain`, both selected in `src/landscape.cpp:GenerateLandscape`.
- Heightmaps: `src/heightmap.cpp:LoadHeightmap`, `src/heightmap.cpp:GetHeightmapDimensions`,
  `src/heightmap.cpp:FixSlopes`, `src/heightmap.cpp:FlatEmptyWorld`.
- Town data for heightmaps: `src/genworld.cpp:LoadTownData` (JSON from the heightmap directory,
  see `docs/importing_town_data.md`), called from `src/fios_gui.cpp`.
- Command line: `-g [file]` and `-G seed` in `src/openttd.cpp:openttd_main`; usage text and
  `src/openttd.cpp:CreateOptions`.

## Lifecycle

- `GenerateWorld`: refuse while a modal progress runs; switch the local company to spectator;
  `src/misc.cpp:InitializeGame` (allocates the map); auto `map_height_limit` when 0 or heightmap
  mode (from `heightmap_height`, `se_flat_world_height` or `GetEstimationTGPMapHeight`); replace
  `GENERATE_NEW_SEED` with `InteractiveRandom()`; load sprites/NewGRFs, label lists, windows; then
  `_GenerateWorld`.
- `src/genworld.cpp:_GenerateWorld` (as `OWNER_NONE`, `_generating_world` set):
  `_random.SetSeed(generation_seed)`, script randomizers, `StartupEconomy`, `CheckTownRoadTypes`;
  unless `GWM_EMPTY`: `GenerateLandscape` (heightmap, TGP or original, then `FixSlopes`, rivers,
  rough/rocks, desert/rainforest via `src/table/genland.h:_make_desert_or_rainforest_data`). Empty:
  void borders, `FlatEmptyWorld`, `ConvertGroundTilesIntoWaterTiles`.
- Populate (not in the editor): `src/clear_cmd.cpp:GenerateClearTile`,
  `src/town_cmd.cpp:GenerateTowns`, `src/industry_cmd.cpp:GenerateIndustries`,
  `src/object_cmd.cpp:GenerateObjects`, `src/tree_cmd.cpp:GenerateTrees`.
- Start: `StartupCompanies`, `StartupEngines`, `StartupDisasters`; `_generating_world` cleared;
  `Game::StartNew`; unless `GWM_EMPTY` the tile loop runs 0x500 times; outside the editor the game
  script gets up to 2500 `Game::GameLoop` calls with `_generating_world` set again (until it
  sleeps); done callback; `CleanupGeneration`.
- Progress: every `IncreaseGeneratingWorldProgress` checks for abort and calls
  `VideoDriver::GameLoopPause` so the window can draw (`src/genworld_gui.cpp`); a dedicated server
  logs percentages at `Notice` severity of the `net` facility.
- Abort: `HandleGeneratingWorldAbortion` throws `AbortGenerateWorldSignal`; the catch in
  `_GenerateWorld` returns to the menu (editor: empty editor) or, on a dedicated server, exits.

## Invariants

- **[verified]** The map depends on the seed and the settings, not on the clock:
  `_GenerateWorld` seeds `_random` from `game_creation.generation_seed`; TGP's coast noise also mixes
  the seed in directly (`src/tgp.cpp:IntNoise`). Every phase above that draws `Random()` shifts the
  later ones, so inserting a draw changes every map generated after it.
- **[verified]** The seed is decided before generation: `GENERATE_NEW_SEED` (`UINT32_MAX`) means
  "pick one" and is replaced in `GenerateWorld`; `-g` without a file picks one at parse time, `-G`
  overrides it in either order. The seed is saved with the game (setting `generation_seed`,
  `NotInConfig`, since `SaveLoadVersion::Tgp`), and console `getseed` prints it.
- **[verified]** Map size comes from `game_creation.map_x`/`map_y` (log2, default 8 = 256), never
  from a heightmap image: `GreyscaleToMapHeights` scales and pads the image into the map
  (`heightmap_rotation` swaps axes); the GUI only reads image dimensions to suggest a size.
- **[verified]** Loading a heightmap overwrites `construction.freeform_edges` with
  `!IsMapSurroundedByWater()` (`src/heightmap.cpp:LoadHeightmap`).
- **[verified]** Image limits: each side at most `MAX_HEIGHTMAP_SIDE_LENGTH_IN_PIXELS`, total at
  most `MAX_HEIGHTMAP_SIZE_PIXELS` (`IsValidHeightmapDimension` in `src/heightmap.cpp`).
- **[verified]** Commands issued while `_generating_world` is set run locally and are not sent
  (commands map), which is why generation and the GS warm-up can build without a network round.
- **[verified]** The editor never gets towns, industries, objects or trees from `_GenerateWorld`;
  `GWM_EMPTY` also skips the tile loop and sets `snow_line_height` to the default.
- **[supported]** "Don't go from class 4 to 3" - progress classes must only increase
  (`src/genworld_gui.cpp:SetGeneratingWorldProgress` comment).
- **[inferred]** Multiplayer does not need cross-host reproducible generation: the server
  generates and clients download the savegame (ADR 0016). Bit-identical maps from one seed across
  compilers or platforms are not guaranteed: TGP shapes heights with `double` `sin`/`sqrt`
  (`src/tgp.cpp:HeightMapCurves`, `src/tgp.cpp:HeightMapSineTransform`).

## Extension points

- New terrain generator: a value next to `LG_ORIGINAL`/`LG_TERRAGENESIS` in `src/genworld.h`, a
  branch in `src/landscape.cpp:GenerateLandscape`, a height estimate for the auto height limit in
  `GenerateWorld`, the `land_generator` setting in `src/table/settings/world_settings.ini`.
- New heightmap format: a reader beside `ReadHeightmapPNG`/`ReadHeightmapBMP` dispatched from
  `ReadHeightMap` in `src/heightmap.cpp`, a `DetailedFileType` and a FIOS filter.
- New generation phase: a `src/genworld.h:GenWorldProgress` value (and the `percent_table` in
  `src/genworld_gui.cpp`, which is `static_assert`ed to its size) plus the call in
  `_GenerateWorld`, placed where its `Random()` draws do not disturb earlier phases.

## Traps and forbidden operations

- `-g` takes an optional value: the next argument is the file unless it starts with `-`
  (`ODF_OPTIONAL_VALUE` in `src/misc/getoptdata.cpp`). `-g -G 1` is safe; `-g 1` loads a file
  named `1`. `-G` is the seed, `-g <file>` the heightmap (knowledge `heightmap-flag-is-lowercase-g`).
- Opening the generate window replaces `_settings_newgame` `generation_seed` with a new random
  one (`_ShowGenerateLandscape` in `src/genworld_gui.cpp`); a seed set earlier is lost.
- `GenerateWorld` writes `map_height_limit` into the game settings when it was 0 (auto); it stays
  set for that game.
- `src/table/heightmap_colours.h` holds the smallmap's height colour ramps (`_green_map_heights`,
  used by `src/smallmap_gui.cpp`), not heightmap import data.
- TGP keeps its working height map in a process-static (`_height_map` in `src/tgp.cpp`); generation
  is single-threaded and not re-entrant.
- A generation failure on a dedicated server sets `_exit_game`: a headless research run ends
  instead of falling back to a menu; check its log, not only its exit code.
- `-d desync=1` makes generation save `dmp_cmds_<seed>_<date>.sav` into the autosave directory
  after logging `new_map: <seed>`; research runs with that flag leave files behind.

## Tests and validation

- No Catch2 test covers generation, and every regression case loads its `test.sav` with
  `-g` (`cmake/scripts/Regression.cmake`), so `ctest` never runs TGP or heightmap import.
- Validate by generating: `openttd -D -x -g -G <seed> -c <empty.cfg>` (the shape used by
  `tools/research/research` and `tools/research/mcp-rhai/src/panel.rs`, which
  `tools/research/mcp-ab` starts twice with one `--seed`); compare two runs with the same seed and
  config by saving and diffing the map, or by the MCP `openttd://map` resource.
- Tier: `tools/gate.sh change` builds and runs the suite; a generator change needs an explicit
  generation run as above, reported as such (`project.never-report-unrun-validation-as-pass`).
- Debug: `-d desync=1` (seed and dump save), `-d net=N` on a dedicated server (progress), `-d misc`.

## Decisions, rules and knowledge

- ADR 0016 (lockstep; clients receive the map, not the generation), ADR 0019 (generation settings in
  `src/table/settings/world_settings.ini`), ADR 0028 (base sets and NewGRFs are loaded inside
  `GenerateWorld`), ADR 0005 (research CLI drives runs).
- Rules: `project.never-report-unrun-validation-as-pass`,
  `project.research-code-is-gated-and-labelled`.
- Knowledge: `heightmap-flag-is-lowercase-g`, `macos-missing-baseset-hangs-headless-runs`,
  `a-heightmap-is-scaled-into-the-configured-map-and-sets-freeform-edges`.

## Typical tasks

- To make a research run reproducible: fix `-G <seed>`, an explicit config (`-c`, `-x` so it is
  not rewritten), the NewGRF and GS set, and the binary; record all of them in the experiment
  manifest (`tools/research/research`).
- To seed a world from elevation data: a PNG greyscale heightmap with `-g <file>`, `map_x`/`map_y`
  and `heightmap_height` set in the config; expect `freeform_edges` to follow the image's borders.
- To change TGP: start at `src/tgp.cpp:GenerateTerrainPerlin`; keep `Random()` call order unless a
  map change is intended; validate by generating fixed seeds before and after.
- To add a generation step: insert it in `src/genworld.cpp:_GenerateWorld` after the phases whose
  maps must stay the same, with its own progress class.
