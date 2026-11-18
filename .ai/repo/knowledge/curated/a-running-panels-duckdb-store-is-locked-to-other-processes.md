---
schema: knowledge/v1
id: a-running-panels-duckdb-store-is-locked-to-other-processes
kind: knowledge
class: lesson
title: "While a panel runs, no other process can open its DuckDB store, not even read-only"
description: "When a tool, agent or the duckdb CLI wants a panel's store (~/.local/state/prismatic-mcp/store/<instance>.duckdb) during a run."
status: verified
epistemics: observed
date: 2026-11-18
tags: [panel, store, duckdb, events]
provenance:
  origin: authored
  derived_from:
    - issue:I0080
    - file:.ai/repo/adrs/0033-every-panel-event-goes-out-on-zeromq-and-into-a-duckdb-store.md
relations:
  - type: documents
    target: file:research/mcp-control.md
---

# The lesson

- DuckDB lets a single process hold a database file open for writing, and that process holds the
  file's lock. The bus tap of a running panel (`tools/research/mcp-rhai/src/panel_store.rs:start`)
  keeps its store open for the whole run, so every other process is refused.
- `duckdb -readonly` is refused too. Observed on 2026-11-18 against a test stack on panel port 65045:
  `duckdb -readonly ~/.local/state/prismatic-mcp/store/panel-65045.duckdb` answered "Could not set
  lock ... Conflicting lock is held in .../mcp-panel (PID 57857)".
- During a run, read through the panel: `GET|POST /api/db/query`
  (`tools/research/mcp-rhai/src/panel_store.rs:run_query`), Rhai `api_dbQuery(#{sql: "..."})`,
  or the ZeroMQ bus (`GET /api/bus`, `mcp-bus-tail`). `/api/status` and `/api/bus` state the same
  in `store_access` (`tools/research/mcp-rhai/src/panel_store.rs:STORE_ACCESS`).
- Open the file directly (the duckdb CLI, a notebook, another process) only after the panel has
  stopped. On stop, `tools/research/mcp-rhai/src/panel_store.rs:stop` commits the last batch first.
- Do not work around this by giving a second process its own write connection. Two panels never
  share a file, because each instance writes `<instance>.duckdb`.
