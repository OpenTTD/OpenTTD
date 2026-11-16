---
schema: context/v1
id: fork.substrate.consists-groups-and-timetables
kind: context
title: "Consists, groups and timetables: front-vehicle state, vehicle groups and their statistics, timetables, the build-vehicle window, disasters, effect vehicles and the original engine tables"
description: "Read before changing BaseConsist fields, a group or timetable command, group statistics, the build-vehicle window, a disaster or an effect vehicle, or the original engine/sprite/road-drive tables."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/base_consist.*'
  - 'src/group*'
  - 'src/timetable*'
  - 'src/build_vehicle_gui.cpp'
  - 'src/disaster_vehicle.*'
  - 'src/effectvehicle*'
  - 'src/table/engines.h'
  - 'src/table/train_sprites.h'
  - 'src/table/roadveh_movement.h'
---

# Consists, groups and timetables

Read at f8b1d52e80.

## Purpose and ownership

Vehicle-adjacent code the [vehicles and pathfinding](../vehicles-and-pathfinding/README.md) map
does not track: the per-consist state every front vehicle carries (`BaseConsist`), company vehicle
groups and their NOSAVE statistics, timetables (commands and autofill), the purchase window and its
engine list, the two non-company vehicle types (disasters, effect vehicles), and the original TTD
engine, train-sprite and road-drive tables. All upstream; no fork hunk in any tracked file
(`git log --oneline upstream/master..HEAD -- src/group* src/timetable* src/base_consist.*` is empty)
and nothing under `src/prismatic_mcp/` reads groups, timetables or disasters.

## Entry points

- `src/base_consist.h:BaseConsist` - name, timetable counters (`current_order_time`,
  `lateness_counter`, `timetable_start`), depot-unbunching ticks, `service_interval`, current order
  indices and `src/base_consist.h:VehicleFlag` bits; a base of `src/vehicle_base.h:Vehicle`.
- `src/base_consist.cpp:BaseConsist::CopyConsistPropertiesFrom`, `ResetDepotUnbunching`.
- Groups: `src/group.h:Group` (pool `src/group.h:GroupPool`, sentinels in
  `src/group_type.h:ALL_GROUP`, `DEFAULT_GROUP`, `NEW_GROUP`), `src/group.h:GroupStatistics`.
  Commands in `src/group_cmd.cpp`: `CmdCreateGroup`, `CmdDeleteGroup`, `CmdAlterGroup`,
  `CmdAddVehicleGroup`, `CmdAddSharedVehicleGroup`, `CmdRemoveAllVehiclesGroup`, `CmdSetGroupFlag`,
  `CmdSetGroupLivery` (traits in `src/group_cmd.h`). Window: `src/group_gui.cpp:VehicleGroupWindow`.
- Timetables: `src/timetable_cmd.cpp:CmdChangeTimetable`, `CmdBulkChangeTimetable`,
  `CmdSetVehicleOnTime`, `CmdSetTimetableStart`, `CmdAutofillTimetable`; per-arrival update
  `src/timetable_cmd.cpp:UpdateVehicleTimetable` (called from `src/order_cmd.cpp:ProcessOrders`
  and `src/ship_cmd.cpp`); window `src/timetable_gui.cpp:TimetableWindow`.
- Purchase list: `src/build_vehicle_gui.cpp:BuildVehicleWindow`, `ShowBuildVehicleWindow`,
  `DrawEngineList`, `DrawVehiclePurchaseInfo`, sorters in `_engine_sort_functions`.
- Disasters: `src/disaster_vehicle.h:DisasterVehicle`, `src/disaster_vehicle.cpp:_disasters`,
  `DoDisaster`, `_economy_disaster_daily`, `StartupDisasters`, `ReleaseDisastersTargetingIndustry`,
  `ReleaseDisasterVehicle`.
- Effects: `src/effectvehicle_base.h:EffectVehicle`, `src/effectvehicle_func.h:EffectVehicleType`,
  `src/effectvehicle.cpp:CreateEffectVehicle` (`CreateEffectVehicleAbove`,
  `CreateEffectVehicleRel`), per-type procs `_effect_procs`.
