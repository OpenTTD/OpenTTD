---
schema: knowledge/v1
id: a-random-timer-needs-its-own-priority
kind: knowledge
class: constraint
title: "Game timers with the same period and priority run in pointer order, so a timer that draws Random() needs a Priority value of its own"
description: "Before adding an IntervalTimer on TimerGameEconomy or TimerGameCalendar."
status: candidate
epistemics: observed
date: 2026-11-16
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
