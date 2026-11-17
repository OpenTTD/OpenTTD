---
schema: knowledge/v1
id: a-random-timer-needs-its-own-priority
kind: knowledge
class: constraint
title: "Game timers with the same period and priority run in pointer order, so a timer that draws Random() needs a Priority value of its own"
description: "Before adding an IntervalTimer on TimerGameEconomy or TimerGameCalendar."
status: verified
epistemics: observed
date: 2026-11-17
tags: [core-loop, economy-and-companies]
about: src/timer/timer_manager.h
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/timer/timer_manager.h
    - file:src/timer/timer_game_common.h
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-loop/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/economy-and-companies/README.md
---

# Statement

The sorter in `src/timer/timer_manager.h` breaks ties on equal trigger and priority by comparing
timer addresses, which are not equal across peers. `src/timer/timer_game_common.h` documents one
timer per priority. Extend the `Priority` enum of the timer type instead of reusing an existing
value.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-random-timer-needs-its-own-priority` with evidence, or `majordomus knowledge reject a-random-timer-needs-its-own-priority --reason ...`.

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- src/timer/timer_manager.h:105-111 base_timer_sorter: equal period returns a < b, a pointer comparison.
- src/timer/timer_game_common.h:137-150 TPeriod compares trigger then priority, so equal trigger and priority tie on the pointer; :21-23 documents "make sure you only set one callback per priority" and no Random() under Priority NONE.
