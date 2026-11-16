---
schema: context/v1
id: fork.substrate.vehicles-and-pathfinding
kind: context
title: "Vehicles and pathfinding: the vehicle pool, per-type controllers, orders, depots, autoreplace, engines and YAPF"
description: "When a change reads or moves vehicles, edits orders/engines/autoreplace, or touches a pathfinder or its caches."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/vehicle*'
  - 'src/ground_vehicle.*'
  - 'src/articulated_vehicles.*'
  - 'src/train*'
  - 'src/roadveh*'
  - 'src/ship*'
  - 'src/aircraft*'
  - 'src/order*'
  - 'src/depot*'
  - 'src/autoreplace*'
  - 'src/engine*'
  - 'src/pathfinder/**'
---

## Purpose and ownership

Owns every company-ownable vehicle (`src/vehicle_type.h:VehicleType`: Train, Road, Ship, Aircraft;
Effect and Disaster are non-company types), their per-tick movement, order processing, servicing,
autoreplace/autorenew, the engine catalogue, and the pathfinders that pick a track at each junction
(YAPF for rail/road/ship, water regions for ship routing). All of it is upstream code; vehicle
state is part of the lockstep-simulated game state.

## Entry points

- `src/vehicle_base.h:Vehicle` - pool item (`src/vehicle_base.h:VehiclePool`); virtual `Tick`, `OnNewCalendarDay`, `OnNewEconomyDay`, `FindClosestDepot` are overridden per type.
- `src/vehicle.cpp:CallVehicleTicks` - called once per tick from `src/openttd.cpp:StateGameLoop`; ticks every vehicle, then runs queued autoreplaces.
- `src/vehicle_cmd.cpp:CmdBuildVehicle`, `CmdSellVehicle`, `CmdRefitVehicle`, `CmdStartStopVehicle`, `CmdCloneVehicle`, `CmdSendVehicleToDepot` - player/script-facing commands; traits in `src/vehicle_cmd.h`.
- Per-type controllers: `src/train_cmd.cpp:TrainLocoHandler` / `TrainController`, `src/roadveh_cmd.cpp:RoadVehController`, `src/ship_cmd.cpp:ShipController`, `src/aircraft_cmd.cpp:AircraftEventHandler`.
- `src/order_cmd.cpp:ProcessOrders` / `UpdateOrderDest` - per-tick order advance; `CmdInsertOrder`, `CmdDeleteOrder`, `CmdModifyOrder` are the commands (`src/order_cmd.h`).
- `src/order_base.h:OrderList` - pool item holding a `std::vector<Order>` shared by a chain of vehicles.
- `src/order_backup.h:OrderBackup` - per-client backup of orders when a vehicle is sold in a depot, restored by the next build there.
- `src/autoreplace_cmd.cpp:CmdAutoreplaceVehicle`; rules in `src/autoreplace.cpp:EngineReplacement`.
- `src/engine.cpp:StartupEngines`, `CalendarEnginesMonthlyLoop`; `src/engine_base.h:EngineOverrideManager` maps GRF engine ids to `EngineID`.
- Pathfinder API: `src/pathfinder/yapf/yapf.h:YapfTrainChooseTrack`, `YapfRoadVehicleChooseTrack`, `YapfShipChooseTrack`, `YapfTrainFindNearestDepot`, `YapfTrainFindNearestSafeTile`.
- **[fork]** Read-only consumers only: `src/prismatic_mcp/prismatic_mcp_view.cpp` (viewport follow/scroll to a vehicle) and `src/console_cmds.cpp:ConResearchStatus` (vehicle count). No fork hunk in any tracked file.

## Lifecycle

