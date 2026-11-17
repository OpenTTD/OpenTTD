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
- Re-measure after a change to the payment or load model, and with more vehicles per line; record
  the new reading here rather than a second record.
