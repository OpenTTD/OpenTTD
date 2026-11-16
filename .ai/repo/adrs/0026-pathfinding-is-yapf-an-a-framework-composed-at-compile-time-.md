---
schema: adr/v1
id: adr-0026
kind: adr
title: Pathfinding is YAPF, an A* framework composed at compile time from policy templates and specialised per vehicle type
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - pathfinding
  - vehicles
related:
  - file:.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md
  - file:.ai/repo/workflows/substrate/rail-road-and-infrastructure/README.md
  - file:src/pathfinder/yapf/yapf_base.hpp
  - file:src/pathfinder/yapf/yapf_rail.cpp
provenance:
  origin: extracted
  derived_from:
    - file:src/pathfinder/yapf/yapf_base.hpp
    - file:src/pathfinder/yapf/yapf_rail.cpp
    - issue:I0063
---

# 26. Pathfinding is YAPF, an A* framework composed at compile time from policy templates and specialised per vehicle type

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Trains, road vehicles and ships need different node types, costs, destinations and track
following, but share the A* search itself and its performance tuning.

## Decision

1. **One search, many policies.** `src/pathfinder/yapf/yapf_base.hpp:CYapfBaseT` is the A* core;
   a vehicle type composes it with origin, destination, cost, cache and follower policies through
   a types struct (e.g. `src/pathfinder/yapf/yapf_rail.cpp:CYapfRail_TypesT`).
2. **Compile-time composition.** The policies are template parameters, so the per-node calls are
   inlined; there is no runtime polymorphism in the inner loop.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- A cost change is a policy change and must stay deterministic (ADR 0016): no host-dependent
  floats, no iteration over unordered containers.
- Rail segment costs are cached in a process-static cache that must be invalidated with the map
  (candidate `the-rail-segment-cost-cache-is-a-process-static`).

## For this fork

- The fork's route-drivability checks for the bot ask the game rather than re-implementing YAPF.
- Map: [vehicles and pathfinding](../workflows/substrate/vehicles-and-pathfinding/README.md).
