---
schema: knowledge/v1
id: mcp-approvals-survive-stop-rotation-and-a-new-world
kind: knowledge
class: fact
title: "MCP approvals outlive a server stop, a token rotation and a new world; only their tick TTL and use retire them"
description: "Before treating a token rotation or a restart of the MCP server as revoking what was approved."
status: superseded
epistemics: observed
date: 2026-11-16
tags: [prismatic-mcp]
about: src/prismatic_mcp/prismatic_mcp_server.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_server.cpp
    - file:src/misc.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
---

# Statement

`src/prismatic_mcp/prismatic_mcp_server.cpp:Stop` closes sockets and zeroes the token, and
`RotateToken` replaces the token; neither touches `_approvals`.
`src/prismatic_mcp/prismatic_mcp_server.cpp:OnGameModeChanged` is empty and has no caller. Expiry is
measured in game ticks, and `src/misc.cpp:InitializeGame` resets the tick counter, so an approval
from an earlier world can become current again in a new one (the last point is inferred).

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote mcp-approvals-survive-stop-rotation-and-a-new-world` with evidence, or `majordomus knowledge reject mcp-approvals-survive-stop-rotation-and-a-new-world --reason ...`.

# Rejected

Fixed by I0061: Stop, RotateToken and a world whose tick counter restarted (detail::WorldChanged) now clear every approval; the statement held at f069e8f0f5 and no longer does.
