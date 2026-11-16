---
schema: context/v1
id: fork.substrate.map-and-tiles
kind: context
title: "Map and tiles: the tile array, per-type tile procs, terrain, water, trees, bridges and tunnels"
description: "Read before changing how a tile is stored, dispatched, looped, terraformed, cleared or flooded."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/map.cpp'
  - 'src/map_func.h'
  - 'src/map_type.h'
  - 'src/tile_type.h'
  - 'src/tile_cmd.h'
  - 'src/tile_map.h'
  - 'src/tile_map.cpp'
  - 'src/*_map.h'
  - 'src/landscape*'
  - 'src/terraform_cmd.*'
  - 'src/clear_*'
  - 'src/tree_cmd.*'
  - 'src/water_cmd.*'
  - 'src/water.h'
  - 'src/tunnelbridge*'
  - 'src/tunnel_map.cpp'
  - 'src/bridge_map.cpp'
  - 'src/bridge.h'
  - 'src/void_cmd.cpp'
  - 'src/saveload/map_sl.cpp'
  - 'docs/landscape*.html'
---

# Map and tiles

## Purpose and ownership

Owns the global tile array (a `Tile` wrapper over two static arrays), the per-type behaviour
table `_tile_type_procs`, the periodic tile loop, world terrain generation, and the commands that
reshape land: terraform, clear, trees, canals/rivers/locks, bridges and tunnels. Everything else
(rail, road, station, house, industry, object) stores itself in these same tile bytes through a
`*_map.h` accessor header and plugs its behaviour into the same table.

## Entry points

- `src/map_func.h:Tile` - accessor for `type`, `height`, `m1`..`m8` of one tile; `src/map_func.h:Map` - sizes, `Iterate`, scaling helpers.
- `src/map.cpp:Map::Allocate` - (re)allocates the arrays; called by `src/misc.cpp:InitializeGame`, the `MAPS` chunk load in `src/saveload/map_sl.cpp`, and unit tests.
- `src/tile_map.h:GetTileType` / `src/tile_map.h:SetTileType` / `src/tile_map.h:GetTileOwner` - the shared low-level accessors; per-type ones live in `src/rail_map.h`, `src/road_map.h`, `src/station_map.h`, `src/water_map.h`, `src/clear_map.h`, `src/tree_map.h`, `src/bridge_map.h`, `src/tunnel_map.h`, `src/town_map.h`, `src/industry_map.h`, `src/object_map.h`, `src/void_map.h`.
- `src/tile_cmd.h:TileTypeProcs` - the callback struct; `src/landscape.cpp:_tile_type_procs` - the table indexed by `src/tile_type.h:TileType`.
- `src/landscape.cpp:RunTileLoop` and `src/landscape.cpp:CallLandscapeTick` - per-tick drivers, called from `src/openttd.cpp:StateGameLoop`.
- `src/landscape.cpp:CmdLandscapeClear`, `src/landscape.cpp:CmdClearArea`, `src/terraform_cmd.cpp:CmdTerraformLand`, `src/terraform_cmd.cpp:CmdLevelLand`, `src/tree_cmd.cpp:CmdPlantTree`, `src/water_cmd.cpp:CmdBuildCanal`, `src/water_cmd.cpp:CmdBuildLock`, `src/tunnelbridge_cmd.cpp:CmdBuildBridge`, `src/tunnelbridge_cmd.cpp:CmdBuildTunnel`.
- `src/landscape.cpp:GenerateLandscape` - terrain generation, driven by `src/genworld.cpp:GenerateWorld`.
- `src/landscape.cpp:ChangeTileOwner` - dispatches `change_tile_owner_proc`; the whole-map pass on merger/bankruptcy is in `src/economy.cpp:ChangeOwnershipOfCompanyItems`.
- **[fork]** `src/prismatic_mcp/prismatic_mcp_world.cpp:TypeCode` and `src/prismatic_mcp/prismatic_mcp_world.cpp:OwnerCode` read the map (read-only, gated by `OTTD_PRISMATIC_MCP_SERVER`) and encode `TileType` by position in the string `TYPE_CODES`.

## Lifecycle