- Init: `src/vehicle.cpp:InitializeVehicles` clears the autoreplace queue and the vehicle hashes; `src/engine.cpp:SetupEngines` / `StartupEngines` build the engine list.
- Per tick (`src/openttd.cpp:StateGameLoop`, company `OWNER_NONE`): `RunVehicleCalendarDayProc` on a calendar-day change, timers, `RunTileLoop`, then `CallVehicleTicks`. `RunEconomyVehicleDayProc` (inside `CallVehicleTicks`) visits one `DAY_TICKS` slice of the pool per tick, so each vehicle gets its day proc once per day at `index % DAY_TICKS`.
- Day procs (e.g. `src/train_cmd.cpp:Train::OnNewEconomyDay`) run breakdowns, `CheckIfTrainNeedsService`, `src/order_cmd.cpp:CheckOrders`, running costs.
- Monthly/daily engine work is registered as `IntervalTimer` objects in `src/engine.cpp` (`_calendar_engines_monthly`); the yearly vehicle pass is `src/vehicle.cpp:_economy_vehicles_yearly`.
- Depot arrival: `src/vehicle.cpp:VehicleEnterDepot` -> `VehicleEnteredDepotThisTick` queues the vehicle in `_vehicles_to_autoreplace`; `CallVehicleTicks` drains the queue after all ticks.
- Save/load: chunks `VEHS` (`src/saveload/vehicle_sl.cpp`), `ORDL`/`BKOR` (`src/saveload/order_sl.cpp`), `ENGN`/`EIDS` (`src/saveload/engine_sl.cpp`), `ERNW`, `DEPT`. `src/saveload/vehicle_sl.cpp:AfterLoadVehiclesPhase1` rebuilds the NOSAVE chain pointers; `src/saveload/afterload.cpp:ReloadNewGRFData` reruns both phases after a GRF change.

## Invariants

- **[verified]** `Vehicle::Tick` returns `false` exactly when it deleted the vehicle; `src/vehicle.cpp:CallVehicleTicks` asserts the pool slot is empty in that case and that it still holds `v` otherwise.
- **[verified]** The tick loop mutates vehicle state directly (orders, `dest_tile`, depot visits in `src/train_cmd.cpp:CheckIfTrainNeedsService`); this is lockstep-safe because every client runs it. Mutations originating outside the loop go through commands, and the loop itself issues `Command<Commands::AutoreplaceVehicle>::Do` with `_current_company` set to the owner (`src/vehicle.cpp:CallVehicleTicks`).
- **[verified]** A vehicle entering a depot is always stopped first, so it cannot reserve a path out before autoreplace swaps its engine (`src/vehicle.cpp:VehicleEnteredDepotThisTick`).
- **[verified]** `src/vehicle_cmd.cpp:CmdBuildVehicle` with a refit in a test pass really builds the vehicle (subflags gain `Execute`+`AutoReplace`), sells it again, and brackets this with `SaveRandomSeeds`/`RestoreRandomSeeds` to avoid desyncs.
- **[verified]** Conditional orders are bounded: `src/order_cmd.cpp:UpdateOrderDest` aborts when `conditional_depth > GetNumOrders()`; with `pbs_look_ahead` it must not modify order indices.
- **[verified]** The rail segment cost cache is process-global and NOT saved; it is flushed whenever `src/pathfinder/yapf/yapf_rail.cpp:YapfNotifyTrackLayoutChange` bumps `src/pathfinder/yapf/yapf_costcache.hpp:CSegmentCostCacheBase` (`CYapfSegmentCostCacheGlobalT::stGetGlobalCache`). Afterload also flushes it.
- **[verified]** Road vehicle and ship path caches (`src/roadveh.h:RoadVehPathCache`, `src/ship.h:ShipPathCache`) ARE saved (`src/saveload/vehicle_sl.cpp`), so they are game state and must be cleared deterministically.
- **[verified]** Water region data is lazily rebuilt; any change to water tiles must call `src/pathfinder/water_regions.cpp:InvalidateWaterRegion` (it also invalidates the neighbour region when an edge tile changes).
- **[verified]** Pathfinders read only game settings (`_settings_game.pf`, `src/settings_type.h:YAPFSettings`), never client settings; the node limit is `max_search_nodes` in `src/pathfinder/yapf/yapf_base.hpp:CYapfBaseT::FindPath`.
- **[supported]** Cache mismatches are "the usual case" of NewGRF-related desyncs (`docs/desync.md`, section 2.1); vehicle caches are recomputed and compared by `src/cachecheck.cpp:CheckCaches`.

## Extension points

