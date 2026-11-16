---
schema: adr/v1
id: adr-0024
kind: adr
title: The cargodist link graph is computed in background threads and joined at a fixed economy date, pausing the game rather than joining late
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - economy
  - threads
  - determinism
related:
  - file:.ai/repo/workflows/substrate/economy-and-companies/README.md
  - file:.ai/repo/workflows/substrate/core-loop/README.md
  - file:src/linkgraph/linkgraphschedule.cpp
  - file:docs/linkgraph.md
provenance:
  origin: extracted
  derived_from:
    - file:src/linkgraph/linkgraphschedule.cpp
    - file:src/linkgraph/linkgraphjob.cpp
    - file:docs/linkgraph.md
    - issue:I0066
---

# 24. The cargodist link graph is computed in background threads and joined at a fixed economy date, pausing the game rather than joining late

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

Cargo distribution solves flow problems over every link graph, which takes longer than a tick on
large games; doing it in the game loop would stall it, and doing it asynchronously risks peers
seeing results at different times (ADR 0016).

## Decision

1. **Spawn and join on the economy calendar.** `src/linkgraph/linkgraphschedule.cpp:OnTick_LinkGraph`
   spawns the next job at one offset of `linkgraph.recalc_interval` and joins at the half-way
   offset, at `LinkGraphSchedule::SPAWN_JOIN_TICK`.
2. **Jobs work on a copy.** A `LinkGraphJob` runs on its own thread over a snapshot; its result is
   merged only at its join date (`join_date` in `src/linkgraph/linkgraphjob.cpp`).
3. **Never join late.** When a job is due but unfinished, the game pauses with
   `PauseMode::LinkGraph` (posted as a command two ticks early so every peer pauses at the same
   tick; `AfterLoad_LinkGraphPauseControl` after load) until it finishes.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- Link graph handlers read only their job (candidate `link-graph-handlers-read-only-their-job`):
  touching live game state from a job thread races and desyncs.
- A slow machine shows `PauseMode::LinkGraph` pauses instead of diverging; an experiment that
  measures pauses must separate this cause from the others (candidate `every-pause-bit-is-taken`).

## For this fork

- Economic experiments that change `linkgraph.*` settings change when results land; record them
  in the manifest.
- Map: [economy and companies](../workflows/substrate/economy-and-companies/README.md).
