---
schema: knowledge/v1
id: duels-run-side-by-side-are-noisy
kind: knowledge
class: fact
title: "Duels run side by side on a loaded machine differ by 10% or more for the same bot"
description: "Before reading an A/B duel result, or before running several duels at once; the 1% spread measured on a quiet machine does not hold under load."
status: verified
epistemics: observed
date: 2026-11-17
tags: [duel, noise, experiment, a-b]
route: project
provenance:
  origin: authored
  derived_from:
    - file:research/results/duel-20261117T183400-seed31337.md
    - file:research/results/duel-20261117T193920-seed31337.md
    - file:research/results/duel-20261117T193812-seed31337.md
    - file:research/results/duel-20261117T193926-seed20208.md
    - file:research/results/duel-20261117T193930-seed20208.md
    - file:tools/research/mcp-duel
    - file:research/results/duel-20261117T195416-seed31337.md
    - file:research/results/duel-20261117T195419-seed31337.md
    - file:tools/research/mcp-rhai/src/panel.rs
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/the-planner-overpredicts-what-a-line-earns.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/the-bots-rail-pathfinder-lays-plain-track-only.md
---

# Fact

The uncalibrated planned bot on seed 31337, 36 months, same code, ended 1050432 (duel-20261117T183400,
two duels at once) and 950946 (duel-20261117T193920, four duels at once plus a 120-month duel on an
18-core machine at load 13 to 14): 10% apart. The unplanned rival is the same bot in every arm, yet on
seed 31337 it ended 19132 in one arm and 29102 in the other of the same paired run (52% apart).
The earlier "about 1%" between two runs of one bot was measured one duel at a time.

A duel is driven in wall-clock time: the panel plans, then starts the AIs, and the driver samples
months as they pass. On a loaded machine the planner's minutes become a different game tick at which
the planned bot starts, and the game's own randomness diverges from there.

**The start is held since 2026-11-17** (panel `PRISMATIC_START_PAUSED=N`; a duel started from the panel
sets 2): the game stays paused while the planner plans and both bots start, and runs once N AI
companies exist; the result's plan log says at which tick. Two identical 6-month duels on seed 31337,
run at once on the loaded machine, then ended with the same fleets every month and the rival at 9852
and 9830 (0.2%; duel-20261117T195416 and T195419), against 52% before. Not bit-identical yet, so keep
reading A/Bs against repeats.

# How to apply it

- Read an A/B only against its own noise: put the rival's spread (the same bot in both arms) next to
  the planned bot's difference. A difference smaller than that says nothing.
- Run duels with the held start (`mcp-duel` and the panel's duel form both default to
  `PRISMATIC_START_PAUSED=2` since 2026-11-18; `=0` starts unpaused), prefer several seeds and repeats over one pair, and run duels one or two at a
  time when the result is meant to decide a default.
- The paired corrected-calibration A/B of 2026-11-17 is such a case: seed 31337 −16.4%
  (794737 vs 950946), seed 20208 +43.5% (796631 vs 555180). Inconclusive; calibration stays off.
