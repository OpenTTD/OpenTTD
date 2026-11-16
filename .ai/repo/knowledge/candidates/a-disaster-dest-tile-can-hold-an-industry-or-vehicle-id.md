---
schema: knowledge/v1
id: a-disaster-dest-tile-can-hold-an-industry-or-vehicle-id
kind: knowledge
class: fact
title: "A disaster vehicle's dest_tile holds an IndustryID (airplane, helicopter) or a VehicleID (small UFO) once it has a target"
description: "When reading, drawing, exporting or validating dest_tile of a DisasterVehicle."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [vehicles, consists-groups-and-timetables]
about: src/disaster_vehicle.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/disaster_vehicle.cpp
    - file:src/saveload/vehicle_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/consists-groups-and-timetables/README.md
---

# Statement

`src/disaster_vehicle.cpp:DisasterTick_Aircraft` reads `Industry::Get(v->dest_tile.base())` and
`src/disaster_vehicle.cpp:DisasterTick_Ufo` stores `TileIndex{u->index.base()}` of the targeted road
vehicle in `dest_tile`; `src/disaster_vehicle.cpp:ReleaseDisastersTargetingIndustry` compares it to
an `IndustryID`, and `src/saveload/vehicle_sl.cpp:AfterLoadVehiclesPhase2` turns it back into a
`RoadVehicle`. Code that treats every vehicle's `dest_tile` as a map tile (a viewer, an exporter, an
invariant check) gets a meaningless or out-of-map tile for these states. Check `state` and
`subtype` first.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote a-disaster-dest-tile-can-hold-an-industry-or-vehicle-id` with evidence, or `majordomus knowledge reject a-disaster-dest-tile-can-hold-an-industry-or-vehicle-id --reason ...`.