- Tables: `src/table/engines.h:_orig_engine_info`, `_orig_rail_vehicle_info`,
  `_orig_road_vehicle_info`, `_orig_ship_vehicle_info`, `_orig_aircraft_vehicle_info` (read by
  `src/engine.cpp:Engine::Engine` and the old loader); `src/table/train_sprites.h:_engine_sprite_base`
  (included by `src/train_cmd.cpp`); `src/table/roadveh_movement.h:_road_drive_data` (included
  mid-file by `src/roadveh_cmd.cpp`, after `RoadDriveEntry`).

## Lifecycle

- Groups: created by command; `Company::freegroups` hands out the per-company `number`. Saved in
  chunk `GRPS` (`src/saveload/group_sl.cpp`: name, owner, type, flags, livery, parent, number).
  `children`, `folded` and `statistics` are NOSAVE: `src/group_cmd.cpp:UpdateGroupChildren`
  (called from `src/saveload/afterload.cpp:AfterLoadGame`) rebuilds children and drops an invalid
  parent; `src/group_cmd.cpp:GroupStatistics::UpdateAfterLoad` recounts vehicles and engines after
  load and in `src/saveload/afterload.cpp:ReloadNewGRFData`.
- Statistics move incrementally: `CountVehicle`/`CountEngine` (+1/-1) from build
  (`src/vehicle_cmd.cpp:CmdBuildVehicle`), delete (`src/vehicle.cpp:Vehicle::PreDestructor`),
  train re-arrangement (`src/train_cmd.cpp:CmdMoveRailVehicle`), company merge
  (`src/economy.cpp:ChangeOwnershipOfCompanyItems`); `UpdateProfits` runs from the yearly
  `src/vehicle.cpp:_economy_vehicles_yearly`; `UpdateAutoreplace` after any group or renew change.
- Timetable: the vehicle controllers increment `current_order_time` per tick
  (`src/train_cmd.cpp:Train::Tick`); on each order change `UpdateVehicleTimetable` sets
  `TimetableStarted` at the first manual order, autofills travel/wait times (rounded up to whole
  seconds, minimum one tick), clears autofill after one full round and accumulates
  `lateness_counter`, wrapping it modulo the full timetable duration.
- Time domains (ADR 0025): timetable counters are `TimerGameTick` ticks; `timetable_start` is an
  absolute `TimerGameTick::counter` value; dates for the GUI and the start-date limits are
  `TimerGameEconomy` (`src/timetable_cmd.cpp:GetStartTickFromDate`, `GetDateFromStartTick`,
  `src/timetable.h:MAX_TIMETABLE_START_YEARS`). Disasters tick on an economy day
  (`TimerGameEconomy::Priority::Disaster`) but pick by `TimerGameCalendar::year` windows.
- Disasters: `StartupDisasters` (from `src/genworld.cpp`) seeds `_disaster_delay`, saved as
  `next_disaster_start` in `src/saveload/misc_sl.cpp`. Each economy day it counts down; at zero it
  re-randomises and, when `difficulty.disasters` is on, runs one init proc from `_disasters`.
- Effect vehicles: created by industries, vehicle smoke/sparks (`src/vehicle.cpp:Vehicle::ShowVisualEffect`),
  breakdowns, crashes and disasters; ticked by `src/vehicle.cpp:CallVehicleTicks` like every
  vehicle and deleted by their own tick proc.

## Invariants

- **[verified]** `CopyConsistPropertiesFrom` copies name, timetable counters, service interval,
  order indices and the timetable/service flags, but not the depot-unbunching ticks or
  `round_trip_time`; it is how `src/order_backup.cpp:OrderBackup` and
  `src/vehicle_base.h:Vehicle::CopyVehicleConfigAndStatistics` (autoreplace, train re-heading)
  carry consist state across a vehicle swap.
