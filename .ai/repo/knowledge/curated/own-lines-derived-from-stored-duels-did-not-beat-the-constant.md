---
schema: knowledge/v1
id: own-lines-derived-from-stored-duels-did-not-beat-the-constant
kind: knowledge
class: fact
title: "own_lines derived from stored duel outcomes did not beat the constant 8 on three held-start ten-year pairs"
description: "Before turning PRISMATIC_BOT_FROM_DUELS on by default, changing the rule own-lines-by-handed-v1, or tuning own_lines again from duels."
status: verified
epistemics: observed
date: 2026-11-18
tags: [bot, own-lines, duel, a-b, planning, noise]
about: tools/research/mcp-rhai/src/duel_outcomes.rs
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0087
    - file:tools/research/mcp-rhai/src/duel_outcomes.rs
    - file:tools/research/mcp-rhai/src/panel_bot.rs
    - file:research/results/duel-20261118T102030-seed31337.md
    - file:research/results/duel-20261118T102031-seed31337.md
    - file:research/results/duel-20261118T123135-seed4242.md
    - file:research/results/duel-20261118T123138-seed4242.md
    - file:research/results/duel-20261118T143645-seed20208.md
    - file:research/results/duel-20261118T143728-seed20208.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/the-bots-rail-pathfinder-lays-plain-track-only.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - type: relates_to
    target: file:.ai/repo/rules/project/a-tuning-claim-needs-held-start-pairs.v1.md
---

# Fact

I0087 made the planned bot's `own_lines` and `rail_after` derivable from stored duel outcomes
(`PRISMATIC_BOT_FROM_DUELS=1`, rule `own-lines-by-handed-v1` in `tools/research/mcp-rhai/src/duel_outcomes.rs`).
The rule is keyed on how many freight lines the plan hands over. If some near pair paid beyond the spread
and none cost, it takes twice the largest own_lines that paid. If none paid, it takes 0. On 2026-11-18 it was
measured against the constant (`OWN_LINES` = 8) on held-start pairs (`PRISMATIC_START_PAUSED=2`), 120
months, both halves side by side, each pair handed the same lines:

| Seed | Lines handed | Derived own_lines | Constant (8) | Derived | Change | Rival spread | Duels |
|---|---|---|---|---|---|---|---|
| 31337 | 13 | 0 | 4940838 | 4213684 | -14.7% | 29.3% | duel-20261118T102031, T102030 |
| 4242 | 10 | 16 | 4072265 | 3717995 | -8.7% | 30.5% | duel-20261118T123135, T123138 |
| 20208 | 9 | 16 | 3410336 | 4011279 | +17.6% | 56.0% | duel-20261118T143645, T143728 |

Every difference is inside its rival's spread (mcp-duel's `rival_spread` over the held ten-year duels of
the seed), and two of the three go against the rule. **The derived rule stays off and `OWN_LINES` = 8
stays the default** (ADR 0034 point 6, rule `project.a-tuning-claim-needs-held-start-pairs`). `rail_after`
was never derived: no stored pair varies it, so it always came from the plan's scores.

Three findings came with the measurement:

- **Ten-year held runs do not repeat.** The same seed, the same handed lines and the same own_lines
  setting, held, a day apart: 31337 own_lines 8 4433637 then 4940838 (+11.4%), own_lines 0 4479471 then
  4213684 (-5.9%); 20208 own_lines 8 3690922 then 3410336 (-7.6%); 4242 own_lines 8 4022718 then 4072265
  (+1.2%). The 0.2-0.3% agreement measured at months 6 and 12 does not carry to month 120 (knowledge
  `duels-run-side-by-side-are-noisy`). So one ten-year pair per seed is a single draw, and yesterday's
  "own lines pay at 9-11 handed lines, neutral at 14" rests on one draw per seed as well.
- **The rule compares every value against the smallest only.** In the seed 4242 pair, 16 measured
  below 8, but as "16 against 0" it still paid, so the 20208 run took 16 again. A next rule should set
  each value against the best measured one, and it needs repeats per arm before it doubles anything.
- **Own lines take industries from the rival.** Within a pair the rival differs by up to 30% (4242:
  92215 against 70675), so the "rival spread" of a pair whose arms differ is partly an effect of the arm,
  not noise alone.

The history the rule read: the six held ten-year duels of 2026-11-17/18 imported once into
`$PRISMATIC_STORE_DIR/duel-import.duckdb` (`mcp-panel --import-duels`), plus the `plan.duel` events
the pairs of this table published into their own stores (each later derived half read the earlier pairs:
6, 8 and 10 outcomes).

# How to apply it

- Keep `PRISMATIC_BOT_FROM_DUELS` off by default. Turn it on per run only to measure a changed rule, and
  record the pairs the same way (workflow `a-decision-reads-the-store`).
- Before trusting any own_lines difference over ten years, repeat each arm: the repeat spread of one arm is
  5-11% on two of the three seeds, the same size as the effects being claimed.
- Start the two halves of a pair at least a few seconds apart. They ended in the same second once and got
  one record name; `mcp-duel` now claims the name with an exclusive create.
