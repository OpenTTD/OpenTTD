---
schema: knowledge/v1
id: held-start-duel-pairs-did-not-confirm-the-air-and-catchment-defaults
kind: knowledge
class: fact
title: "Held-start duel pairs on three seeds did not confirm air_add_waiting=10 with catchment_stops=8, which won the paired battle"
description: "Before claiming a bot default from duels, or reading the battle's paired win of air_add_waiting/catchment_stops; ten-year held-start pairs were inconclusive while 36 paired battle seeds favoured the change."
status: candidate
epistemics: observed
date: 2026-11-19
tags: [duel, a-b, prismatic-ai, defaults, noise, ai-battle]
route: project
provenance:
  origin: authored
  derived_from:
    - file:research/results/duel-20261119T124718-seed4242.md
    - file:research/results/duel-20261119T124719-seed4242.md
    - file:research/results/duel-20261119T145044-seed31337.md
    - file:research/results/duel-20261119T145045-seed31337.md
    - file:research/results/duel-20261119T165450-seed20208.md
    - file:research/results/duel-20261119T165451-seed20208.md
    - file:research/results/duel-20261119T103653-seed4242.md
    - file:research/results/duel-20261119T103654-seed4242.md
    - file:tools/research/mcp-duel
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
---

# Fact

The battle changed PrismaticAI's defaults to `air_add_waiting=10` and `catchment_stops=8` in
`f26a6f7f02`, after a paired battle on 36 common seeds over 20 years: +14.8% mean value, better
on 27 of 36. Rule `project.a-tuning-claim-needs-held-start-pairs` asks for held-start duel pairs on
seeds 4242, 31337 and 20208. Those pairs were run on 2026-11-19, with the bot at `a23cbe0df4`.
In that version a panel plan no longer switches growth off. Each pair had the same 120 months,
`PRISMATIC_START_PAUSED=2`, both arms side by side on one machine, and the same rival (the unplanned
PrismaticAI with the defaults of that revision).

| Seed | Arm on (10/8): A / rival | Arm off (0/0): A / rival | Difference | Rival between arms |
|---|---|---|---|---|
| 4242 | 6,026,776 / 4,971,339 = 1.212 | 4,958,663 / 4,087,092 = 1.213 | −0.001 | 21.6% |
| 31337 | 5,768,960 / 7,131,024 = 0.809 | 5,701,100 / 6,708,326 = 0.850 | −0.041 | 6.3% |
| 20208 | 5,044,516 / 5,648,639 = 0.893 | 3,953,357 / 4,705,943 = 0.840 | +0.053 | 20.0% |

In every seed the "on" arm flew more aircraft: 39/36/36 against 50/12/11. Yet no difference
exceeded the rival's own spread between the two arms, and the three signs disagree. By the rule
the result is **inconclusive**. The default rests on the paired battle evidence, which is
deterministic per seed and spans 36 seeds, not on these duels.

The two earlier seed-4242 records (`T103653`, `T103654`, before `a23cbe0df4`) ran a planned bot that
never flew: 20 vehicles, 0 aircraft. They measure only `catchment_stops`, and are inconclusive too:
+8.3% for A against a rival 16% apart.

# How to apply it

- Do not cite these duels as confirmation of the 10/8 defaults. Cite the battle's paired comparison,
  and name it as such.
- With three seeds and a rival that moves 6 to 22% between identical arms, a ten-year duel pair
  resolves only effects much larger than about 20%. For smaller effects use the battle's paired
  seeds (`/api/promotions`), which compare on many fresh maps.
- In these pairs the planned bot (with `a23cbe0df4`) beat the unplanned rival by about 21% on seed
  4242 in both arms. That is the fix's effect, not the defaults'.
