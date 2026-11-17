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
    - file:research/calibration.json
    - file:research/results/duel-20261117T183356-seed31337.md
    - file:research/results/duel-20261117T183400-seed31337.md
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
  comparison (the :8800 portfolio turned into three air routes). The store's `default` (median over
  every line) covers them (`research/calibration.json`, written by `mcp-plan --calibrate --record`).
- A calibrated plan did not pay yet. Seed 31337, 36 months, same code, run side by side
  (2026-11-17): calibrated 1023810 (research/results/duel-20261117T183356-seed31337.md, four
  investments, three freight lines) against 1050432 uncalibrated (duel-20261117T183400, five
  investments): 2.5% lower, about twice the spread of two runs of one bot. It led by 4.6% at month
  31. Calibration stays off by default; one reading of one game is too little to steer the plan.
- Re-measure after a change to the payment or load model, and with more vehicles per line; record
  the new reading here rather than a second record.
