---
schema: adr/v1
id: adr-0025
kind: adr
title: Game time runs on separate tick, calendar, economy and real-time timers, and periodic work subscribes to the one that matches its meaning
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - timers
  - core-loop
related:
  - file:.ai/repo/workflows/substrate/core-loop/README.md
  - file:.ai/repo/workflows/substrate/economy-and-companies/README.md
  - file:.ai/repo/workflows/substrate/consists-groups-and-timetables/README.md
  - file:src/timer/timer_game_economy.h
  - file:src/timer/timer_game_calendar.h
provenance:
  origin: extracted
  derived_from:
    - file:src/timer/timer_game_economy.h
    - file:src/timer/timer_game_calendar.h
    - file:src/timer/timer_game_realtime.h
    - file:src/timer/timer_game_tick.h
    - issue:I0066
---

# 25. Game time runs on separate tick, calendar, economy and real-time timers, and periodic work subscribes to the one that matches its meaning

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

The calendar (the date shown, vehicle introduction years, seasons) and the economy (production,
payments, ratings, ageing) used to share one clock; a wallclock timekeeping mode lets the
economy run in fixed-length "minutes" while the calendar runs slower or is frozen.

## Decision

1. **Four clocks.** `src/timer/timer_game_tick.h:TimerGameTick` counts ticks;
   `src/timer/timer_game_calendar.h:TimerGameCalendar` the displayed date;
   `src/timer/timer_game_economy.h:TimerGameEconomy` the economy's days, months and years (its
   header lists what runs on it); `src/timer/timer_game_realtime.h:TimerGameRealtime` wall time
   for presentation work.
2. **Subscription.** Periodic work registers an interval timer on the clock whose meaning it has,
   with a priority that orders timers of the same period.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- Choosing the wrong clock is a behaviour bug that only shows in wallclock mode.
- Real-time timers must never touch game state (ADR 0016).
- Timers with equal period and priority run in pointer order; a timer that draws `Random()` needs
  its own priority (candidate `a-random-timer-needs-its-own-priority`).

## For this fork

- Research measurements state which clock they sample and record the timekeeping mode.
- Map: [core loop](../workflows/substrate/core-loop/README.md).
