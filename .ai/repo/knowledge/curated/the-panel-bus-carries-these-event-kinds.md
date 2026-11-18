---
schema: knowledge/v1
id: the-panel-bus-carries-these-event-kinds
kind: knowledge
class: fact
title: "The panel's bus carries game, company, decision, console, news, panel and plan events from one declared table; the hub's audit events never reach it"
description: "Before subscribing to the panel's bus, its ZeroMQ socket or its DuckDB store (ADR 0033), or before a decision reads recorded history (ADR 0034): which event kinds exist (the table src/panel_events.rs declares, GET /api/bus kinds), who publishes each, what each carries, how often, what the plan.* events carry, and what is not on the bus at all."
status: verified
epistemics: observed
date: 2026-11-18
tags: [panel, events, bus, zeromq, duckdb, hub]
about: tools/research/mcp-rhai/src/panel.rs
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/src/panel_events.rs
    - file:tools/research/mcp-rhai/src/panel.rs
    - file:tools/research/mcp-rhai/src/hub.rs
    - file:tools/research/mcp-rhai/src/bin/mcp-hub.rs
    - file:tools/research/mcp-rhai/src/panel_bot.rs
    - file:tools/research/mcp-rhai/src/panel_results.rs
    - file:tools/research/mcp-rhai/src/panel_decision.rs
    - file:tools/research/mcp-duel
    - issue:I0080
    - issue:I0081
relations:
  - type: documents
    target: file:.ai/repo/adrs/0033-every-panel-event-goes-out-on-zeromq-and-into-a-duckdb-store.md
  - type: supports
    target: file:.ai/repo/adrs/0034-planning-decisions-read-recorded-history-and-emit-structured.md
  - type: relates_to
    target: file:.ai/repo/knowledge/candidates/console-lines-the-panel-parses-are-an-interface.md
---

# Fact

Every event goes through `tools/research/mcp-rhai/src/panel.rs:Bus::publish` as
`{seq, time, kind, data}`; the newest `BUS_KEEP` = 5000 are kept in memory, and `GET /api/events`
and `/api/events/stream` read them. ADR 0033's tap (`tools/research/mcp-rhai/src/panel_store.rs`) forwards exactly these
events to ZeroMQ (topic = kind) and to the store's `events` table.

## The declared kinds (I0081)

Every kind is a row of one table, `tools/research/mcp-rhai/src/panel_events.rs:KINDS`.
`GET /api/bus` answers it as `kinds`, and the region below is written from it
(`mcp-panel --write-event-kinds`). A cargo test of `mcp-panel` fails when a publish call in the
panel's sources names a kind outside the table, when a declared kind has no producer, or when this
region differs from the table: the list cannot drift from the code any more.

