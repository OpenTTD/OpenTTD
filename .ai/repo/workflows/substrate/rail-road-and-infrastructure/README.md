---
schema: context/v1
id: fork.substrate.rail-road-and-infrastructure
kind: context
title: "Rail, road and infrastructure: rail/road types, track bits, signals and path reservations, waypoints, airports, objects, tile areas"
description: "Read before changing track or road building, rail/road types, signals or PBS reservations, waypoints, airport layouts, objects (HQ, lighthouses), tile-area iterators, or the construction toolbars."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/rail.cpp'
  - 'src/rail.h'
  - 'src/rail_gui.*'
  - 'src/rail_type.h'
  - 'src/rail_cmd.*'
  - 'src/road.*'
  - 'src/road_func.h'
  - 'src/road_gui.*'
  - 'src/road_internal.h'
  - 'src/road_map.cpp'
  - 'src/road_type.h'
  - 'src/road_cmd.*'
  - 'src/elrail*'
  - 'src/pbs.*'
  - 'src/signal*'
  - 'src/track_*'
  - 'src/waypoint*'
  - 'src/airport*'
  - 'src/object*'
  - 'src/bridge_gui.cpp'
  - 'src/bridge_type.h'
  - 'src/dock_gui.cpp'
  - 'src/terraform_gui.*'
  - 'src/tree_gui.cpp'
  - 'src/autoslope.h'
  - 'src/slope_*'
  - 'src/direction_*'
  - 'src/tilearea*'
  - 'src/transport_type.h'
  - 'src/animated_tile*'
  - 'src/table/railtypes.h'
  - 'src/table/roadtypes.h'
  - 'src/table/airport_defaults.h'
  - 'src/table/airport_movement.h'
  - 'src/table/airporttile_ids.h'
  - 'src/table/airporttiles.h'
  - 'src/table/track_data.h'
  - 'src/table/track_land.h'
  - 'src/table/elrail_data.h'
  - 'src/table/signal_data.h'
  - 'src/table/object_land.h'
  - 'src/table/road_land.h'
  - 'src/table/autorail.h'
  - 'src/table/bridge_land.h'
  - 'src/saveload/labelmaps_sl.cpp'
  - 'src/saveload/waypoint_sl.cpp'
  - 'src/saveload/object_sl.cpp'
  - 'src/saveload/airport_sl.cpp'
  - 'src/saveload/animated_tile_sl.cpp'
  - 'docs/elrail*'
---

# Rail, road and infrastructure

Read at f8b1d52e80.

## Purpose and ownership

Owns what companies build on tiles for transport: rail and road type tables (base and NewGRF),
the track/trackdir/direction/slope vocabulary, rail and road construction commands, signals and
the signal-block updater, path-based signalling (PBS) reservations, waypoints and buoys, the
airport finite state machines and layouts, objects (transmitter, lighthouse, statue, owned land,
HQ, NewGRF objects), tile-area iterators, animated tiles, autoslope, and the construction
toolbars that post these commands. The tile storage itself is the
[map and tiles](../map-and-tiles/README.md) map; stations and road stops are
[towns, industries, stations](../towns-industries-stations/README.md); trains, aircraft and YAPF are
[vehicles and pathfinding](../vehicles-and-pathfinding/README.md); NewGRF railtype/roadtype/airport/
object callbacks are [NewGRF](../newgrf/README.md); the command protocol is
[commands](../commands/README.md). No fork hunk in the tracked files (checked with
`git log --oneline upstream/master..HEAD` over them).

## Entry points

- Types: `src/rail_type.h:RailType` (64 slots), `src/road_type.h:RoadType` (63; `ROADTYPE_END`
  equals `INVALID_ROADTYPE`), `src/road_type.h:RoadBits`, `src/track_type.h:Trackdir`,
  `src/track_type.h:TrackStatus`, `src/slope_type.h:Foundation`, `src/signal_type.h:SignalType`.
- Type tables: `src/rail.h:RailTypeInfo` via `src/rail.h:GetRailTypeInfo` over
  `src/rail_cmd.cpp:_railtypes`, seeded from `src/table/railtypes.h:_original_railtypes`;
  `src/road.h:RoadTypeInfo` via `src/road.h:GetRoadTypeInfo`, seeded from
  `src/table/roadtypes.h:_original_roadtypes`. Lookup by label: `src/rail.cpp:GetRailTypeByLabel`,
  `src/road.cpp:GetRoadTypeByLabel`.
