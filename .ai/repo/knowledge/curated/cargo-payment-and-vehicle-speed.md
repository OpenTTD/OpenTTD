---
schema: knowledge/v1
id: cargo-payment-and-vehicle-speed
kind: knowledge
class: fact
title: "Income per delivery is distance times a time factor; ground vehicles cover about 0.036 tiles a day per km/h"
description: "When a planner, bot or script estimates what a route earns, how long cargo is in transit, or which mode of transport pays."
status: verified
epistemics: observed
date: 2026-11-16
tags: [economy, cargo, payment, speed, planner, passengers, prices]
about: src/economy.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0053
    - file:src/economy.cpp
    - file:src/table/cargo_const.h
    - file:src/ground_vehicle.hpp
    - file:src/vehicle_base.h
    - file:src/train_cmd.cpp
    - file:src/town_cmd.cpp
    - file:src/table/pricebase.h
    - file:tools/research/mcp-rhai/src/plan.rs
---

# Fact

**Payment**: `src/economy.cpp` `GetTransportedGoodsIncome` (around line 957). Without a NewGRF
callback, income is `dist * time_factor * num_pieces * current_payment / 2^21`. `dist` is the
Manhattan distance between the source and destination stations. `transit_periods` is first scaled
by the `cargo_aging_rate` setting. With `periods1, periods2 = cs->transit_periods`:

- `time_factor = max(255 - max(t - periods1, 0) - max(t - periods1 - periods2, 0), 31)` while
  `2(t - periods1) - periods2 <= 224`;
- beyond that, `max(2*31*16*16 / (over + 32), 1) / 16`, which falls towards 1 and joins 31
  continuously.

Passengers (`src/table/cargo_const.h:58`) have `initial_payment` 3185 and `transit_periods`
[0, 24]. Their factor is 255 - t up to 24 periods, then 279 - 2t, and reaches the floor of 31 at
124 periods. A cargo period is 185 ticks (`CARGO_AGING_TICKS`, `src/cargopacket.h`), which is 2.5
days at 74 ticks a day.

**Speed in tiles**: `src/ground_vehicle.hpp` `DoUpdateSpeed` advances `speed * 3/4` progress a call.
A movement step needs 192 progress (`TILE_AXIAL_DISTANCE`, `src/vehicle_base.h:29`), and a tile
has 16 steps. Road vehicles count speed in 0.5 km/h units and run once a tick. Trains count in
km/h and run their handler twice a tick (`src/train_cmd.cpp:4213-4215`). Both therefore cover
`kmh * 2 * 0.75 / 3072` tiles a tick: about **0.036 tiles a day, or 0.0903 tiles a cargo period,
per km/h** (100 km/h is about 3.6 tiles a day). Aircraft are slowed by the `plane_speed` setting
(`src/aircraft_cmd.cpp:659`, default 1/4), so their effective speed is a quarter of the displayed
one.

**Passengers generated**: `src/town_cmd.cpp` `TownGenerateCargoBinomial` gives each house, on each
of its tile loops (256 ticks), up to `(population + 7) / 8` potential passengers at 50% each. That
is about `population / 16` per loop, or roughly `0.54 * population` a month, for the whole town.
A station gets only what its catchment covers and its rating keeps.

**Base prices** (`src/table/pricebase.h`): a road piece 95 (a full tile 190), a rail track 100, a
road depot 500, a rail depot 600, a bus or truck stop 200, a rail station 200 plus 180 per length,
an airport tile 600. Vehicles: `BuildVehicleTrain` 400000, `BuildVehicleAircraft` 700000, each
scaled by its engine's cost factor. The construction cost setting and inflation scale all of them.

# How to apply

- Estimate a route's value from these formulas, not from a guessed curve.
  `tools/research/mcp-rhai/src/plan.rs` (model v2) does it, and every constant above is one of its
  parameters, so a caller can replace it.
- Income per passenger rises with distance as long as transit time stays short. Long and fast
  routes pay the most per passenger, which is why early aircraft and fast trains are profitable
  between large towns far apart.
- Before building, query live prices and vehicle data instead of trusting base prices:
  `ScriptCargo::GetCargoIncome(cargo, distance, days_in_transit)`, `ScriptEngine::GetPrice`,
  `GetMaxSpeed`, `GetCapacity`, `GetRunningCost`, `ScriptRoad::GetBuildCost`, `ScriptAirport::GetPrice`.
- Not modelled yet: vehicle purchase and running costs, station rating and catchment beyond the
  `supply_factor` parameter, terraforming and bridges. The planner says so in every answer
  (`not_modelled`).
