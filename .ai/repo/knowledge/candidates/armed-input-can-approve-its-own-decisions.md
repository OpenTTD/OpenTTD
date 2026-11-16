---
schema: knowledge/v1
id: armed-input-can-approve-its-own-decisions
kind: knowledge
class: constraint
title: "An MCP controller with armed input can type into the in-game console, so it can approve its own pending decisions"
description: "Before relying on operator approval in a process started with PRISMATIC_MCP_INPUT=1."
status: candidate
epistemics: inferred
date: 2026-11-16
tags: [prismatic-mcp]
about: src/prismatic_mcp/prismatic_mcp_ui.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_ui.cpp
    - file:src/console_gui.cpp
    - file:src/console_cmds.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
  - type: relates_to
    target: file:.ai/repo/adrs/0014-interface-input-from-an-agent-is-armed-per-process-by-the-ow.md
  - type: relates_to
    target: file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
---

# Statement

Armed key input is fed to `src/window.cpp:HandleKeypress` as the local client
(`src/prismatic_mcp/prismatic_mcp_ui.cpp:AsDriver`). The console window executes the typed line
through `IConsoleCmdExec` (`src/console_gui.cpp`), and `src/console_cmds.cpp:ConPrismaticMcp`
accepts `prismatic_mcp approve <id>`. So the digest-bound approval of ADR 0008 does not bind a
controller whose process is armed: arming is the real boundary (ADR 0014).

Inferred from the source; not reproduced. Found by an independent model review and checked here.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote armed-input-can-approve-its-own-decisions` with evidence, or `majordomus knowledge reject armed-input-can-approve-its-own-decisions --reason ...`.