- Rail commands (`src/rail_cmd.h`): `src/rail_cmd.cpp:CmdBuildSingleRail`,
  `src/rail_cmd.cpp:CmdBuildRailroadTrack` (`BuildRailLong`, a `BuildRail` loop in
  `src/rail_cmd.cpp:CmdRailTrackHelper`), `src/rail_cmd.cpp:CmdBuildSingleSignal`,
  `src/rail_cmd.cpp:CmdConvertRail`; depots, removal and signal drags beside them.
- Road commands (`src/road_cmd.h`): `src/road_cmd.cpp:CmdBuildRoad`,
  `src/road_cmd.cpp:CmdBuildLongRoad`, `src/road_cmd.cpp:CmdRemoveLongRoad`,
  `src/road_cmd.cpp:CmdBuildRoadDepot`, `src/road_cmd.cpp:CmdConvertRoad`; town-road removal policy in
  `src/road_cmd.cpp:CheckAllowRemoveRoad` (declared in `src/road_internal.h`).
- Waypoints and buoys (`src/waypoint_cmd.h`): `src/waypoint_cmd.cpp:CmdBuildRailWaypoint`,
  `src/waypoint_cmd.cpp:CmdBuildRoadWaypoint`, `src/waypoint_cmd.cpp:CmdBuildBuoy`;
  `src/waypoint_base.h:Waypoint` is a `src/base_station_base.h:SpecializedStation`.
- Airports: `src/airport.h:AirportFTAClass` built by `src/airport.cpp:AirportBuildAutomata` from
  `src/table/airport_movement.h:_airport_fta_country` and siblings; `src/airport.cpp:GetAirport`;
  built by `src/station_cmd.cpp:CmdBuildAirport`; driven by `src/aircraft_cmd.cpp:AirportMove`.
- Objects: `src/object_cmd.cpp:CmdBuildObject`, `src/object_cmd.cpp:BuildObject`,
  `src/object_cmd.cpp:GenerateObjects`, `src/object_cmd.cpp:UpdateCompanyHQ`; pool
  `src/object_base.h:Object`; statues only via `src/town_cmd.cpp:TownActionBuildStatue`.
- Signals and PBS: `src/signal.cpp:AddTrackToSignalBuffer`, `src/signal.cpp:UpdateSignalsInBuffer`,
  `src/pbs.cpp:TryReserveRailTrack`, `src/pbs.cpp:FollowTrainReservation`,
  `src/pbs.cpp:IsSafeWaitingPosition`; train side `src/train_cmd.cpp:TryPathReserve`.
- Tile areas: `src/tilearea_orthogonal.h:OrthogonalTileArea` (alias `TileArea`),
  `src/tilearea_diagonal.h:DiagonalTileArea`, `src/tilearea_spiral.h:SpiralTileSequence`,
  `src/tilearea_bitmap.h:BitmapTileArea`, `src/tilearea_airport.h:AirportTileIterator`.
- GUIs posting the commands: `src/rail_gui.cpp:DoRailroadTrack`,
  `src/road_gui.cpp:BuildRoadToolbarWindow`, plus the airport, dock, terraform, tree, object and
  bridge windows (`git grep -n '::Post' -- src/*_gui.cpp` lists them).
- **[fork]** Callers: the PrismaticAI bot (a Squirrel AI in the external bridge repository, see
  `research/mcp-control.md`) lays track with `src/script/api/script_rail.cpp:ScriptRail::BuildRail`
  (`BuildRailLong`, `fail_on_obstacle` true) plus `BuildRailStation`/`BuildRailDepot`, and roads
  through `src/script/api/script_road.cpp:ScriptRoad::_BuildRoadInternal` (`BuildRoadLong`, `is_ai`
  true). The human-like player `tools/research/mcp-human/src/play.rs` drags the road toolbar, so the
  game posts `BuildRoadLong` with `is_ai` false. The MCP server reads but never builds:
  `src/prismatic_mcp/prismatic_mcp_graph.cpp:RoadWays` and
  `src/prismatic_mcp/prismatic_mcp_graph.cpp:RailWays`; see the
  [PrismaticMCP map](../prismatic-mcp/README.md).

## Lifecycle

- **Type tables**: `src/newgrf.cpp:ResetNewGRFData` calls `src/rail_cmd.cpp:ResetRailTypes` and
  `src/road_cmd.cpp:ResetRoadTypes`; NewGRF Action 0 allocates slots with
  `src/rail_cmd.cpp:AllocateRailType` / `src/road_cmd.cpp:AllocateRoadType`; `AfterLoadGRFs` in
  `src/newgrf.cpp` calls `src/rail_cmd.cpp:InitRailTypes` (GUI sprites, hidden mask, sorted list).