- **[verified]** A timetable edit invalidates depot unbunching for the whole shared-order chain
  (`ResetDepotUnbunching` in `src/timetable_cmd.cpp:CmdChangeTimetable`); order, depot and
  start/stop paths do the same (`git grep -n ResetDepotUnbunching -- src`).
- **[verified]** Group statistics are NOSAVE yet feed game logic: `src/engine.cpp` blocks future
  exclusive previews when `GetGroupNumEngines(..., ALL_GROUP, ...)` is 0, and
  `src/script/api/script_engine.cpp:ScriptEngine::IsBuildable` reads `group_all`. They are not in
  `src/cachecheck.cpp:CheckCaches`, so a missed `CountEngine` diverges silently.
- **[verified]** `GroupStatistics::Get` asserts that a real group's owner and vehicle type match
  the caller's; `ALL_GROUP`/`DEFAULT_GROUP` live on the company (`group_all`, `group_default`).
- **[verified]** Parent loops are prevented only in `CmdAlterGroup` (`GroupIsInGroup` check, per
  its own comment); `CmdCreateGroup` cannot form one because a new group has no children.
- **[verified]** Only one vehicle of a shared-order chain autofills: `CmdAutofillTimetable` clears
  `AutofillTimetable` on every other shared vehicle.
- **[verified]** Effect vehicles are lockstep game state: they occupy `VehiclePool` slots, draw
  from the game `Random()` in their init/tick procs, are saved in `VEHS`
  (`src/saveload/vehicle_sl.cpp`), and spawn only under game settings (`vehicle.smoke_amount`).
  Only drawing (`GetTransparencyOption`) and sound (`_settings_client.sound.ambient`) are client
  side. They never enter the tile hash (`src/vehicle.cpp:Vehicle::UpdatePositionAndViewport`
  skips `UpdatePosition`), so they never block construction.
- **[verified]** Disaster vehicles are in the tile hash but `src/vehicle.cpp:EnsureNoVehicleOnGround`
  skips them; their tile clearing runs `Command<Commands::LandscapeClear>::Do` from the tick as
  `OWNER_WATER`/`OWNER_NONE` (`src/disaster_vehicle.cpp:DisasterClearSquare`).
- **[verified]** `src/engine.cpp` `static_assert`s that the four per-type `_orig_*_info` tables
  sum to `_orig_engine_info`; the order (rail, road, ship, aircraft) fixes original `EngineID`s.
- **[verified]** `src/roadveh.h:RoadVehicle` `disaster_vehicle` is NOSAVE; it is rebuilt from
  the UFO's `dest_tile` in `src/saveload/vehicle_sl.cpp:AfterLoadVehiclesPhase2`, which deletes a
  second UFO targeting the same vehicle.

## Extension points

- New consist-level field: add it to `BaseConsist`, decide whether `CopyConsistPropertiesFrom`
  carries it, save it version-gated in `src/saveload/vehicle_sl.cpp` and, if it must survive
  sell-and-rebuild, in the order-backup table of `src/saveload/order_sl.cpp`.
- New group property: a field on `Group`, a `GRPS` entry with a new `SaveLoadVersion`, a mode in
  `src/group_cmd.h:AlterGroupMode` or a new command, a `ScriptGroup` wrapper if scripts need it.
- New disaster: an init proc plus a row in `_disasters`, a `DisasterSubType` (append; the subtype
  is saved) and a tick proc in `_disastervehicle_tick_procs`, both indexed by subtype.
- New effect: append to `EffectVehicleType` before `EV_END` (saved as subtype) and add a row to
  `_effect_procs` (a `std::array` sized `EV_END`).
- New purchase-list sort: a sorter plus a string in both `_engine_sort_functions` and
  `_engine_sort_listing` for that vehicle type; indices are remembered in `_engine_sort_last_criteria`.

## Traps and forbidden operations

- `BuildVehicleWindow::SelectEngine` calls `Command<Commands::BuildVehicle>::Do` with
  `DoCommandFlag::QueryCost` from the GUI. With a cargo filter that forces a refit this builds and
  sells a real vehicle on the local client only (knowledge
  `the-purchase-window-cost-query-builds-a-real-vehicle`).
