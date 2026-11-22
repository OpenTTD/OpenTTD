---
schema: knowledge/v1
id: the-panel-bus-carries-these-event-kinds
kind: knowledge
class: fact
title: "The panel's bus carries game, company, decision, console, news, panel and plan events from one declared table; the hub's own audit and derived game events go to the hub's bus and store"
description: "Before subscribing to a panel's or the hub's bus, its ZeroMQ socket or its DuckDB store (ADR 0033), or before a decision reads recorded history (ADR 0034): which event kinds exist (the table src/panel_events.rs declares, GET /api/bus kinds of each), who publishes each, what each carries, how often, what the plan.* events carry, where the hub's events are stored, and what is not on any bus at all."
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
    - file:tools/research/mcp-rhai/src/calibrate.rs
    - file:tools/research/mcp-rhai/src/bus_sinks.rs
    - issue:I0080
    - issue:I0081
    - issue:I0082
    - issue:I0083
    - issue:I0084
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

Every kind is a row of one table, `tools/research/mcp-rhai/src/panel_events.rs:KINDS`: the
panel's rows (by `panel` or `external`) and the hub's (by `hub`, I0084). A panel's `GET /api/bus`
answers the panel's rows as `kinds`, the hub's `GET /api/bus` the hub's rows, and the region below
is written from all of them (`mcp-panel --write-event-kinds`). A cargo test of `mcp-panel` fails
when a publish call in the panel's sources names a kind outside the table, when a declared kind has
no producer, or when this region differs from the table; one of `mcp-hub` fails when a kind the
hub's code raises (`derive_state_events`, `audit_findings`, `audit_changes`) is not one of the
hub's rows, or a hub row is raised by none: the list cannot drift from the code any more. A kind can
be the panel's and the hub's both (`game.month`), as two rows with their own producer and data.

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
| `panel.mesh` | panel | `panel.rs:Game::event` | `{text}` | a plan.directive or plan.directive_outcome was mirrored to the Majordomus mesh as a typed record, or not (src/panel_mesh.rs; only with $MAJORDOMUS_URL and $MAJORDOMUS_SESSION) |
| `panel.plan` | panel | `panel.rs:Game::event` | `{text}` | an action plan or an investment was decided |
| `panel.intent` | panel | `panel.rs:Game::event` | `{text}` | a sentence was answered (`POST /api/intent`, `~ plan` in the console) |
| `panel.duel` | panel | `panel_results.rs` | `{text}` | a duel started from the panel; the README scoreboard was regenerated |
| `rhai.shared` | panel | `rhai_shared.rs:h_shared_set, h_shared_delete` | `{op: set\|delete, key, rev, writer, value}` | a script, a service or the operator wrote or deleted a shared key |
| `plan.portfolio` | panel | `panel_bot.rs:planned` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | the planner's portfolio for the bot was read (the bot start, `POST /api/bots/planned`) |
| `plan.bot_settings` | panel | `panel_bot.rs:planned` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | the bot's settings were chosen from that portfolio |
| `plan.calibration` | panel | `panel.rs:h_calibration_record` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a calibration reading was measured (`POST /api/calibration`); the planned bot reads every stored one back (I0082) |
| `plan.investment` | panel | `panel.rs:h_investment` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | AlgoGlyph's transport-investment chose what to build (`GET /api/investment`) |
| `plan.action` | panel | `panel.rs:h_action_plan` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | AlgoGlyph's action-plan answered (`POST /api/actionplan`) |
| `plan.intent` | panel | `panel.rs:h_intent, answer_plan` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a sentence was resolved to an intent and candidates |
| `plan.directive` | panel | `panel_decision.rs:pull` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | Prismatic's directive was pulled: refused by the contract, or each step with its state (applied, pending approval, refused, error) (`POST /api/decision/pull`); `inputs.stored` is the fit it sent (its samples query or bus, rows, model, I0085) or the fallback |
| `plan.directive_outcome` | panel | `panel.rs:watch, panel_decision.rs:on_decision` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a directive that waited (plan.directive outcome pending) has the game's verdict for every step, or waited an hour without one (`expired`); `inputs.live.directive_seq` names its plan.directive |
| `plan.kernel` | external (`POST /api/bus/publish`) | `kernel_feed.rs:event (mcp-kernel-feed --publish)` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | an AlgoGlyph kernel answered a request fed from the store (I0086) |
| `plan.duel` | external (`POST /api/bus/publish`) | `tools/research/mcp-duel:publish_verdict` | `{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} \| {fallback}}, choice}` | a duel ended; `choice` is its verdict `{seed, months, held_start_tick, a, b, rival_spread, winner}`, read from the store (I0083) |
| `game.new_world` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{date}` | an MCP-only instance's tick went back |
| `game.month` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{date}` | an MCP-only instance's month changed |
| `game.paused` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{date}` | an MCP-only instance's paused flag went on |
| `game.resumed` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{date}` | an MCP-only instance's paused flag went off |
| `vehicles.count` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{from, to}` | an MCP-only instance's vehicle count changed |
| `companies.count` | hub (its own bus and store) | `hub.rs:derive_state_events` | `{from, to}` | an MCP-only instance's company count changed |
| `audit.duplicate_station` | hub (its own bus and store) | `hub.rs:audit_findings` | a pair of `hub.audit`'s `duplicates`: `{owner, facilities, a, b, distance}` | a duplicate station pair appeared |
| `audit.duplicate_depot` | hub (its own bus and store) | `hub.rs:audit_findings` | a pair of `hub.audit`'s `depots.duplicates`: `{owner, type, a, b, distance, by, reach}` | a duplicate depot pair appeared |
| `audit.over_capacity` | hub (its own bus and store) | `hub.rs:audit_findings` | a route of `hub.audit`'s `capacity.over`: `{a, b, cargo, vehicles, capacity, usage, needed, too_many}` | a route over capacity appeared (advisory) |
| `audit.resolved` | hub (its own bus and store) | `hub.rs:audit_changes` | `{was, key, finding}`: the finding's kind, its key and its data | a finding the last audit announced went away |
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
  decision instead of parsing English (`tools/research/mcp-duel` reads its run's `plan.*` events
  from the store since I0083, and keeps the `planner:` sentences, read from the store too, only for
  the reader of its record).

## Planning decisions as data (ADR 0034 point 3)

Each planning decision publishes one `plan.*` event beside its `panel.*` sentence, built by
`panel_events.rs:plan`:

- `decision`: the decision's name (`bot.portfolio`, `bot.settings`, `calibration.record`,
  `algoglyph.transport_investment`, `algoglyph.action_plan`, `intent.answer`,
  `prismatic.directive`, `algoglyph.kernel`, `duel.verdict`);
- `version`: `{revision, params, digest}`: the git revision the binary was built from (`build.rs`,
  `-dirty` when the crate had uncommitted changes), the parameters the decision used and their
  FNV-1a 64 digest, so two runs with equal parameters carry equal digests;
- `inputs`: `{live, stored}`: `live` names what was read live (the planner's own `reads` list of
  `openttd://` resources, the console line money came from, the directive's URL ...), `stored` the
  store query with its row count, or `{fallback}`: why no history was read. The calibration is the
  first decision that reads the store (I0082, below), and the bot's settings read it behind a switch
  (I0087, below). `plan.action` names the `samples` statement its cash and loan came from, with the
  rows, or the fallback after which the console filled them (I0086). `plan.directive`'s is what
  Prismatic fitted from (I0085): the samples query (or the bus) with its rows and the fit itself, or
  the fallback (no fit sent, Prismatic fell back, the fit breaks the contract). Every other fallback
  says why that decision reads no history;
