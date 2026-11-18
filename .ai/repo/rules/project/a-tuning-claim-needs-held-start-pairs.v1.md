---
id: project.a-tuning-claim-needs-held-start-pairs
version: 1
kind: rule
title: A bot or planner default changes on measurement only after held-start duel pairs on three seeds
description: A constant, calibration or derived rule of the planned bot or the planner (own_lines, rail_after, calibration factors, a fitted Prismatic model) replaces the current default only when paired duels started from a held game (PRISMATIC_START_PAUSED=2) on three seeds favour it beyond each pair's rival spread, and the pairs are recorded under research/results/. Enforced by review, not by a tool.
statement: Change a default of the bot or the planner on the strength of a measurement only when held-start duel pairs on three seeds favour it beyond the rival's spread, and record the pairs; report anything less as inconclusive.
status: active
class: advisory
depends_on: [project.never-report-unrun-validation-as-pass@1]
tags: [duel, experiment, a-b, planning, noise]
---

# Rationale

Before the held start two runs of one bot differed by up to 52%, and a +10.4% for own lines that set
`OWN_LINES` = 8 turned out to be noise: the held-start re-measure gave -1.0% over ten years
(`research/results/duel-20261117T234725-seed31337.md`, knowledge `duels-run-side-by-side-are-noisy`).
With the held start two runs differ by about 0.2-0.3%, and the calibrated plan, measured that way on
three seeds, averaged about zero (knowledge `the-planner-overpredicts-what-a-line-earns`). ADR 0034
makes decisions read recorded history; this rule keeps history from tuning on noise.

# Required behaviour

- Run the arms as pairs on the same seed, both with `PRISMATIC_START_PAUSED=2` (the panel's duel form
  sets it), on three seeds (4242, 31337 and 20208 are the ones measured so far), one or two duels at a
  time.
- Read each pair against its rival's spread: the same unplanned bot runs in both arms.
- Keep the records under `research/results/` and name them in the commit and in a knowledge note.
- The procedure is workflow `a-decision-reads-the-store`.

# Failure behaviour

Advisory: there is no validator, because a duel takes wall-clock minutes per game month and no gate
runs one. A reviewer reports a default changed on one seed, one unpaired run, a run without the held
start, or a difference inside the rival's spread, and the claim is reported as inconclusive (rule
`project.never-report-unrun-validation-as-pass`).