- **Init**: `src/misc.cpp:InitializeGame` calls `Map::Allocate`, which also calls `src/pathfinder/water_regions.cpp:AllocateWaterRegions`. Generation: `src/landscape.cpp:InitializeLandscape` (clear grass, void border), `GenerateLandscape`, `src/clear_cmd.cpp:GenerateClearTile`, `src/tree_cmd.cpp:GenerateTrees`; then `src/genworld.cpp` runs `RunTileLoop` 0x500 times so the land "ages" before play.
- **Per tick** (`src/openttd.cpp:StateGameLoop`, not paused): `AnimateAnimatedTiles`, timers, `RunTileLoop`, vehicles, then `CallLandscapeTick` (`OnTick_Town`, `src/tree_cmd.cpp:OnTick_Trees`, `OnTick_Station`, `OnTick_Industry`, `OnTick_Companies`, `OnTick_LinkGraph`). The scenario editor runs the same three without timers.
- **Per tile**: `tile_loop_proc` of the tile's type, e.g. `src/clear_cmd.cpp:TileLoop_Clear`, `src/tree_cmd.cpp:TileLoop_Trees`, `src/water_cmd.cpp:TileLoop_Water` (also called by other types' loops for flooding).
- **Save/load**: `src/saveload/map_sl.cpp` stores one chunk per byte plane: `MAPS` (dimensions), `MAPT` type, `MAPH` height, `MAPO` m1, `MAP2` m2, `M3LO` m3, `M3HI` m4, `MAP5` m5, `MAPE` m6, `MAP7` m7, `MAP8` m8. Old bit layouts are rewritten in `src/saveload/afterload.cpp:AfterLoadGame`. The tile-loop cursor is saved in `src/saveload/misc_sl.cpp` (`cur_tileloop_tile`).
- **Teardown**: none explicit; the arrays are replaced by the next `Map::Allocate`.

## Invariants

- **[verified]** Map dimensions are powers of two in [`MIN_MAP_SIZE`, `MAX_MAP_SIZE`]; anything else is a `FatalError` - `src/map.cpp:Map::Allocate`, `src/map_type.h:MAX_MAP_SIZE_BITS`. Tile indices are `y << log_x | x`, so wrap/mask helpers in `src/map_func.h:TileAddWrap` rely on this.
- **[verified]** Per-tile storage is 8 bytes `TileBase` (type, height, m1..m5) plus 4 bytes `TileExtended` (m6, m7, m8) - `src/map_func.h:TileBase`, `src/map_func.h:TileExtended`. There is no spare field; new data must reuse documented free bits.
- **[supported]** Bit meanings per tile type are documented only in `docs/landscape.html` and `docs/landscape_grid.html`; every accessor in `src/map_func.h:Tile` says "Look at docs/landscape.html".
- **[verified]** `TileType::Void` appears exactly on the outer edge (south-west/south-east always, plus north edges with freeform edges); `src/tile_map.h:SetTileType` asserts `IsInnerTile(tile) == (type != Void)`, and `src/landscape.cpp:InitializeLandscape` lays the border.
- **[verified]** Owner lives in m1 bits 0..4, and House and Industry tiles have no owner: `src/tile_map.h:GetTileOwner` asserts on both. Use `src/town_map.h:GetTownIndex` / `src/industry_map.h:GetIndustryIndex` instead.
- **[verified]** All per-type behaviour dispatches through `src/landscape.cpp:_tile_type_procs[GetTileType(tile)]`; the unused slots up to `TileType::MaxSize` are `nullptr`, and optional procs (`add_accepted_cargo_proc`, `animate_tile_proc`, `check_build_above_proc`, ...) are null-checked by callers in `src/tile_cmd.h`.
- **[verified]** The tile loop is deterministic: `src/landscape.cpp:RunTileLoop` visits 2^(log_x+log_y-8) tiles per tick in Galois-LFSR order from `_cur_tileloop_tile`, plus tile 0 every 256 ticks. The cursor must never be 0 (`assert`); `AfterLoadGame` repairs a saved 0.
- **[verified]** Bridges are not a tile type on their middle tiles: the "bridge above" bits sit in the type byte (`src/bridge_map.h:IsBridgeAbove`) and the ramps are found by walking (`src/bridge_map.cpp:GetSouthernBridgeEnd`). Tunnel middles carry no marker at all; `src/tunnel_map.cpp:IsTunnelInWayDir` scans outward for a portal.
- **[verified]** Terraforming is modelled before it is applied: `src/terraform_cmd.cpp:CmdTerraformLand` collects new heights in `TerraformerState`, runs each affected tile's `terraform_tile_proc` twice (pass 0 without `Execute`, pass 1 costed), and writes heights with `SetTileHeight` only at the end of execution.
- **[verified]** Clearing any tile next to water must reset the neighbours' non-flooding cache bit (m3 bit 0, `src/water_map.h:IsNonFloodingWaterTile`): `src/landscape.cpp:DoClearSquare` calls `src/water_cmd.cpp:ClearNeighbourNonFloodingStates` and `src/pathfinder/water_regions.cpp:InvalidateWaterRegion`.
- **[verified]** Flooding acts as `OWNER_WATER`: `src/water_cmd.cpp:DoFloodTile` swaps `_current_company` for its duration.
- **[verified]** Multi-tile objects are cleared once per command: `src/object_base.h:ClearedObjectArea` entries are reset at each top-level command in `src/command.cpp`, and `CmdLandscapeClear` only calls the proc for the first tile of an area.

