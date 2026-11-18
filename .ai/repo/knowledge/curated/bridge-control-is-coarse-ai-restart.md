---
schema: knowledge/v1
id: bridge-control-is-coarse-ai-restart
kind: knowledge
class: fact
title: "rcon reaches only server console commands, so the bridge steers a company by restarting its AI with new settings"
description: "When designing a new control path from an external agent into a running game."
status: verified
epistemics: observed
date: 2026-07-24
tags: [bridge, admin-network, rcon, squirrel]
about: research/prismatic-bridge/ARCHITECTURE.md
route: documentation
provenance:
  origin: extracted
  derived_from:
    - commit:505fb87d7a441500b785e3b8a84f88b1152928df
    - commit:1e5ad23af931f4e5934e78eceb08f45d3c943e71
    - file:research/prismatic-bridge/ARCHITECTURE.md
    - issue:I0009
    - file:src/prismatic_mcp/prismatic_mcp_ai.cpp
    - file:tools/research/mcp-rhai/src/panel_decision.rs
relations:
  - type: documents
    target: file:.ai/repo/adrs/0007-prismatic-steers-a-company-through-a-standalone-bridge-that-.md
  - type: supports
    target: file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
  - type: relates_to
    target: file:.ai/repo/adrs/0034-planning-decisions-read-recorded-history-and-emit-structured.md
---

# Fact

- `AdminRemoteConsoleCommand` reaches `IConsoleCmdExec` only, never `DoCommand`. rcon is not a
  gameplay command channel.
- `start_ai` and `reload_ai` are console commands that take a settings string, which a Company
  AI reads with `AIController.GetSetting()`. That is the bridge's whole control path.
- The Squirrel sandbox has no socket or file I/O, so a script cannot open its own channel.
- A mid-run directive therefore needs an AI restart, which loses the script's in-progress state.
  The finer-grained routes are the admin network's Game Script channel (`ScriptAdmin.Send`) or
  the embedded MCP server (ADR 0008).

# Since 2026-11-17: a running AI's settings change without a restart, over MCP

The rcon path above is still what it was, and it is what a vanilla server offers. On a research build
with the MCP server, `openttd.ai.set_setting` (approval-gated, `src/prismatic_mcp/prismatic_mcp_ai.cpp:Validate`
and `Execute`) sets one setting of a running AI through the same `AIConfig` that `AIController.GetSetting()`
reads, and reads it back; only settings the AI declares with the in-game flag can change while it runs.
The panel applies a Prismatic directive with a `company_id` this way
(`tools/research/mcp-rhai/src/panel_decision.rs:pull`, through `panel_bot.rs:set_bot_setting`) instead of
`reload_ai`, which restarted the AI with its old settings and dropped the decided ones; the bridge does
the same since its main 1580b4b (`research/prismatic-bridge/ARCHITECTURE.md`, section 7). A directive mid-run
therefore no longer needs a restart on this fork's research build. History can reach the bot only this
way: as integer settings, because the Squirrel sandbox still has no I/O (ADR 0034).
