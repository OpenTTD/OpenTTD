---
schema: knowledge/v1
id: a-bot-change-that-moves-money-in-every-game-replays-as-chaos
kind: knowledge
class: fact
title: "A PrismaticAI change that moves a few units of money in every game sends exact replays down other paths"
description: "Before reading an ablate --match A/B of a PrismaticAI change: replays are exact, so any game-state difference, even a few units of interest, diverges the whole game, and a few replays then measure chaos, not the change."
status: verified
epistemics: observed
date: 2026-11-21
tags: [prismatic-ai, ai-battle, ablate, a-b, noise, experiment]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/prismatic-ai/PrismaticAI/air.nut
    - file:tools/research/ai-battle/battle.py
    - file:research/ai-battle/README.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/prismatic-ai-without-a-steward-keeps-one-train-per-line.md
  - type: relates_to
    target: file:.ai/repo/workflows/ai-battle-last-place-diagnosis.md
---

# Fact

`battle.py ablate --match` replays an arena match exactly: the same build, seed and lineup give
the same final values, to the unit. That exactness cuts both ways. Any difference in game state,
however small, changes what the other AIs and the town and economy code see from then on, and
20 years later the game is a different game.

Observed on 2026-11-21 with the airport-levelling fix (`LevelKeeping`, air.nut, commit 8c8d025388).
Its first version paid the loan down and took it back around every airport levelling. That moved
the company's money by a few units (interest falls due at a month's end while the levelling runs).
In 8 replays from editions 5e5b9f93 to 799899b1 the final values changed by -56.6% to +55.7%, about
-12% geometric; in 99fb93f6 m-0041 the bot logs stayed identical up to a line with funds 195684
against 195682, then diverged. A second version, for sites whose estimate times eight reached the
budget, still changed 3 of the 8 (+14.6%, -46.5%, -22.8%); in each of those three the logged
levelling completed for 10k to 18k against estimates of 15k to 29k, so the fix changed nothing but
the loan's timing. Limited to sites estimated above 30000 (where the estimate is known to fail),
the 8 replays came out identical to the unit, and the bankrupt game the fix was for (512c95be
m-0052) went from 1 to 2.91M.

By contrast the oil-rig planner fix (531922be7d) changed what the bot does in the games it touched,
and its 8 replays moved within 0 to +9.5% with no loss: a systematic effect shows a narrow spread
of one sign.

# How to apply it

- When an A/B of a PrismaticAI change spreads widely in both directions (tens of percent up and
  down), diff the two replays' bot logs (`--log-dir`) before believing the mean: find the first
  line that differs and check whether the change acted there or only nudged state.
- Make a fix a no-op where it is not needed. A guard that triggers in every game, even harmlessly,
  turns every replay into a new sample of the game's chaos, and an A/B over a handful of games
  cannot see the fix through it.
- A change that rightly acts in many games (a new rule the bot follows everywhere) needs many more
  replays than 8, or the arena's own editions, before its sign is trusted.