<!-- event-kinds:begin (generated from tools/research/mcp-rhai/src/panel_events.rs KINDS: mcp-panel --write-event-kinds) -->
| Kind | By | Producer | Data | When |
|---|---|---|---|---|
| `game.observed` | panel | `panel.rs:derive_game_events` | `{date, tick, vehicles, companies (count), towns}` | the first read after the MCP server is up |
| `company.observed` | panel | `panel.rs:derive_game_events` | the company object of `openttd://companies` | the first read, once per company |
| `game.new_world` | panel | `panel.rs:derive_game_events` | `{date, towns}` | the tick went back |
| `game.month` | panel | `panel.rs:derive_game_events` | `{date, vehicles, companies: {id: {money, value}}}` | the date's month changed |
| `game.paused` | panel | `panel.rs:derive_game_events` | `{date, tick}` | the paused flag went on |
| `game.resumed` | panel | `panel.rs:derive_game_events` | `{date, tick}` | the paused flag went off |
| `vehicles.count` | panel | `panel.rs:derive_game_events` | `{from, to, date}` | the fleet total changed |
| `company.founded` | panel | `panel.rs:derive_game_events` | the company object | a company id appeared |
| `company.removed` | panel | `panel.rs:derive_game_events` | the company object | a company id went |
| `company.renamed` | panel | `panel.rs:derive_game_events` | `{id, from, to}` | a company's name changed to a non-empty one |
| `decision.recorded` | panel | `panel.rs:derive_game_events` | the decision object of `openttd://decisions` | an MCP decision appeared |
| `decision.updated` | panel | `panel.rs:derive_game_events` | the decision object of `openttd://decisions` | an MCP decision changed |
| `game.sample` | panel | `panel.rs:record_sample` | `{t, date, vehicles, fleet, towns, paused, companies: [{id, name, is_ai, money, value, loan, net, fleet}]}` | once per new game day (fills the store's `samples`) |
| `ai.log` | panel | `panel.rs:read_log` | `{company, level, text}` | a console line `[script:N] [C] [L] text`, as the server prints it |
| `console.line` | panel | `panel.rs:read_log` | `{line}` | every other console line |
| `news.shown` | panel | `panel.rs:watch_news` | the news item object | each news item once, read every 2 s from the GUI client |
| `ask.answered` | panel | `panel.rs:ask_answers` | the question with its answer | once per answer |
| `panel.server` | panel | `panel.rs:Game::event` | `{text}` | the server starts, exits, reports its script budget |
| `panel.mcp` | panel | `panel.rs:Game::event` | `{text}` | the MCP server comes up; a tool call waits for approval |
| `panel.operator` | panel | `panel.rs:Game::event` | `{text}` | the operator approves, switches auto-approve, downloads content, is refused a console line |
| `panel.console` | panel | `panel.rs:Game::event` | `{text}` | a console line sent through `POST /api/console` |
| `panel.rhai` | panel | `panel.rs:Game::event` | `{text}` | a Rhai script ran |
| `panel.view` | panel | `panel.rs:Game::event` | `{text}` | a GUI client attached |
| `panel.ai` | panel | `panel.rs:Game::event` | `{text}` | the bot's `planner:` lines, its start, the held start, calibration, Prismatic's directive |
| `panel.plan` | panel | `panel.rs:Game::event` | `{text}` | an action plan or an investment was decided |
| `panel.intent` | panel | `panel.rs:Game::event` | `{text}` | a sentence was answered (`POST /api/intent`, `~ plan` in the console) |
| `panel.duel` | panel | `panel_results.rs` | `{text}` | a duel started from the panel; the README scoreboard was regenerated |
| `plan.portfolio` | panel | `panel_bot.rs:planned` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | the planner's portfolio for the bot was read (the bot start, `POST /api/bots/planned`) |
| `plan.bot_settings` | panel | `panel_bot.rs:planned` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | the bot's settings were chosen from that portfolio |
| `plan.calibration` | panel | `panel.rs:h_calibration_record` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a calibration reading was measured and recorded (`POST /api/calibration`) |
| `plan.investment` | panel | `panel.rs:h_investment` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | AlgoGlyph's transport-investment chose what to build (`GET /api/investment`) |
| `plan.action` | panel | `panel.rs:h_action_plan` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | AlgoGlyph's action-plan answered (`POST /api/actionplan`) |
| `plan.intent` | panel | `panel.rs:h_intent, answer_plan` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a sentence was resolved to an intent and candidates |
| `plan.directive` | panel | `panel_decision.rs:pull` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | Prismatic's directive was applied or refused (`POST /api/decision/pull`) |
| `plan.kernel` | external (`POST /api/bus/publish`) | `kernel_feed.rs:event (mcp-kernel-feed --publish)` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | an AlgoGlyph kernel answered a request fed from the store (I0086) |
<!-- event-kinds:end -->

Notes the table does not carry:

- **Game state is derived by difference.** `panel.rs:watch` reads `openttd://game/state`,
  `openttd://companies` and `openttd://decisions` once a second. A read that fails is skipped, so a
  change is seen late, never twice; a one-second read can miss a pause shorter than a second.
- **News present when the panel attached is not published**; `ask.answered` is published on each
  news poll and on each read of `GET /api/ask`.
- **`game.sample` is the one producer I0080 added** (ADR 0034 point 5): before it, samples lived
  only in the panel's own `history` (`HISTORY_MAX` = 8000, `GET /api/overview/history`); it is
  what fills the store's `samples`.
- **`panel.*` is prose.** `Game::event` publishes `{text}` and keeps the newest 200 for
  `GET /api/status`. The sentences stay for the page; consumers read the `plan.*` event of the same
  decision instead of parsing English (`tools/research/mcp-duel` still parses `planner:` lines from
  the capped 200 of `/api/status`; I0083 moves it to the store).

## Planning decisions as data (ADR 0034 point 3)

Each planning decision publishes one `plan.*` event beside its `panel.*` sentence, built by
`panel_events.rs:plan`:

- `decision`: the decision's name (`bot.portfolio`, `bot.settings`, `calibration.record`,
  `algoglyph.transport_investment`, `algoglyph.action_plan`, `intent.answer`,
  `prismatic.directive`, `algoglyph.kernel`);
