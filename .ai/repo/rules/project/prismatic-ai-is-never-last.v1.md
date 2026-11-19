---
id: project.prismatic-ai-is-never-last
version: 1
kind: rule
title: PrismaticAI with default settings is never last, in the battle's newest record or on the fitness seed
description: The contestants named in battle.json never_last (PrismaticAI with default settings, and PrismaticAI self-steward) may not be last on the newest battle record's leaderboard (battle.py check) or by final value in the fitness match (battle.py fitness); both run in tools/gate.sh change.
statement: Keep every contestant in battle.json never_last above last place, in the newest battle record and in the fitness match. Fix the bot when it falls there, and never drop a contestant from the list to make a check pass.
status: active
class: blocking
depends_on: [project.prismatic-ai-passes-the-headless-fitness-check@1, project.battle-records-are-reproducible@1]
tags: [prismatic-ai, ai-battle, regression, enforcement]
---

# Rationale

The owner's order (2026-11-19): "PrismaticAI must never be last in any form". In the battle's
editions b2ff6a45 and 155d18ea, PrismaticAI with no settings ended last in every match. A bot
started without a plan from the panel ran a few buses and stopped. The cause, and the fix
(`autopilot`, default 1, which turns on the in-bot steward, own freight lines and intercity
roads when there is no plan), are in knowledge
`prismatic-ai-without-a-steward-keeps-one-train-per-line`.

This changes the behaviour of the unplanned bot by the owner's order, not by a tuning
measurement. The duels' unplanned rival keeps its old behaviour with `autopilot=0`, so rule
`project.a-tuning-claim-needs-held-start-pairs` still governs any default chosen by measurement.

# Required behaviour

- `battle.json` lists the protected contestants under `never_last`, and the fitness lineup under
  `fitness.never_last`. Both lists name `PrismaticAI` (no settings) and `PrismaticAI self-steward`.
- A change that makes either of them last fails the gate. Fix the bot. Do not remove the
  contestant or weaken the fitness lineup.
- The control contestant `PrismaticAI solo-max` (`autopilot=0`, no steward) is not protected: it
  exists to measure what the steward adds.

# Failure behaviour

`battle.py check` exits 10 when a protected contestant is last on the newest record's leaderboard
(among contestants that played). `battle.py fitness` exits 10 when one is last by final value on
the fitness seed. `tools/gate.sh change` runs both. The fitness check runs when the bot or the
battle tooling changed.

# Verification

`python3 tools/research/ai-battle/battle.py check`, `python3 tools/research/ai-battle/battle.py
fitness`, and `tools/research/test_ai_battle.py` (`test_fitness_fails_when_a_never_last_bot_is_last`).
