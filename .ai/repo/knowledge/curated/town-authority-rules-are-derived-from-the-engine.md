---
schema: knowledge/v1
id: town-authority-rules-are-derived-from-the-engine
kind: knowledge
class: fact
title: "Town-authority rules (ratings, gates, actions) are derived from src/ into one model; read it, do not copy numbers"
description: "Before writing bot, script or panel logic about town ratings: where the derived model is, what it holds, and the rules a company most often trips over."
status: verified
epistemics: derived
date: 2026-11-22
tags: [town, town-rating, prismatic-ai, rhai, control-panel, derived-model]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/town-model/derive.py
    - file:tools/research/town-model/town_model.json
    - file:src/town_cmd.cpp
    - file:src/town_type.h
relations:
  - type: relates_to
    target: file:research/mechanisms/town-authority.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/levelling-an-airport-site-clears-trees-and-costs-town-rating.md
---

# Fact

`tools/research/town-model/derive.py` reads the engine sources and writes the town-authority
model to `tools/research/town-model/town_model.json`. The model holds:
- the `RATING_*` constants;
- the rating bands an AI sees;
- every place a company's rating changes (15 today);
- every comparison of a rating with a threshold (13);
- the eight town actions with their cost factors and effects;
- the rules tying a town to stations;
- the settings involved;
- the relations graph.

Every rule records its source file and line. The same model is emitted as `TOWN_MODEL` for
PrismaticAI (`town_model.nut`, helpers in `town.nut`) and as `town_model.rhai` for scripts
(`town_lib.rhai`). It is also emitted as the tables of `research/mechanisms/town-authority.md`,
and the panel's Towns page shows it. `derive.py --check` runs in `tools/gate.sh change`.

The rules that matter most for a bot:
- **New stations:** the authority refuses a new station at -200 or below. Every cleared tree tile
  costs -35, and levelling clears them too. An idle station near the town costs -15 a month, an
  active one gives +12, and below 200 the rating recovers by 5 a month by itself.
- **Raising the rating:** planting trees raises it by 7 per tile, but only up to 220.
- **What an AI sees:** only the band (`AITown.GetRating`), up to 200 below the real value.
  `openttd://towns` (MCP, research build) now gives the exact rating with active and idle stations
  per company.

# How to apply it

- Read thresholds from `TOWN_MODEL` (Squirrel) or `town_lib.rhai` (Rhai); never write a rating
  number by hand. After an engine change, run `derive.py` and commit its outputs.
- A town-aware decision in PrismaticAI still needs an A/B. The two decisions tried so far, a
  tree-count guard and replanting, are in the levelling record.
