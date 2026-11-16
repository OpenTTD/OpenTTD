---
schema: context/v1
id: fork.substrate.towns-industries-stations
kind: context
title: "Towns, industries, stations and cargo: who produces, who holds and who moves cargo"
description: "Read before changing towns, industries, stations, road stops, cargo packets, cargo monitors or subsidies."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/town*'
  - 'src/house*'
  - 'src/industry*'
  - 'src/cargo*'
  - 'src/source_type.h'
  - 'src/station*'
  - 'src/base_station_base.h'
  - 'src/roadstop*'
  - 'src/subsidy*'
  - 'src/cachecheck.cpp'
  - 'src/saveload/town_sl.cpp'
  - 'src/saveload/industry_sl.cpp'
  - 'src/saveload/station_sl.cpp'
  - 'src/saveload/cargopacket_sl.cpp'
  - 'src/saveload/cargomonitor_sl.cpp'
  - 'src/saveload/subsidy_sl.cpp'
---

# Towns, industries, stations and cargo

## Purpose and ownership

Owns the pooled world objects that produce, hold and accept cargo: `Town` (and its houses),
`Industry`, `Station`/`Waypoint` (one shared `BaseStation` pool), `RoadStop`, `CargoPacket`,
`Subsidy`, and the cargo-monitor maps used by Game Scripts. Producers push cargo into stations;
vehicles load and unload it in `src/economy.cpp` (payment itself is the economy context).

## Entry points

- Pools: `src/town.h:TownPool`, `src/industry.h:IndustryPool`, `src/base_station_base.h:StationPool` (Station and Waypoint share it; `src/base_station_base.h:SpecializedStation::IsExpected` tells them apart), `src/roadstop_base.h:RoadStopPool`, `src/cargopacket.h:CargoPacketPool`, `src/subsidy_base.h:SubsidyPool`.
- Ticks from `src/landscape.cpp:CallLandscapeTick`: `src/town_cmd.cpp:OnTick_Town`, `src/station_cmd.cpp:OnTick_Station`, `src/industry_cmd.cpp:OnTick_Industry`.
- Tile loops (every 256 ticks per tile): `src/town_cmd.cpp:TileLoop_Town` (houses generate passengers/mail), `src/industry_cmd.cpp:TileLoop_Industry` (calls `TransportIndustryGoods`).
- Producer to station: `src/station_cmd.cpp:MoveGoodsToStation`, called by `src/town_cmd.cpp:TownGenerateCargo`, `src/industry_cmd.cpp:TransportIndustryGoods` and company headquarters in `src/object_cmd.cpp`.
- Station to vehicle and back: `src/economy.cpp:PrepareUnload` (from `src/vehicle.cpp:Vehicle::BeginLoading`), `src/economy.cpp:LoadUnloadStation` (per station per tick from `src/vehicle.cpp:CallVehicleTicks`), `src/economy.cpp:LoadUnloadVehicle`; the list operations are `src/cargopacket.cpp:VehicleCargoList::Stage`, `StationCargoList::Load`, `StationCargoList::Reserve`, `VehicleCargoList::Unload` with action functors in `src/cargoaction.h`.
- Delivery side effects: `src/economy.cpp:DeliverGoods` (town counters, `src/cargomonitor.cpp:AddCargoDelivery`, `src/subsidy.cpp:CheckSubsidised`) and `src/economy.cpp:TriggerIndustryProduction`.
- Commands: `src/town_cmd.cpp:CmdFoundTown`, `src/town_cmd.cpp:CmdDeleteTown`, `src/town_cmd.cpp:CmdDoTownAction`, `src/industry_cmd.cpp:CmdBuildIndustry`, `src/industry_cmd.cpp:CmdIndustrySetProduction`, `src/station_cmd.cpp:CmdBuildRailStation`, `src/station_cmd.cpp:CmdBuildRoadStop`, `src/station_cmd.cpp:CmdBuildAirport`, `src/station_cmd.cpp:CmdBuildDock`, `src/subsidy.cpp:CmdCreateSubsidy`.
- **[fork]** read-only readers: `src/prismatic_mcp/prismatic_mcp_world.cpp:Stations` and `src/prismatic_mcp/prismatic_mcp_server.cpp:ResourceTowns`, gated by `OTTD_PRISMATIC_MCP_SERVER`.

