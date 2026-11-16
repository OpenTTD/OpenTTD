---
schema: knowledge/v1
id: a-script-path-search-spends-game-time
kind: knowledge
class: lesson
title: "A path search in an AI script spends game time: raise the opcode budget and weight the heuristic"
description: "When a Squirrel AI (PrismaticAI) plans long rail or road lines, or an A/B run shows a bot idle for months."
status: verified
epistemics: observed
date: 2026-11-16
tags: [ai, squirrel, pathfinding, performance, settings]
about: src/table/settings/script_settings.ini
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0064
    - file:src/table/settings/script_settings.ini
---

# Lesson

An AI script runs a fixed number of operations per game tick and is then suspended until the next
tick. The budget is `script.script_max_opcode_till_suspend`: default 10000, range 500 to 250000,
`src/table/settings/script_settings.ini`. A plain A* over a 240-tile rail line called the
tile-query API so many times that, in an A/B run, the bot had borrowed for the line but not started
it two game months later, with company value 1. Meanwhile the bus-only baseline was earning. That
run measured the slowness of the search, not the planner, so it was stopped.

# How to apply

- The panel sets `script.script_max_opcode_till_suspend 250000`, the game's maximum, before it
  starts the bot. Scripts run on the server only, so this changes nothing that clients compute.
- PrismaticAI's rail search is a weighted A* (heuristic times 2). It explores far fewer nodes, and
  the line it finds is at most twice the optimum, in practice a few percent longer.
- When a result shows the bot idle while it plans, read the `bot_log` that `mcp-ab` stores before
  judging the strategy.
