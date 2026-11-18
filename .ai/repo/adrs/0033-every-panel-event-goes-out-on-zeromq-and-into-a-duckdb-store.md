---
schema: adr/v1
id: adr-0033
kind: adr
title: Every panel event goes out on a ZeroMQ PUB socket and into a DuckDB store, per instance, from one tap on the bus
status: accepted
date: 2026-11-18
tags:
  - panel
  - events
  - data
  - integration
related:
  - file:tools/research/mcp-rhai/src/panel.rs
  - file:research/mcp-control.md
  - file:.ai/repo/rules/project/mcp-surfaces-are-derived.v1.md
provenance:
  origin: authored
  derived_from:
    - issue:I0080
---

# 33. Every panel event goes out on a ZeroMQ PUB socket and into a DuckDB store, per instance, from one tap on the bus

## Context

The Rust panel (`tools/research/mcp-rhai/src/panel.rs`) already has one event bus (`Bus`): every
derived game event (game.month, company.*, vehicles.count, decision.*), every AI log line (ai.log),
every news and console line, and the panel's own events go through `Bus::publish`. Today the bus is
read over HTTP only (`GET /api/events`, the SSE stream `/api/events/stream`) by the page and by
checks, and it is held in memory and lost when the panel stops.

The owner asked (2026-11-18) for that stream to reach every consumer cleanly: Rust tools, the
blackboard and agents over ZeroMQ, and a store per session, instance and game in DuckDB that every
event updates. Every decision (the planner, the bot start, a duel's reading) must be able to use the
store, and it must be Rhai-friendly and exposed in the UI and API, all governed through `.ai/` and
Majordomus.

## Decision

1. **One tap, no producer changes.** A single subscriber of the panel's `Bus` forwards every event,
   in bus order, to two sinks. No code that publishes an event changes.
2. **ZeroMQ PUB.** The panel binds a PUB socket on `tcp://127.0.0.1:<port>`. The port comes from
   `PRISMATIC_BUS_PORT`, or is an ephemeral one the OS picks. The pure-Rust `zeromq` crate is used,
   because no system libzmq is assumed. Each message has two frames:
   - the topic, which is the event kind (`game.month`, `ai.log`, ...), so a subscriber filters by
     prefix;
   - an envelope, JSON of schema `prismatic-bus/v1`: `{schema, seq, ts_ms, instance, session, seed,
     game_tick, game_date, kind, data}`.

   The endpoint is published in the instance's registry record (`bus`) and by `GET /api/bus`, so
   discovery finds it and nobody hard-codes a port. Prismatic (Elixir), AlgoGlyph (Node) and Rust
   agents subscribe with any ZeroMQ SUB client.
3. **DuckDB store.** Each panel instance writes one DuckDB file:
   `$PRISMATIC_STORE_DIR/<instance>.duckdb`, by default `~/.local/state/prismatic-mcp/store/`. It
   uses the `duckdb` crate linked against the system libduckdb (Homebrew), not the bundled build, so
   no heavy C++ build is needed. Tables:
   - `runs(instance, session, seed, started_ms, panel_port, mcp_port, game_port)`: one row per panel
     start;
   - `events(instance, seq, ts_ms, kind, game_tick, game_date, data JSON)`: every bus event;
   - `samples(instance, ts_ms, game_tick, game_date, company, name, money, loan, value, net,
     vehicles)`: the overview history, one row per company per game day.

   Writes are batched and committed at least twice a second, so every event reaches the store within
   a second.
4. **Read surfaces, derived.** The routes are rows of the panel's route table, so they reach OpenAPI,
   Swagger UI and Rhai (`api_*`) with no hand-written binding:
   - `GET /api/bus`: endpoint, schema and the topics seen;
   - `GET /api/db/tables`: the schema;
   - `GET` or `POST /api/db/query` (`sql`): read-only statements only (SELECT, WITH, DESCRIBE,
     SUMMARIZE, SHOW). The answer is `{columns, rows, count}`, capped and stated when capped.

   A page `/data` in the panel (Flowbite, ECharts through the components.js door) runs a query, keeps
   it in the address, and charts numeric columns.
5. **Use at decision time.** A decision reads the store through the same query API: the bot start,
   the planner's calibration, a duel's reading. Moving existing calibration (the file research/calibration.json, retired by I0082)
   onto the store is follow-up work, recorded in I0080.

## Consequences

- A panel crash no longer loses its history: the store keeps it. Runs of a duel stay comparable after
  the fact (`runs` + `events` + `samples` by instance).
- Any process on the machine can follow a game live without HTTP polling. The PUB socket binds to
  loopback only.
- The panel gains two dependencies (`zeromq` with a small tokio runtime on its own thread, and
  `duckdb`). A machine without libduckdb cannot build the store. The store is a cargo feature
  (`store`, on by default here), and the panel runs without it, reporting "store: off" in
  `/api/status`.
- The event tap is the one place to add further sinks.