## Lifecycle

- **Init**: generation creates towns/industries (`src/industry_cmd.cpp:GenerateIndustries`); `src/industry_cmd.cpp:ResetIndustries` restores `_industry_specs` from the original table; `src/cargotype.cpp:SetupCargoForClimate` fills `CargoSpec::array`. Spatial indexes: `src/station.cpp:RebuildStationKdtree`, `src/town_cmd.cpp:RebuildTownKdtree`.
- **Per tick**: towns try to grow when `grow_counter` runs out (`TownTickHandler`); stations update rating every `STATION_RATING_TICKS`, do a big tick (acceptance, deletion of unused stations) every `STATION_ACCEPTANCE_TICKS` offset by station index, and `DeleteStaleLinks` every `STATION_LINKGRAPH_TICKS`; industries run `ProduceIndustryGoods` (constants in `src/timer/timer_game_tick.h:Ticks`).
- **Periodic (economy timers)**: `_economy_towns_monthly`/`_economy_towns_yearly` in `src/town_cmd.cpp`, `_economy_industries_daily`/`_economy_industries_monthly` in `src/industry_cmd.cpp` (production change, closure by `delete i`), `_economy_stations_monthly` in `src/station_cmd.cpp`, `_economy_subsidies_monthly` in `src/subsidy.cpp`.
- **Teardown**: destructors unlink cross references (`src/town_cmd.cpp:Town::~Town`, `src/industry_cmd.cpp:Industry::~Industry`, `src/station.cpp`) and return early when `CleaningPool()` is true.
- **Save/load**: chunks `CITY`/`HIDS` (`src/saveload/town_sl.cpp`), `INDY`/`IIDS`/`TIDS`/`IBLD`/`ITBL`, `STNN`/`STNS`/`ROAD`, `CAPA` (cargo packets), `SUBS`, `CMDL`/`CMPU` (monitors). Caches are rebuilt after load: `src/saveload/town_sl.cpp:RebuildTownCaches`, `src/subsidy.cpp:RebuildSubsidisedSourceAndDestinationCache`, `src/station_base.h:RecomputeCatchmentForAll`.

## Invariants

- **[verified]** Cargo is never created at a station except through `src/station_cmd.cpp:MoveGoodsToStation`, which splits an amount across up to two best-rated stations per company and scales it by rating; producers record `transported` from its return value.
- **[verified]** Station cargo is keyed by next hop: `src/cargopacket.h:StationCargoPacketMap` is a `MultiMap<StationID, CargoPacket *>`; loading asks for the vehicle's next stopping stations (`GetNextStoppingStation`) and the flows in `src/station_base.h:FlowStatMap`.
- **[verified]** Income is booked when the vehicle leaves, not on unload: `src/economy.cpp:CargoPayment::PayFinalDelivery` accumulates and `src/economy.cpp:CargoPayment::~CargoPayment` calls `SubtractMoneyFromCompany`; the payment is deleted in `src/vehicle.cpp:Vehicle::LeaveStation`. One `CargoPayment` per vehicle (`static_assert` in `PrepareUnload`).
- **[verified]** Removing the last facility does not delete a station: `src/station_cmd.cpp:StationHandleBigTick` deletes it after 8 big ticks with `IsInUse()` false, and `OnTick_Station` skips the rest of that station's tick when it is gone.
- **[verified]** Deleting the current item while iterating a pool is safe (`src/core/pool_type.hpp:PoolIterator` holds an index and skips invalid slots); the monthly industry loop relies on it.
- **[verified]** A town cannot be deleted while anything refers to it: `src/town_cmd.cpp:CmdDeleteTown` refuses stations (except oil rigs), waypoints and depots of the town and clears its tiles first; `Town::~Town` then only asserts (`WITH_ASSERT`) that no tile, industry or object points to it. `Industry::~Industry`, by contrast, clears its own tiles.
- **[verified]** Every station has a town: `src/economy.cpp:DeliverGoods` dereferences `st->town` without a check.
- **[verified]** Derived caches must equal a full recompute: `src/cachecheck.cpp:CheckCaches` compares town caches, `industries_near`/`stations_near`, road stop entries, station cargo counts and docking tiles when `-d desync=2` is set.
- **[supported]** `src/timer/timer_game_common.h` (comment on `Priority`): timers other than `Priority::None` may call `Random()`, so their order matters and each priority holds a single timer per trigger; towns, industries, stations and subsidies each own one.
- **[inferred]** `CargoType` is a slot in `CargoSpec::array` assigned per climate and NewGRF set (`src/cargotype.cpp:SetupCargoForClimate`); the stable identity across games is the `CargoLabel` (`src/cargo_type.h:CargoLabel`).

