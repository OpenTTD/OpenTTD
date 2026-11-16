---
schema: context/v1
id: fork.substrate.original-data-and-game-init
kind: context
title: "Original data tables and game init: default industry, house and cargo specs, tile draw layouts, town-name parts, NewGRF inspect tables, and InitializeGame"
description: "Read before editing a src/table/ data table of original TTD content (specs, draw layouts, town names, NewGRF debug variables) or the order of resets in InitializeGame."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/misc.cpp'
  - 'src/table/build_industry.h'
  - 'src/table/cargo_const.h'
  - 'src/table/town_land.h'
  - 'src/table/industry_land.h'
  - 'src/table/station_land.h'
  - 'src/table/clear_land.h'
  - 'src/table/tree_land.h'
  - 'src/table/water_land.h'
  - 'src/table/townname.h'
  - 'src/table/newgrf_debug_data.h'
---

# Original data tables and game init

Read at f8b1d52e80.

## Purpose and ownership

The residue of `src/` once every other subsystem map claimed its files: the compiled-in tables of
original TTD content that their subsystem `.cpp` includes as a private header, and
`src/misc.cpp`, which resets the world before a new or loaded game. Each table belongs functionally
to its consumer's map ([towns, industries, stations](../towns-industries-stations/README.md),
[map and tiles](../map-and-tiles/README.md), [NewGRF](../newgrf/README.md)); this map holds the
rules that are about the tables themselves. All upstream: `git log --oneline upstream/master..HEAD`
over these paths is empty.

Two kinds of table, with different stakes:

- **Gameplay defaults** (lockstep state is derived from them): `_origin_industry_specs`,
  `_origin_industry_tile_specs` (`src/table/build_industry.h`), `_original_house_specs`
  (`src/table/town_land.h`), `_default_cargo` and `_default_climate_cargo`
  (`src/table/cargo_const.h`), `_station_bridgeable_info` (`src/table/station_land.h`), the
  tree-type ranges in `src/table/tree_land.h`, and the town-name parts in `src/table/townname.h`.
- **Drawing only**: `_town_draw_tile_data`, `_industry_draw_tile_data` and its animation tables
  (`src/table/industry_land.h`), `_station_display_datas`, the clear/fence/farmland sprites
  (`src/table/clear_land.h`), tree sprite layouts, ship-depot and lock layouts
  (`src/table/water_land.h`), and the NewGRF inspect descriptors (`src/table/newgrf_debug_data.h`).

## Entry points

- `src/misc.cpp:InitializeGame` - called by `src/genworld.cpp:GenerateWorld` and by
  `src/saveload/saveload.cpp` before chunks load (`InitializeGame(256, 256, true, true)`).
- `src/misc.cpp:GenerateUid`, `GenerateSavegameId` - non-deterministic id for the save
  (`_game_session_stats.savegame_id`, saved by `src/saveload/misc_sl.cpp`, read by
  `src/survey.cpp`); also used by `src/network/network.cpp`.
- Spec resets from the defaults: `src/industry_cmd.cpp:ResetIndustries`,
  `src/newgrf_house.cpp:ResetHouses`, `src/cargotype.cpp:SetupCargoForClimate`, all driven by
  `src/newgrf.cpp:ResetNewGRFData`. NewGRF Action 0 copies an original industry as a base in
  `src/newgrf/newgrf_act0_industries.cpp`.
- Drawing consumers: `src/industry_cmd.cpp:DrawTile_Industry`, `src/town_cmd.cpp:GetTownDrawTileData`,
  `src/station_cmd.cpp:GetStationTileLayout`, `src/water_cmd.cpp`, `src/clear_cmd.cpp`,
  `src/tree_cmd.cpp`, `src/tree_gui.cpp`.
- Town names: `src/townname.cpp:GenerateTownNameString` dispatches through
  `src/townname.cpp:_town_name_generators` to `MakeEnglishOriginalTownName` and its siblings, which
  index the `_name_*` arrays with `SeedChance`/`SeedModChance`. The old loader
  (`src/saveload/oldloader_sl.cpp`) includes the same header.
- NewGRF inspect: `src/newgrf_debug_gui.cpp:GetFeatureHelper` reads `_nifeatures`.

## Lifecycle

- `InitializeGame` order: close every window (`UnInitWindowSystem`), `Map::Allocate`, clear pause,
  game speed, `TimerGameTick::counter` and the tile-loop cursor; `MakeNewgameSettingsLive` when
  `reset_settings`; reset calendar and economy dates when `reset_date` (economy starts at year 1
  under wallclock units, else follows the calendar, ADR 0025) together with old names;
  `LinkGraphSchedule::Clear`; `PoolBase::Clean(PoolType::Normal)`; rebuild the kd-trees; reset
  persistent NewGRF data; then the per-subsystem `Initialize*` calls (sound, music, vehicles, news,
  landscape, build GUIs, trees, industries, objects, companies, AI, GS, cheats, text effects, chat,
  animated tiles, economy); finally reset the gamelog and log a `Start` action.
- Spec tables are copied into the live `_industry_specs`, `_house_specs` and `CargoSpec` arrays on
  every `ResetNewGRFData`, then NewGRFs override the copies; the originals stay `const`.
- Drawing tables are read every frame by the tile procs (ADR 0023).

## Invariants

- **[verified]** `InitializeGame` cleans only `PoolType::Normal` pools; network client/admin and
  `Data` pools survive it (`src/core/pool_type.hpp:PoolType`). It does not reseed `_random`.