- `choice`: what was decided, as data (the portfolio's lines, the settings in their slots with
  `rail_after`, `own_lines` and the lines dropped, the directive applied or refused with its errors,
  the kernel's answer).

`plan.portfolio` and `plan.bot_settings` come from every planned bot start and from
`POST /api/bots/planned` (`start: true` also starts the AI); `plan.bot_settings` names the
`plan.portfolio` it was chosen from (`inputs.live.portfolio_seq`) and the run's extra settings
(`choice.from_run`). `plan.kernel` and `plan.duel` are the external kinds: `mcp-kernel-feed
--publish` and `tools/research/mcp-duel` (once a duel's months are played) post them to
`POST /api/bus/publish`, which accepts only kinds the table marks external and only with the four
fields. A duel's `plan.duel` carries its verdict as `choice` (`{seed, months, held_start_tick, a, b,
rival_spread, winner}`) and in `inputs.stored` the store queries its plan, AI log and months came
from, with their row counts, or the fallback it took without a store (I0083).

**Calibration reads the store (I0082).** `plan.calibration` (`POST /api/calibration`) carries the
reading (`choice.reading.lines`, each with `mode` and `ratio`) and the game it came from
(`inputs.live.game`: `{seed, map: "WxH"}`); its `inputs.stored` is the query that read the earlier
readings, so `choice.factors` are those of every stored reading with this one. A planned bot start
with `PRISMATIC_PLAN_CALIBRATION` `all` or `seed` (the duel form's *Calibrated plan* sets `all`) reads
them back with `calibrate.rs:readings_sql` across every store of the store directory
(`panel_store.rs:query_every_store`: its own through the code of `/api/db/query`, a stopped panel's
file read-only, a running panel's through its `/api/db/query`), and `plan.portfolio` and
`plan.bot_settings` name that query with its rows (`inputs.stored`) and the readings used
(`inputs.live.calibration.readings`). Off (the default), with the store off, or with no reading, both
carry `{fallback: "uncalibrated: <why>"}` and the plan runs on the model alone. The one reading of
the retired file research/calibration.json was imported once as a run of its own
(`mcp-panel --import-calibration`, instance `calibration-import`, seed and map null).

**The bot's settings read the stored duels (I0087).** With `PRISMATIC_BOT_FROM_DUELS=1`, a planned
bot start reads every store's `plan.duel` verdicts of at least 60 months, with the `plan.bot_settings`
of the same instance (`duel_outcomes.rs:outcomes_sql`, across stores as above). Each verdict is joined
with the settings its own run published before it, and `own_lines` and `rail_after` are derived by the
rule `own-lines-by-handed-v1`. `plan.bot_settings` then names that query with its rows (`inputs.stored`)
and the rule, outcomes, pairs and reasons (`inputs.live.from_duels`), and its `version.params` gains
`from_duels`. Off (the default), with the store off, nothing handed over or no pair, it carries
`{fallback: "constants: <why>"}` and the parameters of before. The duels recorded before I0083 were
imported once as `plan.duel` events of a run of their own (`mcp-panel --import-duels`, instance
`duel-import`), each with what its run handed over in `choice.plan`.

## The hub's own events (I0084)

`tools/research/mcp-rhai/src/hub.rs:fan_in` reads each panel's `/api/events` every 500 ms (`poll`
in `tools/research/mcp-rhai/src/bin/mcp-hub.rs`) and relays them as `hub.event` over its
WebSocket; a relayed event is in its panel's store already and goes nowhere else. The events the
hub raises itself go through `hub.rs:State::raise`: to the WebSocket subscribers as before, and in
the same prismatic-bus/v1 envelope (`hub.rs:hub_envelope`, which is
`tools/research/mcp-rhai/src/bus_sinks.rs:envelope`) to the hub's own ZeroMQ PUB socket and DuckDB
store (`bus_sinks.rs:Sinks`, the code the panel's tap uses):

- for a game with an MCP server and no panel, `hub.rs:fan_in_mcp` derives `game.new_world`,
  `game.month` `{date}`, `game.paused` / `game.resumed`, `vehicles.count` and `companies.count`
  every `MCP_STATE_EVERY` = 10 cycles (5 s) with `hub.rs:derive_state_events`; their envelope
  carries the game clock of the read they were seen at;
- `hub.rs:audit_watch_once` raises `audit.duplicate_station`, `audit.duplicate_depot`,
  `audit.over_capacity` and `audit.resolved` every 60 s (`--audit-every`); their `game_tick` and
  `game_date` are null unless the hub reads that instance's state itself.

The envelope's `instance` is the instance the event is about, and so is the `instance` of its row in
the hub's `events` table; `seq` is the hub's own counter and `seed` is null (the hub does not know
it). The socket binds `tcp://127.0.0.1:<--bus-port, default ephemeral>`, the store is
`$PRISMATIC_STORE_DIR/hub-<port>.duckdb` (`--no-store` leaves it out), with a `runs` row naming the
hub and null ports. `GET /api/bus`, `GET /api/db/tables` and `GET /api/db/query` of the hub are rows
of its method table (`hub.bus`, `hub.db_tables`, `hub.db_query`), so they are in its OpenAPI,
GraphQL and Rhai (`hub_db_query(#{sql: ...})`). The hub's store is a file of the store directory, so
the calibration's `query_every_store` reads it with the panels' stores: it holds no
`plan.calibration`, and while the hub runs it is listed with an error, since the hub has no registry
record whose `/api/db/query` could answer for it.

## Not on any bus

- **The game itself.** Nothing on the bus is read from the game's own event stream; every game
  event is a difference between two MCP reads.

# How to apply it

- Subscribe by kind prefix (`game`, `company`, `plan`, `ai.log`) on ZeroMQ, or query `events` by
  `kind` in the store (`select kind, count(*) from events where kind like 'plan.%' group by 1`); do
  not parse `panel.*` text when a structured kind exists.
- A consumer that needs the hub's audit findings or a panel-less game's events subscribes to the
  hub's bus (`GET /api/bus` of the hub names the endpoint) or queries the hub's store
  (`select instance, kind, data from events where kind like 'audit.%'`); a panel's events are in
  that panel's store, never in the hub's.
- `GET /api/bus` `kinds` is the authority for a running panel; a panel built before I0081 has no
  `kinds` and no `plan.*` events. A new kind is a row of `KINDS` first (the cargo test refuses it
  otherwise), then `mcp-panel --write-event-kinds`.
- The hub's kinds are rows of the same table, by `hub`: a new one is a row first (the `mcp-hub`
  cargo test refuses it otherwise, and `State::raise` streams but does not publish an undeclared
  kind). They are never on a panel's bus: `POST /api/bus/publish` refuses them.