## Extension points

- Town production: `src/town_cmd.cpp:TownGenerateCargoOriginal` / `TownGenerateCargoBinomial`, rates from `src/house.h:HouseSpec`; house tables `src/table/town_land.h`.
- Industry types: original specs `src/table/build_industry.h`, runtime `src/industrytype.h:IndustrySpec` (`_industry_specs`), NewGRF callbacks `src/newgrf_industries.cpp`.
- Cargo types: `src/table/cargo_const.h`, labels in `src/cargotype.cpp`; NewGRF cargo `src/newgrf_cargo.cpp`.
- New cargo movement: a functor in `src/cargoaction.h` plus a `MoveToAction` in `src/cargopacket.h:MoveToAction`.
- Cargo/subsidy sources: `src/source_type.h:SourceType`.
- New station kind/facility: `src/station_type.h:StationFacility`, build command in `src/station_cmd.cpp`, `DEF_CMD_TRAIT` in `src/station_cmd.h`.

## Traps and forbidden operations

- Consume `Random()` independently of client settings: `src/industry_cmd.cpp:ProduceIndustryGoods` evaluates `Chance16R` before testing `_settings_client.sound.ambient`; reversing that `&&` would make the random stream depend on a local setting.
- `CmdFoundTown` is `CommandFlag::NoTest` ("founding random town can fail only in exec run", `src/town_cmd.h`).
- Moving a station sign or creating a station must update `_station_kdtree` (see `src/station_cmd.cpp:BuildOilRig`); town creation inserts into `_town_kdtree`.
- `src/cargopacket.cpp:CargoPacket::InvalidateAllFrom` must run when a cargo source disappears (`Town::~Town` does it), or packets keep a dangling source.
- `src/cachecheck.cpp:CheckCaches` rebuilds caches as a side effect (`RebuildTownCaches`, `RecomputeCatchmentForAll`); `-d desync=2` therefore changes what it observes.

## Tests and validation

- Regression (needs a base set): `regression/regression/main.nut` sections `Cargo`, `CargoList`, `Industry`, `IndustryList`, `IndustryTypeList`, `Station`, `StationList`, `Town`, `TownList` and `PrintSubsidy`; `regression/stationlist/main.nut` (ctest `regression_stationlist`).
- Unit: `src/tests/history_func.cpp` (`RotateHistory`, used by town and industry statistics).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R regression` while iterating.
- Debug: `-d desync=2` for cache validation (`docs/desync.md`), `-d misc=N`.

## Decisions, rules and knowledge

- Rules: `project.game-state-mutates-only-through-commands`, `project.savegame-fields-are-version-gated`, `project.public-api-is-exposed` (script API in `src/script/api/`).
- ADR 0008 (embedded MCP server reads stations and towns).
- Knowledge: `road-stops-and-depots-need-their-entrance-road`.
- Upstream docs: `docs/desync.md`, `docs/savegame_format.md`, `docs/importing_town_data.md`.

## Typical tasks

- To change how cargo appears at stations: start at `src/station_cmd.cpp:MoveGoodsToStation`, then `src/station_cmd.cpp:UpdateStationWaiting`; validate with the `StationList`/`CargoList` regression and `-d desync=2`.
- To change loading or unloading: start at `src/economy.cpp:LoadUnloadVehicle`, then `src/cargopacket.cpp:VehicleCargoList::Stage`; validate with `tools/gate.sh change`.
- To change town growth: start at `src/town_cmd.cpp:GrowTown` and `src/town_cmd.cpp:UpdateTownGrowth`; validate with the `Town` regression section.
- To change industry production: start at `src/industry_cmd.cpp:ProduceIndustryGoods` and `src/industry_cmd.cpp:ChangeIndustryProduction`; keep the `Random()` call order.
- To add a subsidy rule: start at `src/subsidy.cpp:FindSubsidyCargoDestination` and `src/subsidy.cpp:CheckSubsidised`; validate with `PrintSubsidy` output in the regression.