- **[verified]** Length checks are compile-time: `static_assert`s in `src/table/town_land.h` tie
  `_original_house_specs` to `NEW_HOUSE_OFFSET` and `_town_draw_tile_data` to `NEW_HOUSE_OFFSET`
  times 4 stages times 4 variants; `src/table/station_land.h` ties the rail and waypoint layouts.
  `_origin_industry_specs`/`_origin_industry_tile_specs` are sized by `NEW_INDUSTRYOFFSET` and
  `NEW_INDUSTRYTILEOFFSET` (`src/industry_type.h`).
- **[verified]** Draw-table indices are derived from saved map state: `_town_draw_tile_data` by
  house id, a tile hash and the building stage (`src/town_cmd.cpp`), `_industry_draw_tile_data` by
  industry gfx and animation/construction state (`src/industry_cmd.cpp:DrawTile_Industry`).
  Reordering an entry misdraws every existing save; it does not desync.
- **[verified]** `_station_bridgeable_info` decides whether a bridge may pass over a station tile in
  `src/station_cmd.cpp:IsRailStationBridgeAboveOk` and its siblings, so it is command-relevant
  game data despite living in the draw-layout header.
- **[verified]** Tree-type ranges (`_tree_base_by_landscape`, `_tree_count_by_landscape`) are
  checked in `src/tree_cmd.cpp:CmdPlantTree` as well as used by `src/tree_gui.cpp`.
- **[verified]** Town names are not saved as text: a town stores `townnameparts`
  (`src/saveload/town_sl.cpp`) and the name is regenerated from `src/table/townname.h` on each read;
  `src/townname.cpp:VerifyTownName` compares regenerated strings for uniqueness inside
  `src/town_cmd.cpp:CmdFoundTown` and `GenerateTowns` (knowledge
  `town-names-are-regenerated-from-the-name-tables`).
- **[inferred]** `GenerateUid` uses `RandomBytesWithFallback` and the wall clock, never `_random`,
  so it may run outside the lockstep without desyncing; the id reaches clients inside the save.

## Extension points

- New original industry, house or cargo: not an extension point; the offsets
  (`NEW_INDUSTRYOFFSET`, `NEW_HOUSE_OFFSET`) bound saved ids and NewGRF id spaces. Add content
  through a NewGRF (ADR 0028).
- New NewGRF inspect variable: a row in the feature's `NIVariable` array in
  `src/table/newgrf_debug_data.h`; a new feature needs an `NIFeature` and its slot in `_nifeatures`.
- New town-name language: a generator in `src/townname.cpp`, its parts in
  `src/table/townname.h`, appended to `_town_name_generators` (the index is the saved
  `game_creation.town_name` setting) and a string in `english.txt` (ADR 0020).
- New per-game reset: add an `Initialize*` call to `InitializeGame` in dependency order (after
  `PoolBase::Clean`, before `InitializeCompanies` if companies read it).

## Traps and forbidden operations

- Editing any `_name_*` array in `src/table/townname.h` renames towns (and default company names,
  which use the same generators via `src/strings.cpp`) in every existing save and changes which
  seeds `GenerateTownName` accepts, so world generation from the same seed differs; the regression
  suite prints English town names and will fail.
- Changing a gameplay default (an `IndustrySpec` production rate, a `HouseSpec` population, a
  `CargoSpec`) changes simulation for every game without a NewGRF override; peers on different
  builds desync (ADR 0016). Drawing tables only change pixels.
- `src/table/*.h` headers are included by exactly one or two translation units and define `static`
  data; including one in a new `.cpp` duplicates the table. `_original_house_specs` and
  `_origin_industry_specs` are `extern const` precisely so other files can reach them.
- `InitializeGame` first closes every window; a caller holding a `Window *` across it holds a
  dangling pointer.
- `src/table/newgrf_debug_data.h` runs NewGRF variable resolution from a GUI; it must only read
  (ADR 0016, `project.game-state-mutates-only-through-commands`).

## Tests and validation

- No Catch2 test in `src/tests/` covers these tables or `InitializeGame`.
- `regression/regression/main.nut` prints town names (`AITown.GetName`) and industry data against
  `regression/regression/result.txt` with `town_name = english` from `regression/regression.cfg`;
  it runs as `regression_regression` (`ctest -R regression_regression`).
- Tier: `tools/gate.sh change` for any table edit; drawing-only edits need a visual check in a
  running game as well. Debug: `-d grf=1` while NewGRFs override the defaults.

## Decisions, rules and knowledge

- ADR 0016 (lockstep), ADR 0018 (saved ids index these tables), ADR 0023 (tile procs read the draw
  tables), ADR 0025 (economy date reset), ADR 0028 (original content is replaceable data).
- Rules: `project.game-state-mutates-only-through-commands`,
  `project.savegame-fields-are-version-gated`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `town-names-are-regenerated-from-the-name-tables`.
- Sibling maps: [core loop](../core-loop/README.md), [saveload](../saveload/README.md),
  [world generation](../world-generation/README.md), [core utilities](../core-utilities/README.md).

## Typical tasks

- To find who reads a table: `git grep -n 'table/<name>.h' -- src`, then the consumer's map.
- To add a reset to new games: edit `src/misc.cpp:InitializeGame`, check both the genworld and the
  load path, validate with `tools/gate.sh change`.
- To expose a NewGRF variable in the inspect window: add an `NIVariable` row in
  `src/table/newgrf_debug_data.h`, check it in a running game with the inspect tool.
