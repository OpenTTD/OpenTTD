---
schema: knowledge/v1
id: the-majordomus-blackboard-is-written-by-announcing-on-a-board
kind: knowledge
class: fact
title: "The Majordomus blackboard has no write of its own: a process reaches it by announcing on a board as an MCP peer, and on this machine the mesh half is closed"
description: "Before forwarding anything (a bus digest, a status, a result) to the Majordomus blackboard so that other agent sessions see it."
status: candidate
epistemics: observed
date: 2026-11-18
tags: [panel, bus, majordomus, blackboard]
about: tools/research/mcp-rhai/src/bin/mcp-bus-blackboard.rs
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/src/bin/mcp-bus-blackboard.rs
    - file:research/mcp-control.md
relations:
  - type: relates_to
    target: file:.ai/repo/adrs/0033-every-panel-event-goes-out-on-zeromq-and-into-a-duckdb-store.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md
---

# Statement

Read in Majordomus 1.0.2 (source `prismatic-majordomus`, `apps/majordomus-cli/src/`), and checked
against the running server of this checkout on 2026-11-18:

- `capability/builtin/blackboard.rs` (ADR 0176) joins the boards (`peers.list`), the mesh journal
  and the monitor, and "writes nothing and keeps nothing". `blackboard.snapshot` has no write.
- The writes that reach it are `peers.announce` (`POST /api/v1/peers/announce`, or MCP
  `majordomus_announce`), which needs an MCP session; and the mesh journal: `POST
  /api/v1/mesh/sessions` (intent), `/mesh/beats` (phase), `/mesh/claims`, `/mesh/handovers`,
  `/mesh/reviews`. A session whose intent changes becomes a `session_changed` monitor signal
  (`monitor/mesh.rs`). A beat is suppressed as heartbeat noise.
- MCP over HTTP (`http/mcp.rs`) answers `POST /mcp` with JSON and an `Mcp-Session-Id`. It forgets
  a session after 90 s of silence, so a long-running client pings. The board keeps a departed
  peer's claims and drops the repeats of one client re-announcing the same named claims
  (`peers.rs:drop_repeated_departures`).
- On this machine `GET /api/v1/mesh` answers `active: false` ("this machine's key ... is not in
  trust.allow"), and the blackboard lists its `mesh` and `monitor.mesh` sources as `absent`. Only
  the board half carries a post today.

# How to apply it

Use `mcp-bus-blackboard` as the pattern. Open an MCP HTTP session on the primary checkout's server
(`majordomus serve ensure --format json`), announce named claims with an empty scope (nothing to
collide with), and ping inside 90 s. The mesh journal reaches other machines only once this
machine's key is enrolled (`majordomus mesh enrol`, committed). That is the owner's decision, not
a tool's.

# Review

Authored from the read and the live run of issue-less task t-20261118022636-0218; awaiting the
owner's review.