- **Save/load of types**: the map stores a type *index*; the `RAIL` and `ROTT` chunks
  (`src/saveload/labelmaps_sl.cpp:RAILChunkHandler`) store the label per index, and
  `src/saveload/labelmaps_sl.cpp:AfterLoadLabelMaps` remaps every tile via
  `src/newgrf_railtype.cpp:ConvertRailTypes` / `src/newgrf_roadtype.cpp:ConvertRoadTypes`.
- **Build**: GUI or script -> `Command<Commands::BuildRailLong>` -> per tile `BuildRail` ->
  `LandscapeClear` / slope check -> on execute `MakeRailNormal` (or merge track bits), company
  infrastructure counters, `AddTrackToSignalBuffer`, `YapfNotifyTrackLayoutChange`.
- **Signals**: changes queue `(tile, side)` items in `_globset`; the buffer is flushed by
  `src/command.cpp:CommandHelperBase::InternalExecuteProcessResult`, by train movement and when it
  reaches `SIG_GLOB_UPDATE` items; `src/signal.cpp:ExploreSegment` walks each block and sets
  signal states.
- **Reservations**: trains reserve ahead via YAPF and `TryReserveRailTrack`; building a
  signal on reserved track frees the owning train's path and calls `TryPathReserve`
  again inside the command (`src/rail_cmd.cpp:CmdBuildSingleSignal`).
- **Animated tiles**: `src/animated_tile.cpp:AddAnimatedTile` appends to `_animated_tiles` (saved list,
  state bit in the map); `src/animated_tile.cpp:AnimateAnimatedTiles` ticks them, dropping `Deleted`.

## Invariants

- **[verified]** Reservations are game state in the map bytes: plain rail holds one reserved
  track or two non-overlapping parallel ones in m2 (`src/rail_map.h:GetRailReservationTrackBits`,
  `src/rail_map.h:SetTrackReservation` asserts `!TracksOverlap`); depots, crossings, station
  tiles and tunnel/bridge heads have a single bit each (`src/pbs.cpp:GetReservedTrackbits`). They
  are saved with the map planes; `TryReserveRailTrack` reads a client setting only to mark tiles dirty.
- **[verified]** One signal-update run serves one owner: `AddTrackToSignalBuffer` asserts
  `_globset.IsEmpty() || owner == _last_owner`; a command touching two companies' track must
  flush between them.
- **[verified]** Company infrastructure counters are a cache that every build/remove/convert path
  must update; overlapping track bits on one tile count squared (`CmdBuildSingleRail`), and
  `src/cachecheck.cpp:CheckCaches` recomputes them via
  `src/saveload/company_sl.cpp:AfterLoadCompanyStats` and logs a mismatch under `-d desync=2`.
- **[verified]** A saved type label that no loaded NewGRF provides loads as `RAILTYPE_RAIL`, and a
  road/tram label as `ROADTYPE_ROAD`/`ROADTYPE_TRAM` (`ConvertRailTypes`, `ConvertRoadTypes`); a new
  NewGRF type from `AllocateRailType` copies `RAILTYPE_RAIL`, compatible and powered only with itself.
- **[verified]** Disabling electric rail rewrites engine and train rail types, not tiles
  (`src/elrail.cpp:UpdateDisableElrailSettingState`); `src/openttd.cpp:MakeNewGameDone` applies it on
  every new game.
- **[verified]** Autoslope applies only to companies, or to `OWNER_NONE` in the scenario editor
  (`src/autoslope.h:AutoslopeEnabled`); towns and deity do not get it.
- **[verified]** Waypoints and buoys share the station pool and `StationID`; a removed waypoint
  lingers unused and is reused by a new one of the same owner and kind within 8 tiles
  (`src/waypoint_cmd.cpp:FindDeletedWaypointCloseTo`).
- **[verified]** Airport movement is a fixed per-type FSM; NewGRF airports reuse one through
  `src/newgrf_airport.h:AirportSpec` (`fsm`). Occupancy is `src/station_base.h:Airport` `blocks`, a
  saved bitset.
- **[verified]** `CmdBuildObject` refuses statues (`OBJECT_STATUE`), refuses everything before the
  first town exists, and moves an existing HQ by clearing the old one as `OWNER_WATER`.
- **[supported]** `TransportType` values are tied to tunnel/bridge map bits (`src/transport_type.h`).

