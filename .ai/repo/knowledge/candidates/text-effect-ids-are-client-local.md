---
schema: knowledge/v1
id: text-effect-ids-are-client-local
kind: knowledge
class: constraint
title: "A TextEffectID stored in a game object is per-client view state; game logic must never branch on it"
description: "When reading or adding a TextEffectID (loading indicators, income text) in a vehicle, station or other pooled object."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [graphics-and-sprites, vehicles-and-pathfinding, determinism]
about: src/texteff.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/texteff.cpp
    - file:src/economy.cpp
    - file:src/vehicle_base.h
    - file:src/saveload/vehicle_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/graphics-and-sprites/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md
---

# Statement

`src/vehicle_base.h:Vehicle::fill_percent_te_id` lives in a pooled game object, but
`src/economy.cpp:LoadUnloadVehicle` sets it only when the client setting
`gui.loading_indicators` and `_local_company` allow it, and `src/texteff.cpp:AddTextEffect` returns
`INVALID_TE_ID` in the menu. Its value therefore differs between peers. It is not saved
(`src/saveload/vehicle_sl.cpp` only resets it on load) and not part of the desync check. Use such a
field only to update or hide the effect; any game decision that read it would desync.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote text-effect-ids-are-client-local` with evidence, or `majordomus knowledge reject text-effect-ids-are-client-local --reason ...`.
