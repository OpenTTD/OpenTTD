---
schema: knowledge/v1
id: prismatic-ai-without-a-steward-keeps-one-train-per-line
kind: knowledge
class: fact
title: "PrismaticAI run without the panel keeps one train per rail line and hoards its earnings, so its fleet shrinks"
description: "Before reading any result of PrismaticAI started without the MCP panel (the LAN AI battle, a plain start_ai): its rail lines never grow and fleet.nut sells loss-making road vehicles without replacing them."
status: verified
epistemics: observed
date: 2026-11-19
tags: [prismatic-ai, steward, ai-battle, fleet, diagnosis]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/prismatic-ai/PrismaticAI/lines.nut
    - file:tools/research/prismatic-ai/PrismaticAI/fleet.nut
    - file:tools/research/prismatic-ai/PrismaticAI/main.nut
    - file:tools/research/prismatic-ai/PrismaticAI/steward.nut
    - file:tools/research/mcp-panel/rhai/rail_lib.rhai
    - file:tools/research/ai-battle/battle.py
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - type: relates_to
    target: file:.ai/repo/adrs/0035-company-ais-are-ranked-in-headless-null-driver-matches-on-ever.md
---

# Fact

A rail line of PrismaticAI runs what the Rhai steward sets: `line<N>_trains` and `line<N>_wagons`,
default -1, which means no target (`lines.nut:ReconcileLines`). The steward lives in the MCP panel
(`rail_steward.rhai` with `rail_lib.rhai`). Started without the panel, by `start_ai` in the LAN AI
battle, the bot keeps the one train each line was built with. Once it has planned `own_lines`
lines, nothing else spends its money. `fleet.nut` sells road vehicles that lose money or sit idle
and does not replace them.

Observed on 2026-11-19 in the battle's edition b2ff6a45, in every match the orchestrator's
self-monitoring watched: solo-max (`own_lines=8`) reaches 20-29 vehicles by its second or third
year, then falls to 11-17 (`fleet_drop` alerts) while its bank balance rises into the millions.
With default settings (`own_lines=0`, `max_routes=4`) PrismaticAI stays at 2-4 vehicles.

On the same seed and binary, with the working tree of 2026-11-19, the opt-in in-bot steward
(`self_steward=1`, `steward.nut`) roughly doubled to quintupled solo-max's value at year 10. On two
seeds it ran 30 vehicles where solo-max ran 9-12, with 10 and 8 trains added and one line closed.
That is two seeds, unpaired with a held start, and in a headless battle rather than a duel, so it
is no reason to change a default (rule `project.a-tuning-claim-needs-held-start-pairs`). The
battle's records compare the two contestants on every seed of an edition.

Since 2026-11-19 the unplanned bot defaults to `autopilot=1`, which turns on this steward, own
lines and intercity roads when no plan was handed over, by the owner's order that PrismaticAI is
never last (rule `project.prismatic-ai-is-never-last`). `autopilot=0` gives the old unplanned
bot. The battle's control contestant `PrismaticAI solo-max` runs that way.

# How to apply it

- Do not read PrismaticAI's battle results as what the bot does when the panel steers it. The
  default and solo-max contestants measure the bot without its steward. `PrismaticAI self-steward`
  measures the steward's policy run inside the bot.
- A change to `lines.nut`, `fleet.nut`, `steward.nut` or the own-lines loop in `main.nut` must
  pass `tools/research/ai-battle/battle.py fitness`. `tools/gate.sh change` runs it on such a
  change, and it fails on a collapsing fleet of the contestant it holds healthy.
- During a run, read the dashboard's alerts or a record's `anomalies`. A `fleet_drop` on a
  contestant is the signature of this fact.
