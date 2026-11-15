---
id: project.mcp-surfaces-are-derived
version: 1
kind: rule
title: Every MCP surface is derived from one declaration, never listed by hand
description: REST routes, OpenAPI operations, Swagger UI, Rhai functions, event kinds and the bot's settings form come from the live MCP catalogue (tools/list, resources/list), the panel's own route table, the console's list_cmds and each AI's declared settings. A new tool, command or setting reaches every surface without a change to the others.
statement: Derive every surface of the MCP integration from the live catalogue and the one route table; do not hand-list tools, routes, functions or settings in a second place.
status: active
class: blocking
depends_on: [project.never-report-unrun-validation-as-pass@1]
tags: [mcp, openapi, rhai, derivation]
---

# Rationale

The owner's requirement is one declaration and many surfaces. A hand-written list goes stale the first
time the C++ side adds a tool, and a stale surface claims capability the game does not have.

# Required behaviour

- Panel routes: `make_routes()` in `tools/research/mcp-panel/server.py`, plus `derived_routes()` from the catalogue.
- Rhai: one function per tools/list entry, per `list_cmds` command and per panel OpenAPI operation, registered at start-up.
- Bot settings: `openttd.ai.list`, else the AI's `info.nut`.
- Proof against a running panel: `tools/research/mcp-panel-check` (every tool and resource has its route and schema, the routes answer, the event feed works).

# Failure behaviour

A surface that lists something by hand, or a failing `mcp-panel-check`, blocks the change. There is
no Majordomus validator for it yet; `mcp-panel-check` and review enforce it.