- A disaster's `dest_tile` is not always a tile: airplanes/helicopters keep an `IndustryID`, the
  small UFO a `VehicleID` (knowledge `a-disaster-dest-tile-can-hold-an-industry-or-vehicle-id`).
  `age` is reused as a step counter reset to `CalendarTime::MIN_DATE`.
- `_disaster_delay` draws `Random()` every time it expires, even with disasters off; skipping the
  draw when disabled would change the random stream.
- `CmdSetVehicleOnTime` with `apply_to_group` tests `v->vehicle_flags` (the clicked vehicle)
  rather than each `u` inside both loops, and `CmdSetTimetableStart` resets unbunching on `v`
  rather than each `w`; read them as written before reasoning about per-vehicle state.
- `CmdAddVehicleGroup` with `NEW_GROUP` calls `CmdCreateGroup` directly (not through
  `Command<>::Do`); its test pass returns `GroupID::Invalid()`.
- Timetable values are `uint16_t` ticks; the GUI clamps (`ClampTo<uint16_t>`), a script or MCP
  caller must too. `TimetableWindow` forces the client setting `gui.timetable_mode` to seconds when
  `TimerGameEconomy::UsingWallclockUnits` is on.
- `Engine::display_last_variant` and `Group::folded` are NOSAVE client-side fields that GUI code
  writes directly; this is the only sanctioned direct GUI write in this area.

## Tests and validation

- No Catch2 test in `src/tests/` covers groups, timetables, disasters or effect vehicles.
  `regression/regression/main.nut` exercises groups (`Regression::Group`) and vehicles; it runs as
  `regression_regression` (`ctest -R regression_regression`).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R regression_` while iterating. Without a
  base set the regression cases are not run (knowledge `macos-missing-baseset-hangs-headless-runs`).
- Debug: `-d misc=2` prints `UpdateGroupChildren`'s invalid-parent warning; `-d desync=2` runs
  `CheckCaches` but, as above, does not check group statistics.

## Decisions, rules and knowledge

- ADR 0016 (lockstep: disasters and effect vehicles are simulated identically on every peer),
  ADR 0017 (groups and vehicles in typed pools; `GroupID` sentinels sit above the pool limit),
  ADR 0018 (`GRPS` fields and subtypes are saved), ADR 0025 (tick/economy/calendar split above),
  ADR 0021 (`ScriptGroup` and `ScriptEventDisaster*`).
- Rules: `project.game-state-mutates-only-through-commands`,
  `project.savegame-fields-are-version-gated`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `effect-vehicles-are-game-state`, `the-purchase-window-cost-query-builds-a-real-vehicle`,
  `a-disaster-dest-tile-can-hold-an-industry-or-vehicle-id`, `a-command-test-pass-must-not-draw-random`.
- Sibling maps: [commands](../commands/README.md), [core loop](../core-loop/README.md) (timers),
  [saveload](../saveload/README.md), [scripting](../scripting/README.md),
  [GUI and console](../gui-and-console/README.md).

## Typical tasks

- To change how a vehicle is counted in a group: start at `src/group_cmd.cpp:GroupStatistics::CountEngine`
  and every caller (`git grep -n 'GroupStatistics::' -- src`), keep `UpdateAfterLoad` equal to the
  incremental path, validate with `regression_regression`.
- To change timetable autofill or lateness: start at `src/timetable_cmd.cpp:UpdateVehicleTimetable`,
  keep it inside the tick path (deterministic), validate with `tools/gate.sh change`.
- To add a disaster or effect: append the subtype, add the proc row, version-gate any new saved
  field, validate with `tools/gate.sh change` and a long run with `-d desync=2`.
- To change the purchase list: start at `src/build_vehicle_gui.cpp:BuildVehicleWindow::GenerateBuildList`;
  it is client-only, post purchases through `Command<Commands::BuildVehicle>::Post`.
