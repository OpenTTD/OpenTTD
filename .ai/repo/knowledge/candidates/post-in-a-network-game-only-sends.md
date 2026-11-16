---
schema: knowledge/v1
id: post-in-a-network-game-only-sends
kind: knowledge
class: constraint
title: "In a network game Command<>::Post returns once the command is sent; it executes frames later, on every peer"
description: "Before reading game state right after a Post to decide whether a command worked (GUI, MCP, bots)."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [commands, network]
about: src/command.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/command.cpp
    - file:src/command_func.h
    - file:src/network/network_command.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/commands/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
---

# Statement

`src/command.cpp` sets `only_sending = _networking && !network_command` in `InternalPostBefore`,
and the helper returns after `NetworkSendCommand` (`src/network/network_command.cpp`). The callback
fires only on the issuing client, from `NetworkExecuteLocalCommandQueue`, when the frame arrives.
Follow-up logic belongs in a callback or in a later observation of state, not on the line after
`Post`. `src/prismatic_mcp/prismatic_mcp_server.cpp:ExecuteAction` reads `_pause_mode` immediately,
which is exact only in single player.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote post-in-a-network-game-only-sends` with evidence, or `majordomus knowledge reject post-in-a-network-game-only-sends --reason ...`.