- New vehicle command: declare `DEF_CMD_TRAIT` in the subsystem `*_cmd.h` (e.g. `src/vehicle_cmd.h`), add the enum value in `src/command_type.h`, implement `Cmd...` in the `*_cmd.cpp`.
- New per-type behaviour: override the virtuals on the type struct (`src/train.h`, `src/roadveh.h`, `src/ship.h`, `src/aircraft.h`) via `src/vehicle_base.h:SpecializedVehicle`.
- New periodic engine/vehicle work: add an `IntervalTimer<TimerGameCalendar|TimerGameEconomy>` like `src/engine.cpp:_calendar_engines_monthly`.
- New YAPF variant: compose a `Types` struct (`src/pathfinder/yapf/yapf_rail.cpp:CYapfRail_TypesT`) with a follower (`src/pathfinder/follow_track.hpp:CFollowTrackT`), cost and destination policies; register settings in `src/table/settings/pathfinding_settings.ini`.
- A command that changes rail layout must call `YapfNotifyTrackLayoutChange` (current callers: `src/rail_cmd.cpp`, `src/road_cmd.cpp`, `src/station_cmd.cpp`, `src/tunnelbridge_cmd.cpp`, `src/waypoint_cmd.cpp`).

## Traps and forbidden operations

- Writing a vehicle/order field from GUI, MCP or console code: forbidden by `project.game-state-mutates-only-through-commands`; use the existing command.
- Iterating vehicles while deleting: only `Tick` may delete its own vehicle, and the caller must not touch `v` afterwards (`src/vehicle.cpp:CallVehicleTicks`).
- Adding a cache field to `Vehicle`/`GroundVehicleCache` without extending `src/cachecheck.cpp:CheckCaches` leaves desyncs invisible to `-d desync=2`.
- Changing track/water topology without the cache notifications above gives stale paths on the host and fresh ones on a newly joined client.
- `CmdCloneVehicle` is flagged `CommandFlag::NoTest` because NewGRF callbacks make its test pass unreliable (`src/vehicle_cmd.h`); do not rely on its estimate.
- The `test_yapf.h` file named in `src/pathfinder/yapf/yapf_base.hpp` comments does not exist in this tree.

## Tests and validation

- No unit tests in `src/tests/` cover vehicles or YAPF. Behavioural coverage is `regression/regression/main.nut` (`Regression::Vehicle`, `Regression::Order`, `Regression::Engine`, run for 30000 ticks), compared line by line with `regression/regression/result.txt`.
- Tier: `tools/gate.sh change` (full ctest, including `regression_*`); `tools/gate.sh smoke -R regression_regression` for a quick loop. Without a base set the regression cases are NOT RUN and the gate exits PARTIAL (knowledge `macos-missing-baseset-hangs-headless-runs`).
- Debug: `-d yapf=3` (per-search line from `CYapfBaseT::FindPath`), `-d desync=2` (runs `CheckCaches` every tick and, for trains, pathfinds twice with and without the segment cache, logging `ChooseRailTrack cache mismatch` from `src/pathfinder/yapf/yapf_rail.cpp:CYapfFollowRailT::stChooseRailTrack`). Facility names: `src/debug.cpp`, enum `src/debug_type.h:Facility`.

## Decisions, rules and knowledge

- Rules: `project.game-state-mutates-only-through-commands`, `project.savegame-fields-are-version-gated`, `project.never-report-unrun-validation-as-pass`, `project.research-code-is-gated-and-labelled`.
- ADR 0004 (research access to internals), ADR 0008 (MCP reaches the game on the main thread).
- Knowledge: `road-stops-and-depots-need-their-entrance-road`, `macos-missing-baseset-hangs-headless-runs`.
- Upstream docs: `docs/desync.md`, `docs/debugging_desyncs.md`, `docs/savegame_format.md`.

## Typical tasks

- To trace why a train picks a track: start at `src/train_cmd.cpp:ChooseTrainTrack`, then `src/pathfinder/yapf/yapf_rail.cpp:YapfTrainChooseTrack`; run with `-d yapf=3`, validate with `tools/gate.sh change`.
- To add a vehicle command: start at `src/vehicle_cmd.h`, implement in `src/vehicle_cmd.cpp`, expose via the Script API (see the scripting context), validate with `tools/gate.sh change`.
- To add a cached vehicle property: add it to the cache struct, recompute in the type's `UpdateCache`/`ConsistChanged`, extend `src/cachecheck.cpp:CheckCaches`, then run a game with `-d desync=2`.
- To change order semantics: start at `src/order_cmd.cpp:ProcessOrders` and `UpdateOrderDest`, keep `src/saveload/order_sl.cpp` version-gated, validate with `regression_regression`.
- To instrument vehicle ticks for research: hook next to `src/vehicle.cpp:CallVehicleTicks` behind `OTTD_RESEARCH_INSTRUMENTATION`, validate with `tools/gate.sh research`.