- `version`: `{revision, params, digest}`: the git revision the binary was built from (`build.rs`,
  `-dirty` when the crate had uncommitted changes), the parameters the decision used and their
  FNV-1a 64 digest, so two runs with equal parameters carry equal digests;
- `inputs`: `{live, stored}`: `live` names what was read live (the planner's own `reads` list of
  `openttd://` resources, the console line money came from, the directive's URL ...), `stored` the
  store query with its row count, or `{fallback}`: why no history was read. On I0081 no panel
  decision reads the store yet; each fallback names the issue that adds its query (I0082
  calibration, I0085 Prismatic, I0086 kernels, I0087 the bot's settings), and the calibration ones
  name the file `research/calibration.json` they read instead;
- `choice`: what was decided, as data (the portfolio's lines, the settings in their slots with
  `rail_after`, `own_lines` and the lines dropped, the directive applied or refused with its errors,
  the kernel's answer).

`plan.portfolio` and `plan.bot_settings` come from every planned bot start and from
`POST /api/bots/planned` (`start: true` also starts the AI); `plan.bot_settings` names the
`plan.portfolio` it was chosen from (`inputs.live.portfolio_seq`) and the run's extra settings
(`choice.from_run`). `plan.kernel` is the one external kind: `mcp-kernel-feed --publish` posts it to
`POST /api/bus/publish`, which accepts only kinds the table marks external and only with the four
fields.

## Not on the bus

- **The hub's events.** `tools/research/mcp-rhai/src/hub.rs:fan_in` reads each panel's
  `/api/events` every 500 ms (`poll` in `tools/research/mcp-rhai/src/bin/mcp-hub.rs`) and relays
  them as `hub.event` over its WebSocket. For a game with an MCP server and no panel,
  `hub.rs:fan_in_mcp` derives `game.new_world`, `game.month` `{date}` and `game.paused` /
  `game.resumed` every `MCP_STATE_EVERY` = 10 cycles (5 s) with `hub.rs:derive_state_events`.
  `hub.rs:audit_watch_once` raises `audit.duplicate_station`, `audit.duplicate_depot`,
  `audit.over_capacity` and `audit.resolved` every 60 s (`--audit-every`). All of these go to hub
  subscribers only: no panel bus, no ZeroMQ socket and no store sees them.
- **A duel's verdict.** It exists as a `research/results/` file and `panel.duel` prose, not as an
  event. The calibration factors are in `plan.calibration`, but their store is still the file
  `research/calibration.json` (I0082).
- **The game itself.** Nothing on the bus is read from the game's own event stream; every game
  event is a difference between two MCP reads.

# How to apply it

- Subscribe by kind prefix (`game`, `company`, `plan`, `ai.log`) on ZeroMQ, or query `events` by
  `kind` in the store (`select kind, count(*) from events where kind like 'plan.%' group by 1`); do
  not parse `panel.*` text when a structured kind exists.
- A consumer that needs the hub's audit or panel-less games has to subscribe to the hub until they
  reach a bus or a store (milestone `planning-is-generated-from-recorded-history`).
- `GET /api/bus` `kinds` is the authority for a running panel; a panel built before I0081 has no
  `kinds` and no `plan.*` events. A new kind is a row of `KINDS` first (the cargo test refuses it
  otherwise), then `mcp-panel --write-event-kinds`.
- The hub's kinds (`hub.rs:derive_state_events`, the `audit.*` events) are not in the table: they are
  not on a panel's bus (I0084).
