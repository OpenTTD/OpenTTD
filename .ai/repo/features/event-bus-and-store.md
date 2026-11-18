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
panel's table, so OpenAPI, Swagger UI and Rhai reach them. The kinds and payloads are in knowledge
`the-panel-bus-carries-these-event-kinds`.

## What it does not do

No decision reads the store yet: each planning decision publishes its choice as a structured
`plan.*` event beside its prose (I0081; the kinds are one table, `src/panel_events.rs`, listed by
`GET /api/bus`), and its `inputs.stored` names the fallback it took and the issue of milestone
`planning-is-generated-from-recorded-history` that adds its query. The hub's `audit.*` events and
the `game.*` events it derives for panel-less games reach neither the socket nor a store (I0084).
Prismatic and AlgoGlyph do not subscribe yet (I0085, I0086). Without libduckdb the panel runs with
"store: off".