## Extension points

- New tile type: `src/tile_type.h:TileType` (4 bits, `TILE_TYPE_BITS`), a `TileTypeProcs` instance in the new `*_cmd.cpp`, a slot in `src/landscape.cpp:_tile_type_procs`, an accessor header named after the type (`*_map.h`), the bit table in `docs/landscape.html`, a savegame version bump, and the fork's `TYPE_CODES` string in `src/prismatic_mcp/prismatic_mcp_world.cpp`.
- New per-tile data: free bits per type in `docs/landscape_grid.html`; accessor in the type's `*_map.h`; conversion of old saves in `src/saveload/afterload.cpp:AfterLoadGame` behind a new `SaveLoadVersion`.
- New landscape command: handler in the `*_cmd.cpp`, `DEF_CMD_TRAIT` in its `*_cmd.h` (e.g. `src/landscape_cmd.h`, `src/terraform_cmd.h`), enum value in `src/command_type.h:Commands`.
- New per-tick landscape work: add to `src/landscape.cpp:CallLandscapeTick`, or an `IntervalTimer` with its own priority (see the economy context).

## Traps and forbidden operations

- Never call `Random()` in a command's test run: `src/terraform_cmd.cpp:CmdTerraformLand` only rolls `RandomRange` for rocks under `DoCommandFlag::Execute` because "a test run of RandomRange will cause a desync".
- Commands whose test and execution legitimately differ carry `CommandFlag::NoTest`: `ClearArea` (house removal changes town rating) in `src/landscape_cmd.h`, `LevelLand` in `src/terraform_cmd.h`. Copy that flag, with its comment, if a new command has the same property.
- Writing `SetTileType` on an edge tile, or `SetTileOwner` on a house/industry tile, asserts in debug builds and silently corrupts in release builds (asserts off).
- Reordering `TileType` breaks savegames (`MAPT` stores the raw nibble) and the fork's MCP encoding.
- Company limits: `clear_limit`, `terraform_limit`, `tree_limit` are fixed-point (upper 16 bits = whole actions), refilled every tick by `src/company_cmd.cpp:UpdateLandscapingLimits`; a scripted bulk operation can exhaust them.

## Tests and validation

- Unit: `src/tests/landscape_partial_pixel_z.cpp` (slope/height arithmetic), `src/tests/tilearea.cpp` (allocates a 64x64 map; spiral iteration).
- Regression: `regression/regression/main.nut` sections `Map`, `Tile`, `TileList`, `Terraforming`, `Bridge`, `Tunnel`, `Marine` (ctest `regression_regression`; needs a base set).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R <regex>` while iterating.
- Debug: `-d map=N` (map allocation is logged at error level), `-d desync=2` runs `src/cachecheck.cpp:CheckCaches` every tick (`docs/desync.md`).

## Decisions, rules and knowledge

- Rules: `project.game-state-mutates-only-through-commands`, `project.savegame-fields-are-version-gated`, `project.research-code-is-gated-and-labelled` (the fork reader above).
- ADR 0004 (research access to internals), ADR 0008 (embedded MCP server reads the map).
- Knowledge: `road-stops-and-depots-need-their-entrance-road`, `heightmap-flag-is-lowercase-g`.
- Upstream docs: `docs/landscape.html`, `docs/landscape_grid.html`, `docs/savegame_format.md`, `docs/desync.md`.

## Typical tasks

- To read or add a per-tile field: start at the type's `*_map.h` (e.g. `src/water_map.h:SetNonFloodingWaterTile`), confirm the free bits in `docs/landscape_grid.html`, then gate old saves in `src/saveload/afterload.cpp:AfterLoadGame`; validate with `tools/gate.sh change`.
- To change what happens to a tile over time: start at its `tile_loop_proc` (e.g. `src/clear_cmd.cpp:TileLoop_Clear`), keep `Random()` order unchanged unless intended; validate with `-d desync=2` on a saved game and `tools/gate.sh change`.
- To change terraform rules: start at `src/terraform_cmd.cpp:CmdTerraformLand`, then the per-type `terraform_tile_proc` (e.g. `src/tunnelbridge_cmd.cpp:TerraformTile_TunnelBridge`); validate with the `Terraforming` regression section.
- To change flooding: start at `src/water_cmd.cpp:TileLoop_Water` and `src/water_cmd.cpp:DoFloodTile`, keep `ClearNeighbourNonFloodingStates` calls on every new clear path.
- To expose map data to an agent: extend `src/prismatic_mcp/prismatic_mcp_world.cpp:Map` under the research gate; validate with `tools/gate.sh research`.
