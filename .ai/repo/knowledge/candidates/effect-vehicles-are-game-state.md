---
schema: knowledge/v1
id: effect-vehicles-are-game-state
kind: knowledge
class: constraint
title: "Effect vehicles (smoke, sparks, explosions, bubbles) are lockstep game state, not client-side decoration"
description: "When adding, removing or gating the creation of an effect vehicle, or treating smoke as purely visual."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [vehicles, consists-groups-and-timetables, determinism]
about: src/effectvehicle.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/effectvehicle.cpp
    - file:src/vehicle.cpp
    - file:src/saveload/vehicle_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/consists-groups-and-timetables/README.md
---

# Statement

`src/effectvehicle.cpp:CreateEffectVehicle` takes a slot in the shared `VehiclePool`, and several
init/tick procs draw the game `Random()` (`ChimneySmokeInit`, `BubbleTick`; callers such as
`src/vehicle.cpp:Vehicle::ShowVisualEffect` use `Chance16`). Effect vehicles are saved in `VEHS`
(`animation_state`, `animation_substate` in `src/saveload/vehicle_sl.cpp`). Creating or skipping one
on a client-only condition (a client setting, transparency, a dedicated server) shifts the random
stream and later `VehicleID`s and desyncs. Only drawing (`EffectVehicle::GetTransparencyOption`) and
ambient sound may depend on client state. They are never put in the tile hash
(`src/vehicle.cpp:Vehicle::UpdatePositionAndViewport`), so they never block construction.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote effect-vehicles-are-game-state` with evidence, or `majordomus knowledge reject effect-vehicles-are-game-state --reason ...`.
