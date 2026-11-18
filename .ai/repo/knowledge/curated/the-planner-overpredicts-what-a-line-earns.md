---
schema: knowledge/v1
id: the-planner-overpredicts-what-a-line-earns
kind: knowledge
class: fact
title: "The planner predicts more profit per vehicle than the running lines earn"
description: "Before trusting a plan's monthly_profit or score, or when choosing params for a new plan; read the game's own calibration first."
status: verified
epistemics: observed
date: 2026-11-17
tags: [planner, calibration, back-propagation, economy]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/src/calibrate.rs
    - file:tools/research/mcp-rhai/src/plan.rs
    - file:tools/research/mcp-rhai/src/cargo.rs
    - file:research/mcp-control.md
    - file:tools/research/mcp-rhai/src/panel_bot.rs
    - issue:I0082
    - file:research/results/duel-20261117T183356-seed31337.md
    - file:research/results/duel-20261117T183400-seed31337.md
    - file:research/results/duel-20261117T231417-seed20208.md
    - file:research/results/duel-20261117T231419-seed20208.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/cargo-payment-and-vehicle-speed.md
---

# Fact

On the :8800 panel's game (one AI company, 17 years, 2026-11-17), `mcp-plan --calibrate` matched six
running lines to the planner's candidates and compared last year's profit per vehicle per month
with the prediction. Real over predicted: freight rail 0.97, 0.5 and 0.2 (median 0.5), passenger rail
0.2, bus 0.11 and 0.02 (median 0.11). Four more train lines matched no candidate within 12 tiles of
both stations. Every line runs a single vehicle, so the sample is small, and the planner's prediction
is for its own fleet size, divided per vehicle.

# How to apply it

- Read `GET /api/plan?calibrate=1` on a game with lines at least two years old and pass its
  `calibration.params` back as `params` before comparing modes or handing a plan to a bot.
- Treat bus and passenger-rail scores as optimistic until a calibration says otherwise; freight rail
  was the closest.
- Never calibrate only the measured modes: an unmeasured mode keeps factor 1 and wins every
  comparison (the :8800 portfolio turned into three air routes). The factors' `default` (median over
  every line) covers them.
- The readings live in the panel's store, not in a file (I0082): `POST /api/calibration` publishes
  each as `plan.calibration`, `GET /api/calibration` and a planned bot start with
  `PRISMATIC_PLAN_CALIBRATION=all` or `seed` read every store's readings back by query, and the
  plan's events name the query and its rows, or `fallback: uncalibrated` with the reason. The one
  reading of the retired file research/calibration.json (2026-11-17T16:55Z, the :8800 game) is the run
  `calibration-import` of its own store, with no seed or map: `seed` never finds it, `all` gives its
  factors (bus 0.16, freight_rail 0.59, rail 0.15, default 0.37).
- A calibrated plan did not pay yet. Seed 31337, 36 months, same code, run side by side
  (2026-11-17): calibrated 1023810 (research/results/duel-20261117T183356-seed31337.md, four
  investments, three freight lines) against 1050432 uncalibrated (duel-20261117T183400, five
  investments): 2.5% lower, about twice the spread of two runs of one bot. It led by 4.6% at month
  31. Calibration stays off by default; one reading of one game is too little to steer the plan.
- That A/B ran with a defect, found by an independent review (codex, 2026-11-17): the factor is a
  ratio of net profit, but the planner multiplied income by it and then subtracted the running cost,
  so a factor r charged an extra (1 − r) × running cost. Since the fix the factor scales income
  minus running cost (`plan.rs`, `cargo.rs`; test `calibration_scales_net_profit_never_income_alone`).
  The 2.5% therefore says nothing yet about a correct calibration; the next A/B needs several seeds
  and paired runs, not one pair.
- With the fix, paired on two seeds (2026-11-17, four duels at once): seed 31337 calibrated 794737 vs
  950946 (−16.4%), seed 20208 796631 vs 555180 (+43.5%). The same rival differed by 52% between the
  arms of one seed, so this is noise-bound (knowledge `duels-run-side-by-side-are-noisy`); calibration
  stays off by default.
- Again with the held start (`PRISMATIC_START_PAUSED=2`), a pair at a time, 2026-11-17: seed 4242
  554434 vs 569771 (−2.7%, rival 1.2% apart), seed 31337 905692 vs 941478 (−3.8%, rival 17% apart),
  seed 20208 654630 vs 612900 (+6.8%, rival bankrupt in both arms). Averaging about zero over three
  seeds: no case for calibration by default. The next step is more readings (games with several
  vehicles per line), not another A/B on one reading.
- Re-measure after a change to the payment or load model, and with more vehicles per line; record
  the new reading here rather than a second record.
