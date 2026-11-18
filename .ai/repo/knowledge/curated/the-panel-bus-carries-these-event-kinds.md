---
schema: knowledge/v1
id: the-panel-bus-carries-these-event-kinds
kind: knowledge
class: fact
title: "The panel's bus carries game, company, decision, console, news and panel events; samples join it with I0080, and the hub's audit events never reach it"
description: "Before subscribing to the panel's bus, its ZeroMQ socket or its DuckDB store (ADR 0033), or before a decision reads recorded history (ADR 0034): which event kinds exist, who publishes each, what each carries, how often, and what is not on the bus at all."
status: verified
epistemics: observed
date: 2026-11-18
tags: [panel, events, bus, zeromq, duckdb, hub]
about: tools/research/mcp-rhai/src/panel.rs
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/src/panel.rs
    - file:tools/research/mcp-rhai/src/hub.rs
    - file:tools/research/mcp-rhai/src/bin/mcp-hub.rs
    - file:tools/research/mcp-rhai/src/panel_bot.rs
    - file:tools/research/mcp-rhai/src/panel_results.rs
    - file:tools/research/mcp-rhai/src/panel_decision.rs
    - file:tools/research/mcp-duel
    - issue:I0080
relations:
  - type: documents
    target: file:.ai/repo/adrs/0033-every-panel-event-goes-out-on-zeromq-and-into-a-duckdb-store.md
  - type: supports
    target: file:.ai/repo/adrs/0034-planning-decisions-read-recorded-history-and-emit-structured.md
  - type: relates_to
    target: file:.ai/repo/knowledge/candidates/console-lines-the-panel-parses-are-an-interface.md
---

# Fact

Read at e4eeae5dda (line numbers in parentheses are that revision's; cite the symbols). Every
event goes through `tools/research/mcp-rhai/src/panel.rs:Bus` (`publish`, line 284) as
`{seq, time, kind, data}`; the newest `BUS_KEEP` = 5000 are kept in memory, and `GET /api/events`
and `/api/events/stream` read them. ADR 0033's tap forwards exactly these events to ZeroMQ and to
the store; it adds no kind except `game.sample` (below).

## Game state, derived by difference

`panel.rs:watch` reads `openttd://game/state`, `openttd://companies` and `openttd://decisions`
once a second and publishes what `panel.rs:derive_game_events` (lines 1235-1282) finds between two
reads:

| Kind | Payload | When |
|---|---|---|
| `game.observed` | `{date, tick, vehicles, companies (count), towns}` | first read after the MCP server is up |
| `company.observed` | the company object of `openttd://companies` | first read, once per company |
| `game.new_world` | `{date, towns}` | the tick went back |
| `game.month` | `{date, vehicles, companies: {id: {money, value}}}` | the date's month changed |
| `game.paused` / `game.resumed` | `{date, tick}` | the paused flag changed |
| `vehicles.count` | `{from, to, date}` | the fleet total changed |
| `company.founded` / `company.removed` | the company object | a company id appeared or went |
| `company.renamed` | `{id, from, to}` | a company's name changed to a non-empty one |
| `decision.recorded` / `decision.updated` | the decision object of `openttd://decisions` | an MCP decision appeared or changed |

A read that fails is skipped, so a change is seen late, never twice; a one-second read can miss a
pause that lasted less than a second.

## The console and the GUI client

- `ai.log` `{company, level, text}`: every server console line that `panel.rs:ai_log` parses as
  `[script:N] [C] [L] text` (`panel.rs:read_log`, line 701). Every other console line is
  `console.line` `{line}` (line 702). Both arrive as the server prints them.
- `news.shown`: the news item object, each one once, read every 2 s from the GUI client
  (`panel.rs:watch_news`, line 1105). News present when the panel attached is not published.
- `ask.answered`: the question with its answer, once per answer (`panel.rs:ask_answers`, line
  1080), on each news poll and on each read of `GET /api/ask`.

## The panel's own actions

`panel.rs:Game::event` (lines 1055-1062) publishes `panel.<kind>` with `{text}` only, and keeps the
newest 200 for `GET /api/status`. The kinds in use: `panel.server`, `panel.mcp`, `panel.operator`,
`panel.console`, `panel.rhai`, `panel.view`, `panel.ai`, `panel.plan`, `panel.intent`,
`panel.duel`. `panel.ai` carries every `planner:` line of `panel_bot.rs:settings_from_plan` and
`panel_bot.rs:planned_air`, the bot start, the held start's pause and unpause ticks and Prismatic's
applied directive (`panel_decision.rs:pull`). **These are prose.** A decision's inputs, choice and
version exist only inside the sentence; a reader that wants them has to parse English, which is how
`tools/research/mcp-duel` gets the plan today (line 88, from the capped 200 of `/api/status`).

## Added by I0080

`game.sample`: the overview sample of `panel.rs:sample_of` (line 1176) `{t, date, vehicles, fleet,
towns, paused, companies: [{id, name, is_ai, money, value, loan, net, fleet}]}`, once per new game
day. On e4eeae5dda samples were kept only in the panel's own `history` (`HISTORY_MAX` = 8000, one
per day, `GET /api/overview/history`) and were not on the bus. Publishing them is the one producer
change ADR 0033 did not foresee ("no producer changes"); it is what fills the store's `samples`.

## Not on the bus

- **The hub's events.** `tools/research/mcp-rhai/src/hub.rs:fan_in` reads each panel's
  `/api/events` every 500 ms (`poll` in `tools/research/mcp-rhai/src/bin/mcp-hub.rs`) and relays
  them as `hub.event` over its WebSocket. For a game with an MCP server and no panel,
  `hub.rs:fan_in_mcp` derives `game.new_world`, `game.month` `{date}` and `game.paused` /
  `game.resumed` every `MCP_STATE_EVERY` = 10 cycles (5 s) with `hub.rs:derive_state_events`.
  `hub.rs:audit_watch_once` raises `audit.duplicate_station`, `audit.duplicate_depot`,
  `audit.over_capacity` and `audit.resolved` every 60 s (`--audit-every`). All of these go to hub
  subscribers only: no panel bus, no ZeroMQ socket and no store sees them.
- **Decision inputs and outputs as data.** The planner's portfolio, the calibration factors
  (`research/calibration.json`), the investment choice and a duel's verdict exist as files or prose,
  not as events.
- **The game itself.** Nothing on the bus is read from the game's own event stream; every game
  event is a difference between two MCP reads.

# How to apply it

- Subscribe by kind prefix (`game`, `company`, `ai.log`) on ZeroMQ, or query `events` by `kind` in
  the store; do not parse `panel.*` text when a structured kind exists.
- A consumer that needs the hub's audit or panel-less games has to subscribe to the hub until they
  reach a bus or a store (milestone `planning-is-generated-from-recorded-history`).
- Re-read `panel.rs:derive_game_events` and `hub.rs:derive_state_events` before relying on this
  table: the list is not declared in one place yet, so it drifts with the code.
