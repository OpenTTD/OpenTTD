---
schema: feature/v1
id: event-bus-and-store
kind: feature
title: Every panel event on a ZeroMQ socket and in a DuckDB store per instance
short_title: Event bus and store
headline: Any process on the machine follows a game live over ZeroMQ, and every decision can query what earlier games recorded.
summary: One tap on the panel's event bus forwards each event to a loopback ZeroMQ PUB socket (topic = kind, envelope prismatic-bus/v1) and to a DuckDB file per instance (runs, events, samples), read through /api/db/query, Rhai and the /data page; planning decisions publish their choices as plan.* events and are to read it.
status: draft
weight: 55
areas: [agent-integration, observability]
audiences: [prismatic-integrator, coding-agent]
rules: [project.mcp-surfaces-are-derived, project.a-tuning-claim-needs-held-start-pairs]
docs: [research/mcp-control.md]
adrs: [adr-0033, adr-0034]
related: [embedded-mcp-server, prismatic-bridge]
tags: [panel, events, zeromq, duckdb, planning]
---

## What it does

Being built in issue I0080 (ADR 0033): the bus's game, company, decision, console, news and panel
events, plus the daily `game.sample`, reach a ZeroMQ subscriber in bus order and the store within a
second; `GET /api/bus`, `GET /api/db/tables` and read-only `GET|POST /api/db/query` are routes of the
panel's table, so OpenAPI, Swagger UI and Rhai reach them. The hub publishes the events it raises
itself (its `audit.*` findings and the `game.*` it derives for a game without a panel) on a ZeroMQ
socket and into a DuckDB store of its own through the same code (`tools/research/mcp-rhai/src/bus_sinks.rs`, I0084;
`GET /api/bus` and `/api/db/query` of the hub); a panel event the hub relays stays in its panel's
store. The kinds and payloads are in knowledge `the-panel-bus-carries-these-event-kinds`.

## What it does not do

Only the calibration reads the store so far (I0082: the planned bot's `PRISMATIC_PLAN_CALIBRATION`
`all` or `seed` reads every store's `plan.calibration` readings by query, off by default). Every
planning decision publishes its choice as a structured `plan.*` event beside its prose (I0081; the
kinds are one table, `tools/research/mcp-rhai/src/panel_events.rs`, listed by `GET /api/bus`), and
each other decision's `inputs.stored` names the fallback it took and the issue of milestone
`planning-is-generated-from-recorded-history` that adds its query. No decision reads the hub's
store yet. The panel offers Prismatic each company's samples query and bus endpoint and records
the fit it reports (I0085); Prismatic's own fit and AlgoGlyph's subscriber are not built yet (I0086). Without libduckdb the panel runs with
"store: off".
