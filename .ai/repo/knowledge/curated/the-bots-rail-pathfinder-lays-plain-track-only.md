---
schema: knowledge/v1
id: the-bots-rail-pathfinder-lays-plain-track-only
kind: knowledge
class: constraint
title: "PrismaticAI lays rail over buildable land only: a line that needs a bridge, tunnel or level crossing fails"
description: "When choosing which planned rail line the owner's bot builds, or reading why a bot company has no trains."
status: verified
epistemics: observed
date: 2026-11-16
tags: [bot, rail, pathfinding, planner, track-planning]
about: tools/research/mcp-panel/server.py
route: platform
provenance:
  origin: authored
  derived_from:
    - file:research/results/duel-20261116T074419-seed20208.md
    - file:tools/research/mcp-panel/server.py
    - file:tools/research/mcp-rhai/src/linecost.rs
---

# Constraint

The bridge repository's `ai/PrismaticAI/rail.nut` `FindRailPath` is an A* search over buildable
tiles. A water, road or rail tile ends a branch, and the search never places a bridge, a tunnel or
a level crossing. A planned line whose only routes need one of these fails: the bot builds both
stations, logs "no rail path between the stations; removing them", and falls back to buses.

In the 36-month duel on seed 20208 (2026-11-16) this left the planned company without a train.
AlgoGlyph track-planning had found routes for the same line, but they used bridges and a crossing.

# How to apply

- Price rail lines with track-planning (`mcp-plan --track N`) before the bot gets one. The panel's
  bot start passes over a priced line whose counts include a bridge, a tunnel or a crossing, and
  takes the next line of its kind. An unpriced line is still tried blind.
- A bot company with T0 in `companies` after a rail plan: look for "no rail path" in its log
  before blaming the planner's scores.
- Lines that need bridges stay out of reach until the bot learns to place them, or until an
  executor (openttd-07's `mcp-human rail`) lays track-planning's pieces for the bot's company.