## Extension points

- New base rail/road type: append to `src/table/railtypes.h` / `src/table/roadtypes.h` and give it
  a label; the label, not the index, is what survives savegames and NewGRF changes.
- New signal type: `src/signal_type.h:SignalType`, the GUI sprite lookup in
  `src/rail_cmd.cpp:ResolveRailTypeGUISprites`, the signal block rules in `src/signal.cpp`, and the
  map bits (`docs/landscape.html`) behind a new `SaveLoadVersion` (ADR 0018).
- New airport: an FTA table in `src/table/airport_movement.h`, a layout in
  `src/table/airport_defaults.h`, a line in `src/airport.cpp`, a new value in `src/airport.h:AirportTypes`.
- New object type: usually a NewGRF object (`NEW_OBJECT_OFFSET` and up); base types live in
  `src/object_type.h` and `src/table/object_land.h:_objects`.

## Traps and forbidden operations

- A dragged build succeeds if any piece was built: `CmdRailTrackHelper` (with `fail_on_obstacle`
  false, the GUI value) and `CmdBuildLongRoad` (with `is_ai` false, the GUI value) stop at the first
  obstacle after a success and return the partial cost. Verify the whole run; scripts pass true
  (knowledge `a-dragged-build-succeeds-when-any-piece-was-built`).
- The Doxygen of `CmdBuildRailroadTrack` swaps the descriptions of `auto_remove_signals` and
  `fail_on_obstacle`; trust the code in `CmdRailTrackHelper`. `end_tile` comes before `start_tile`.
- `CmdBuildSingleSignal` and track changes run YAPF (`TryPathReserve`) inside the execute pass;
  anything they read must be deterministic game state (`project.game-state-mutates-only-through-commands`).
- `ROADTYPE_LABEL_TRAM` is `"ELRL"`, the same four bytes as the electric rail label; labels are
  per kind, never compare across rail and road.
- `AirportBlock` reuses bits: `RunwayIn`, `RunwayInOut` and `AirportBusy` are one bit and
  `RunwayOut2` equals `TaxiwayBusy`; their meaning depends on the airport type.
- **[inferred]** `GenerateObjects` calls `CmdBuildObject` and `BuildObject` directly; that is safe
  only because peers receive the generated map. During play it would bypass commands (ADR 0016).

## Tests and validation

- Unit: `src/tests/tilearea.cpp` (area iteration) only; no Catch2 test for rail, road, signals.
- Regression: `regression/regression/main.nut` sections `Rail`, `RailTypeList`, `Road`, `Airport`,
  `Bridge`, `Tunnel`, `Marine`, `Station` (ctest `regression_regression`, needs a base set).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R regression_` while iterating.
- Debug: `-d desync=2` checks infrastructure and other caches every tick; `-d yapf=N` for paths.

## Decisions, rules and knowledge

- ADR 0016 (lockstep), ADR 0017 (pools: `Object`, `Waypoint`), ADR 0018 (version-gated fields),
  ADR 0023 (tile dispatch through `_tile_type_rail_procs` and siblings), ADR 0026 (YAPF reserves
  paths), ADR 0008 (MCP reads, never builds), ADR 0007 (the bot steers a company).
- Rules: `project.game-state-mutates-only-through-commands`,
  `project.savegame-fields-are-version-gated`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `road-stops-and-depots-need-their-entrance-road`,
  `a-dragged-build-succeeds-when-any-piece-was-built`, `a-missing-type-label-loads-as-the-base-type`.
- Upstream docs: `docs/landscape.html` (rail, road, object bits), `docs/elrail.svg`.

## Typical tasks

- To change track building: start at `src/rail_cmd.cpp:CmdBuildSingleRail`, keep infrastructure
  counts and `AddTrackToSignalBuffer`/`YapfNotifyTrackLayoutChange` on every path; validate with
  the `Rail` regression section and `-d desync=2`.
- To change road building or town-road removal: start at `src/road_cmd.cpp:CmdBuildRoad` and
  `src/road_cmd.cpp:CheckAllowRemoveRoad`; validate with the `Road` regression section.
- To change signal or PBS behaviour: start at `src/signal.cpp:ExploreSegment` and
  `src/pbs.cpp:TryReserveRailTrack`; run a saved network of trains under `-d desync=2`.
- To let research code build infrastructure: post `BuildRailLong`/`BuildRoadLong` as a company
  through `Command<>::Post` or a script; never write tiles directly (ADR 0008).
