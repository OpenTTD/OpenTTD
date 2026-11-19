---
schema: adr/v1
id: adr-0036
kind: adr
title: PrismaticAI's settings are tuned by a deterministic genetic algorithm over the LAN battle, and a tuned setting reaches a default only through held-start duels
status: accepted
date: 2026-11-19
tags:
  - ai-battle
  - prismatic-ai
  - tuning
  - genetic-algorithm
related:
  - file:.ai/repo/adrs/0035-company-ais-are-ranked-in-headless-null-driver-matches-on-ever.md
  - file:.ai/repo/rules/project/a-tuning-claim-needs-held-start-pairs.v1.md
  - file:.ai/repo/rules/project/prismatic-ai-is-never-last.v1.md
  - file:tools/research/ai-battle/battle.py
provenance:
  origin: authored
  derived_from:
    - file:tools/research/ai-battle/battle.py
    - file:research/ai-battle/README.md
---

# 36. PrismaticAI's settings are tuned by a deterministic genetic algorithm over the LAN battle, and a tuned setting reaches a default only through held-start duels

## Context

The owner asked for self-tuning, data-driven and history-adjusted, and named genetic algorithms
(2026-11-19). PrismaticAI exposes its strategy as settings (aggressiveness, max_routes, own_lines,
cash_reserve, line_budget, air_planes, ...). The LAN battle (ADR 0035) runs matches that are a
pure function of seed, lineup, length and build, on every machine at once. That makes it a
cheap, noise-free fitness function, where duels are expensive and noisy (knowledge
`duels-run-side-by-side-are-noisy`).

## Decision

`battle.json` `tuning` turns the battle into a genetic algorithm over a declared settings space:

1. **Population.** A generation is `population` variants in the PrismaticAI group, beside fixed
   anchors (PrismaticAI with defaults, and solo-max). The group plays one member per match. Its
   members take turns on the **same seed** (common random numbers), so every variant meets the
   same maps and field.
2. **Fitness** = mean normalised points per match (1 for a win, 0 for last) + 0.1 x mean
   log10(value / winner's value), over each variant's scored matches.
3. **Generation step.** Once every variant has `matches_per_variant` scored matches, the
   orchestrator publishes the round's record and keeps the best variant unchanged (elitism). It
   breeds the rest by tournament selection, uniform crossover and step mutation inside the
   declared bounds, with an RNG seeded by the generation number. It then restarts itself into the
   new edition. The state and the full history (genes, parents, fitness, anchors) live in
   `~/ttd-battle/tuning/state.json` and are served at `/api/tuning`.
4. **No default changes from here.** A tuned variant is a candidate. Its settings become a bot
   default only through held-start duel pairs (rule `project.a-tuning-claim-needs-held-start-pairs`).

5. **Model-guided offspring.** Up to `brain_offspring` children per generation come from the
   proposals of `tools/research/battle-brain`: an Axon/EXLA ensemble that learns settings to
   log value share from every recorded result and ranks the gene space by an upper confidence
   bound. Evolution then explores where the model is uncertain or promising, not only where
   mutation lands.

## Consequences

- Tuning runs unattended on every arena. Each generation is a record under `research/results/`.
- The fitness is relative to the field and the match parameters of the edition, and it does not
  carry over to the panel-steered bot.
- One seed per group round removes map luck from the comparison, but not breakdown randomness
  inside a match. Variants with few matches remain noisy, which is why `matches_per_variant`
  exists.
