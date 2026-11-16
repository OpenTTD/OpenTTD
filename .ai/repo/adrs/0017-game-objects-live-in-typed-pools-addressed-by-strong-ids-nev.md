---
schema: adr/v1
id: adr-0017
kind: adr
title: Game objects live in typed pools addressed by strong IDs, never by persistent pointers
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - core
  - saveload
  - network
related:
  - file:.ai/repo/workflows/substrate/core-utilities/README.md
  - file:.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md
  - file:.ai/repo/workflows/substrate/towns-industries-stations/README.md
  - file:.ai/repo/workflows/substrate/saveload/README.md
  - file:src/core/pool_type.hpp
provenance:
  origin: extracted
  derived_from:
    - file:src/core/pool_type.hpp
    - file:src/core/pool_func.hpp
    - issue:I0066
---

# 17. Game objects live in typed pools addressed by strong IDs, never by persistent pointers

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Vehicles, stations, towns, companies, orders, industries, goals, story pages and many more
objects are created and destroyed throughout a game, must be written to a savegame and must be
named identically on every peer of a network game (ADR 0016).

## Decision

1. **One pool per object type.** `src/core/pool_type.hpp:Pool` is a template over the item type,
   a strong index type (`requires std::is_base_of_v<PoolIDBase, Tindex>`), a growth step, a pool
   type and an allocation-cache flag; items derive from `Pool::PoolItem` and are created with
   `new` through the pool's allocator, which takes the first free slot.
2. **Strong IDs.** The index type is a distinct strong type per pool (`VehicleID`, `StationID`,
   ...), so an ID of one pool cannot be passed where another is expected; `MAX_SIZE` comes from
   the index type's `End()`.
3. **IDs are the handle.** Cross-object references that must survive a save or cross the wire
   are IDs; pointers are rebuilt after load.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- An ID is reused after its object is deleted: a stale ID silently names a different object. A
  holder must re-validate (`IsValidID`, `GetIfValid`) at every use, and code that caches IDs over
  ticks (research instrumentation included) must handle reuse.
- Slot allocation order is part of the game state: two peers that create objects in different
  orders diverge. Creation belongs in command execute passes and the game loop only.
- Pool capacity is bounded by the index type; `CanAllocateItem` must be checked in a command's test
  pass before an execute pass allocates.
- Iteration (`Iterate()`) is in slot order, which is deterministic across peers, unlike iteration
  over a hash container keyed by pointer.

## For this fork

- The PrismaticMCP world graph and entity resources (`src/prismatic_mcp/`) expose pool IDs as the
  stable names agents use; an ID read in one tick may name another object later.
- Maps: [core utilities](../workflows/substrate/core-utilities/README.md),
  [saveload](../workflows/substrate/saveload/README.md),
  [vehicles](../workflows/substrate/vehicles-and-pathfinding/README.md).
