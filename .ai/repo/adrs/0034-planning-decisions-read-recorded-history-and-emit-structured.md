---
schema: adr/v1
id: adr-0034
kind: adr
title: Planning decisions read recorded history from the store and publish each choice as a structured event
status: proposed
date: 2026-11-18
tags:
  - panel
  - events
  - data
  - planning
  - integration
related:
  - file:.ai/repo/adrs/0033-every-panel-event-goes-out-on-zeromq-and-into-a-duckdb-store.md
  - file:.ai/repo/adrs/0007-prismatic-steers-a-company-through-a-standalone-bridge-that-.md
  - file:.ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md
  - file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - file:.ai/repo/workflows/a-decision-reads-the-store.md
  - file:tools/research/mcp-rhai/src/panel_bot.rs
provenance:
  origin: authored
  derived_from:
    - issue:I0080
    - file:.ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md
    - file:research/mcp-control.md
---

# 34. Planning decisions read recorded history from the store and publish each choice as a structured event

## Context

ADR 0033 puts every panel event on a ZeroMQ PUB socket and into a DuckDB store per instance
(`runs`, `events`, `samples`), readable through `GET|POST /api/db/query`. It names "use at decision
time" as the purpose and leaves the decisions themselves unchanged. Today (read at e4eeae5dda) every
decision point reads one live snapshot or a hand constant:

- `tools/research/mcp-rhai/src/panel_bot.rs:settings_from_plan` chooses the bot's lines from one
  planner answer, with `MAX_FREIGHT_LINES` = 16 and `OWN_LINES` = 8 typed in, and `rail_after` from
  that answer's scores alone; `panel_bot.rs:planned_air` reads calibration from the file
  research/calibration.json (retired by I0082) when `PRISMATIC_PLAN_CALIBRATION` names it.
- The planner (`plan.rs:defaults`, `cargo.rs:portfolio`, `invest.rs:decide`, `linecost.rs:price`,
  `yards.rs:prove`, `sites.rs:site`, `intent.rs`) works on the current game only;
  `calibrate.rs:calibrate` reads one snapshot of vehicles at least 730 days old.
- `panel_decision.rs:pull` applies a directive Prismatic computed from one read of
  `/api/bridge/companies`; Prismatic's `OpenttdController` (`prismatic-platform`, read only) models
  growth with hand constants (`mean_growth = aggressiveness * 0.08`).
- AlgoGlyph's kernels (knapsack-dp, transport-investment, track-planning, action-plan,
  human-pointing) receive live snapshots per call (`npm run flow -- kernel`, or a long-lived
  `flow serve`, `tools/research/mcp-human/src/algoglyph.rs:Flow`); AlgoGlyph has no ZeroMQ or DuckDB
  code.
- `tools/research/mcp-duel` reads the plan from the 200 prose events of `/api/status` and the AI log
  from at most 5000 bus events.
- Each choice is published only as a `panel.ai` / `panel.plan` sentence (`{text}`), so no consumer
  can read what was decided, from which inputs, by which version.

Two measured facts bound what "tuning from history" can claim. Before the held start
(`PRISMATIC_START_PAUSED=2`, 2026-11-17), two runs of one bot differed by up to 52%; with it, by
about 0.2-0.3% (`research/results/duel-20261117T234725-seed31337.md`, knowledge
`duels-run-side-by-side-are-noisy`). An earlier +10.4% for own lines was noise under that test. And
the calibrated plan, the one history-driven change measured so far, averaged about zero over three
held-start seeds (-2.7%, -3.8%, +6.8%; knowledge `the-planner-overpredicts-what-a-line-earns`).

## Decision

1. **A decision reads history through the store's query API.** A decision point that uses recorded
   history reads it with a read-only statement on `/api/db/query` of its own instance (or, across
   instances, of each instance's store), never by scraping console lines or `panel.*` text, and
   never from a second copy (a JSON file next to the code). The statement is part of the decision's
   code and is named in the event it publishes (point 3).
2. **A stated fallback when the store is off.** ADR 0033 lets the panel run with "store: off". A
   decision that reads history declares what it does then: the current hand default, the live
   snapshot it uses today, or a refusal. It publishes which one it took. Silent degradation to a
   default is not allowed.
3. **Every decision point publishes one structured event next to its prose.** Kinds are `plan.*`
   for what the planner chooses (`plan.portfolio`, `plan.bot_settings`, `plan.calibration`,
   `plan.investment`, ...) and `decision.*` stays the MCP decision lifecycle it is today. The
   payload carries at least `{decision, version, inputs, choice}`: `inputs` names the live
   resources and the store query (with its row count, or `fallback`), `choice` is what was decided
   as data, `version` is the code revision and the parameter set. The `panel.*` sentence stays for
   the page; the structured event is what consumers read. The kinds are declared in one table in the
   code, from which the documentation is derived (rule `project.mcp-surfaces-are-derived`).
4. **AlgoGlyph and Prismatic read the bus or the store, never the console.** Prismatic subscribes to
   the ZeroMQ socket or queries the store to fit its model; AlgoGlyph's kernels receive trailing
   data assembled from the store by their caller (a `flow serve` subscriber, or the panel). Neither
   parses `ai.log`, `console.line` or `panel.*` text.
5. **`game.sample` is the one added producer of I0080.** ADR 0033 said no producer changes; the
   implementation of I0080 publishes the daily overview sample as `game.sample`, because the store's
   `samples` table has no other source. This ADR records that deviation; no further producer is
   added without a decision.
6. **History-driven tuning is accepted only on held-start pairs over three seeds.** A change to a
   default or a constant derived from history (calibration, `OWN_LINES`, `rail_after`, a fitted
   Prismatic model) is accepted when paired duels with the held start (`PRISMATIC_START_PAUSED=2`)
   on three seeds, each pair compared against its own rival's spread, favour it. One seed, one
   unpaired run, or a difference smaller than the rival's spread decides nothing. The result is a
   `research/results/` record and a knowledge note (workflow `a-decision-reads-the-store`).

## Alternatives rejected

- **Each consumer keeps its own history file** (as research/calibration.json did until I0082): a second copy
  per consumer, out of step with the store and invisible to the others.
- **Consumers parse the prose events**: the sentences change with every wording fix; ADR 0033 made
  the data available, and English is not a contract.
- **Prismatic and AlgoGlyph poll the panel's HTTP lists**: capped (200 status events, 5000 bus
  events), lost on a panel restart, and per caller; the socket and the store exist for this.
- **Tune from single runs**: measured to be noise up to 52% before the held start.

## Consequences

- Decision points gain a store query, a fallback and an event each; the work is planned in milestone
  `planning-is-generated-from-recorded-history` (issues I0081 to I0087), every one depending on I0080.
- `mcp-duel` and Prismatic read structured events and stored samples; a duel's verdict survives a
  panel restart.
- The hub's `audit.*` events and its panel-less `game.*` events have to reach a bus or a store before
  a decision can read them (I0084).
- A default derived from history can lose its A/B; the hand constant then stays, and the knowledge
  note records why.
- `research/prismatic-bridge/ARCHITECTURE.md` described Phase 3 as not started, and ADR 0007 still
  says so (its Consequences). Both are stale: `prismatic-platform` answers the decision contract at
  `GET /api/openttd/decision` and the panel applies its settings live through
  `openttd.ai.set_setting`. ADR 0007 is not edited; this ADR and the updated design record it.
